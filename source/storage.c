#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LIVE "sdmc:/bootloader/res/bootscreen.bmp"
#define STAGE "sdmc:/bootloader/res/bootscreen.ubm-stage"
#define ROLLBACK "sdmc:/bootloader/res/bootscreen.ubm-rollback"
#define ROOT "sdmc:/switch/UltraBootscreenManager"
#define BACKUPS ROOT "/backups"
#define ORIGINAL BACKUPS "/original.bmp"
#define STOCK "romfs:/ultranx-original.bmp"
#define MAX_BMP (16 * 1024 * 1024)

static const char *assets[] = {"romfs:/15a.bmp", "romfs:/17e.bmp", "romfs:/04a.bmp"};
static const char *names[] = {"15a - Five hero lineup", "17e - Blue left / Red right", "04a - Hunter and spirit / Sumi-e"};
#define ASSET_COUNT ((int)(sizeof(assets)/sizeof(assets[0])))
static char status[256] = "Select a bootscreen. X opens a full-screen preview.";
static bool locked;

static bool exists(const char *p) { return access(p, F_OK) == 0; }
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static unsigned char *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END)) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 54 || n > MAX_BMP || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    unsigned char *data = malloc((size_t)n);
    if (!data) { fclose(f); return NULL; }
    bool ok = fread(data, 1, n, f) == (size_t)n && !ferror(f);
    if (fclose(f)) ok = false;
    if (!ok) { free(data); return NULL; }
    *size = (size_t)n;
    return data;
}
static bool valid(const unsigned char *p, size_t n) {
    if (n < 54 || p[0]!='B' || p[1]!='M' || le32(p+14)!=40 ||
        le32(p+18)!=720 || le32(p+22)!=1280 || p[26]!=1 || p[27]!=0 ||
        p[28]!=32 || p[29]!=0 || le32(p+30)!=0) return false;
    uint32_t offset = le32(p+10);
    return offset>=54 && offset<=n && n-offset>=720u*1280u*4u;
}
static bool same(const char *a, const char *b) {
    size_t na=0, nb=0;
    unsigned char *pa=read_all(a,&na), *pb=read_all(b,&nb);
    bool ok=pa && pb && na==nb && memcmp(pa,pb,na)==0;
    free(pa); free(pb);
    return ok;
}
static bool commit(void) { return R_SUCCEEDED(fsdevCommitDevice("sdmc")); }
static bool copy_new(const char *from, const char *to) {
    if (exists(to)) return false;
    size_t n=0;
    unsigned char *p=read_all(from,&n);
    if (!p) return false;
    FILE *f=fopen(to,"wb");
    if (!f) { free(p); return false; }
    bool ok=fwrite(p,1,n,f)==n;
    if (fflush(f)) ok=false;
    if (fclose(f)) ok=false;
    free(p);
    if (!commit()) ok=false;
    if (!ok || !same(from,to)) { remove(to); commit(); return false; }
    return true;
}
static bool mkdir_ok(const char *p) {
    if (mkdir(p,0777)==0) return true;
    struct stat st;
    return errno==EEXIST && stat(p,&st)==0 && S_ISDIR(st.st_mode);
}
static bool backup_current(void) {
    if (!mkdir_ok(ROOT) || !mkdir_ok(BACKUPS)) return false;
    if (!exists(LIVE)) return true;
    if (!exists(ORIGINAL) && !copy_new(LIVE,ORIGINAL)) return false;
    time_t now=time(NULL);
    struct tm t;
    localtime_r(&now,&t);
    char stamp[40],path[256];
    strftime(stamp,sizeof(stamp),"%Y%m%d-%H%M%S",&t);
    for (unsigned i=0;i<10000;i++) {
        snprintf(path,sizeof(path),BACKUPS "/previous-%s-%04u.bmp",stamp,i);
        if (!exists(path)) return copy_new(LIVE,path);
    }
    return false;
}
static void recover(void) {
    if (exists(ROLLBACK)) {
        if (!exists(LIVE)) {
            if (rename(ROLLBACK,LIVE) || !commit()) {
                locked=true;
                snprintf(status,sizeof(status),"Recovery failed. Keep bootscreen.ubm-rollback; check SD card.");
                return;
            }
        } else {
            locked=true;
            snprintf(status,sizeof(status),"Interrupted install: both files exist. Back them up on PC first.");
            return;
        }
    }
    if (exists(STAGE) && (remove(STAGE) || !commit())) {
        locked=true;
        snprintf(status,sizeof(status),"Cannot clear interrupted staging file. Check SD card.");
    }
}
static bool install(const char *asset) {
    if (locked) return false;
    size_t n=0;
    unsigned char *p=read_all(asset,&n);
    bool good=p && valid(p,n);
    free(p);
    if (!good) { snprintf(status,sizeof(status),"Invalid BMP; nothing changed."); return false; }
    if (!backup_current()) { snprintf(status,sizeof(status),"Backup failed; installation cancelled."); return false; }
    if (!copy_new(asset,STAGE)) { snprintf(status,sizeof(status),"Staging verification failed; current screen preserved."); return false; }
    bool had_current=exists(LIVE);
    if (had_current && rename(LIVE,ROLLBACK)) {
        remove(STAGE); commit();
        snprintf(status,sizeof(status),"Cannot move current screen; installation cancelled."); return false;
    }
    if (!commit()) {
        locked=true;
        snprintf(status,sizeof(status),"SD commit failed. Exit and inspect backups on PC."); return false;
    }
    if (rename(STAGE,LIVE) || !commit() || !same(asset,LIVE)) {
        bool recovered=true;
        if (exists(LIVE) && remove(LIVE)) recovered=false;
        if (had_current && rename(ROLLBACK,LIVE)) recovered=false;
        if (!commit()) recovered=false;
        locked=!recovered;
        snprintf(status,sizeof(status),recovered ? "Installation failed; previous state restored." : "Recovery failed. Keep rollback and backups; check SD on PC.");
        return false;
    }
    if (had_current && (remove(ROLLBACK) || !commit())) {
        locked=true;
        snprintf(status,sizeof(status),"Installed, but rollback cleanup failed. Check SD on PC."); return false;
    }
    snprintf(status,sizeof(status),"Installed and verified. Visible on the next boot.");
    return true;
}
