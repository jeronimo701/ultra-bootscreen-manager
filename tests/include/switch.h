#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include <direct.h>
#include <io.h>
#include <time.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t Result;
typedef struct { u32 *buffer; } Framebuffer;
typedef struct { u64 buttons; } PadState;
#define R_SUCCEEDED(r) ((r)==0)
#define R_FAILED(r) ((r)!=0)
#define RGBA8_MAXALPHA(r,g,b) ((u32)(r)|((u32)(g)<<8)|((u32)(b)<<16)|0xff000000u)
#define PIXEL_FORMAT_RGBA_8888 1
#define HidNpadStyleSet_NpadStandard 1
#define HidNpadButton_A (1ull<<0)
#define HidNpadButton_B (1ull<<1)
#define HidNpadButton_X (1ull<<2)
#define HidNpadButton_Y (1ull<<3)
#define HidNpadButton_L (1ull<<4)
#define HidNpadButton_R (1ull<<5)
#define HidNpadButton_ZL (1ull<<6)
#define HidNpadButton_ZR (1ull<<7)
#define HidNpadButton_Up (1ull<<8)
#define HidNpadButton_Down (1ull<<9)
#define HidNpadButton_Plus (1ull<<10)

static char fixture[256]="verification/native/default";
static int commit_calls, fail_commit_at, reboot_calls;
static bool fail_stage_open, fail_original_open, fail_stage_rename;
static u64 *sequence;
static unsigned sequence_count, sequence_index;
static char *mapped(const char *path) {
    static char paths[8][1024]; static unsigned index;
    char *p=paths[(index++)%8];
    if (!strncmp(path,"sdmc:/",6)) snprintf(p,1024,"%s/%s",fixture,path+6);
    else if (!strncmp(path,"romfs:/",7)) snprintf(p,1024,"romfs/%s",path+7);
    else snprintf(p,1024,"%s",path);
    return p;
}
static FILE *host_fopen(const char *path,const char *mode) {
    if (strchr(mode,'w') && ((fail_stage_open && strstr(path,"ubm-stage")) || (fail_original_open && strstr(path,"original.bmp")))) return NULL;
    return fopen(mapped(path),mode);
}
static int host_rename(const char *a,const char *b) {
    if (fail_stage_rename && strstr(a,"ubm-stage")) { fail_stage_rename=false; return -1; }
    return rename(mapped(a),mapped(b));
}
static int host_remove(const char *p) { return remove(mapped(p)); }
static int host_access(const char *p,int mode) { return access(mapped(p),mode); }
static int host_mkdir(const char *p) { return mkdir(mapped(p)); }
static int host_stat(const char *p,struct stat *st) { return stat(mapped(p),st); }
static DIR *host_opendir(const char *p) { return opendir(mapped(p)); }
static struct tm *host_localtime_r(const time_t *t,struct tm *out) { *out=*localtime(t); return out; }
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
#endif
#define fopen(p,m) host_fopen(p,m)
#define rename(a,b) host_rename(a,b)
#define remove(p) host_remove(p)
#define access(p,m) host_access(p,m)
#define mkdir(p,m) host_mkdir(p)
#define stat(p,s) host_stat(p,s)
#define opendir(p) host_opendir(p)
#define localtime_r(t,s) host_localtime_r(t,s)

static Result fsdevCommitDevice(const char *name) { (void)name; return ++commit_calls==fail_commit_at ? 1 : 0; }
static Result romfsInit(void) { return 0; }
static void romfsExit(void) { }
static void *nwindowGetDefault(void) { return NULL; }
static Result framebufferCreate(Framebuffer *fb,void *w,int width,int height,int fmt,int n) {
    (void)w; (void)fmt; (void)n; fb->buffer=calloc(width*height,sizeof(u32)); return fb->buffer ? 0 : 1;
}
static Result framebufferMakeLinear(Framebuffer *fb) { (void)fb; return 0; }
static void framebufferClose(Framebuffer *fb) { free(fb->buffer); fb->buffer=NULL; }
static u32 *framebufferBegin(Framebuffer *fb,u32 *stride) { *stride=1280*4; return fb->buffer; }
static void framebufferEnd(Framebuffer *fb) { (void)fb; }
static bool appletMainLoop(void) { return sequence_index<sequence_count; }
static void padConfigureInput(int n,int styles) { (void)n; (void)styles; }
static void padInitializeDefault(PadState *pad) { pad->buttons=0; }
static void padUpdate(PadState *pad) { pad->buttons=sequence_index<sequence_count ? sequence[sequence_index++] : 0; }
static u64 padGetButtonsDown(PadState *pad) { return pad->buttons; }
static Result bpcInitialize(void) { return 0; }
static Result bpcRebootSystem(void) { reboot_calls++; return 0; }
static void bpcExit(void) { }
