#include "storage.c"
#include <strings.h>

#define SCREENS ROOT "/bootscreens"
#define MAX_ENTRIES 512
#define PAGE_SIZE 10
#define FONT_W 16
#define FONT_H 26
#define FONT_SIZE (384 * FONT_W * FONT_H)

typedef struct { char path[768]; char name[256]; bool builtin; } Entry;
static Entry entries[MAX_ENTRIES];
static int count, selected, tab, pending;
static bool russian=true;
static char pending_path[768], pending_name[256];
static unsigned char *font_data;
static Framebuffer framebuffer;
static u32 *pixels, pixel_stride;
static Result reboot_error;
static const u32 WHITE=RGBA8_MAXALPHA(232,238,248),MUTED=RGBA8_MAXALPHA(150,166,192),BLUE=RGBA8_MAXALPHA(110,173,255);

static const char *tr(const char *ru,const char *en) { return russian ? ru : en; }
static const char *title_for(const char *path,const char *name) {
    if (!strcmp(path,STOCK)) return tr("Оригинальная заставка UltraNX","Original UltraNX bootscreen");
    if (!strcmp(path,"romfs:/15a.bmp")) return tr("15a - Пять героев",names[0]);
    if (!strcmp(path,"romfs:/17e.bmp")) return tr("17e - Синий слева / Красный справа",names[1]);
    if (!strcmp(path,"romfs:/04a.bmp")) return tr("04a - Охотник и дух / Суми-э",names[2]);
    return name;
}
static const char *translated_status(void) {
    static const char *pairs[][2]={
        {"Select a bootscreen. X opens a full-screen preview.","Выбери заставку. X: просмотр. A: установить."},
        {"Recovery failed. Keep bootscreen.ubm-rollback; check SD card.","Ошибка отката. Сохрани rollback и проверь SD на ПК."},
        {"Interrupted install: both files exist. Back them up on PC first.","Прерванная установка. Сохрани оба файла на ПК."},
        {"Cannot clear interrupted staging file. Check SD card.","Не удалось убрать временный файл. Проверь SD."},
        {"Invalid BMP; nothing changed.","Нужен BMP 720x1280, 32 bit, без сжатия. Файлы не изменены."},
        {"Backup failed; installation cancelled.","Бэкап не записан. Установка отменена."},
        {"Staging verification failed; current screen preserved.","Ошибка проверки записи. Текущая заставка сохранена."},
        {"Cannot move current screen; installation cancelled.","Не удалось переместить текущую заставку. Установка отменена."},
        {"SD commit failed. Exit and inspect backups on PC.","Ошибка записи SD. Проверь карту и бэкапы на ПК."},
        {"Installation failed; previous state restored.","Установка не удалась. Предыдущее состояние восстановлено."},
        {"Recovery failed. Keep rollback and backups; check SD on PC.","Ошибка отката! Сохрани rollback и бэкапы на ПК."},
        {"Installed, but rollback cleanup failed. Check SD on PC.","Установлено, но ошибка очистки. Проверь SD на ПК."},
        {"Installed and verified. Visible on the next boot.","Заставка установлена и проверена. Бэкап сохранён."},
        {"Invalid or damaged BMP. Preview unavailable.","Неверный или повреждённый BMP. Просмотр недоступен."},
        {"Reboot unavailable. Restart your Switch manually.","Перезагрузка недоступна. Перезагрузи Switch вручную."},
        {"Cannot create folders. Check free space on SD.","Не удалось создать папки. Проверь место на SD."},
        {"Showing first 512 files. Remove excess files from this folder.","Показаны первые 512 файлов. Убери лишние из папки."}
    };
    if (!russian) return status;
    for (unsigned i=0;i<sizeof(pairs)/sizeof(pairs[0]);i++)
        if (!strcmp(status,pairs[i][0])) return pairs[i][1];
    return status;
}
static void save_language(void) {
    const char *temp=ROOT "/language.tmp",*dest=ROOT "/language.txt";
    if (!mkdir_ok(ROOT)) return;
    FILE *f=fopen(temp,"wb"); if (!f) return;
    bool ok=fputs(russian ? "ru" : "en",f)>=0;
    if (fclose(f)) ok=false;
    if (!ok || !commit()) return;
    if (exists(dest) && remove(dest)) return;
    rename(temp,dest); commit();
}
static void load_language(void) {
    FILE *f=fopen(ROOT "/language.txt","rb"); if (!f) return;
    int a=fgetc(f),b=fgetc(f); russian=!(a=='e' && b=='n'); fclose(f);
}
static int by_name(const void *a,const void *b) { return strcasecmp(((const Entry *)a)->name,((const Entry *)b)->name); }
static int backups_first(const void *a,const void *b) {
    const Entry *x=a,*y=b;
    bool xo=!strcmp(x->name,"original.bmp"),yo=!strcmp(y->name,"original.bmp");
    if (xo!=yo) return xo ? -1 : 1;
    return -by_name(a,b);
}
static void append(const char *path,const char *name,bool builtin) {
    if (count>=MAX_ENTRIES) { snprintf(status,sizeof(status),"Showing first 512 files. Remove excess files from this folder."); return; }
    snprintf(entries[count].path,sizeof(entries[count].path),"%s",path);
    snprintf(entries[count].name,sizeof(entries[count].name),"%s",name);
    entries[count].builtin=builtin; count++;
}
static void scan(const char *folder) {
    DIR *dir=opendir(folder); if (!dir) return;
    struct dirent *e;
    while ((e=readdir(dir))) {
        size_t n=strlen(e->d_name);
        if (n<5 || strcasecmp(e->d_name+n-4,".bmp")) continue;
        char path[768]; int written=snprintf(path,sizeof(path),"%s/%s",folder,e->d_name);
        if (written<0 || (size_t)written>=sizeof(path)) continue;
        struct stat st;
        if (stat(path,&st)==0 && S_ISREG(st.st_mode)) append(path,e->d_name,false);
    }
    closedir(dir);
}
static void load_entries(void) {
    count=0; selected=0;
    if (tab==0) {
        for (int i=0;i<ASSET_COUNT;i++) append(assets[i],names[i],true);
        scan(SCREENS);
        if (count>ASSET_COUNT) qsort(entries+ASSET_COUNT,(size_t)(count-ASSET_COUNT),sizeof(Entry),by_name);
    } else if (tab==1) {
        append(STOCK,"Original UltraNX bootscreen",true);
        scan(BACKUPS);
        if (count>1) qsort(entries+1,(size_t)(count-1),sizeof(Entry),backups_first);
    }
}
static void box(int x,int y,int w,int h,u32 color) {
    for (int yy=y;yy<y+h && yy<720;yy++) for (int xx=x;xx<x+w && xx<1280;xx++)
        if (xx>=0 && yy>=0) pixels[(size_t)yy*pixel_stride+xx]=color;
}
static uint32_t next_utf8(const char **text) {
    const unsigned char *s=(const unsigned char *)*text;
    if (*s<128) { *text+=1; return *s; }
    uint32_t cp; int n;
    if ((*s&0xe0)==0xc0) { cp=*s&0x1f; n=2; }
    else if ((*s&0xf0)==0xe0) { cp=*s&0x0f; n=3; }
    else if ((*s&0xf8)==0xf0) { cp=*s&7; n=4; }
    else { *text+=1; return '?'; }
    for (int i=1;i<n;i++) {
        if (!s[i] || (s[i]&0xc0)!=0x80) { *text+=1; return '?'; }
        cp=(cp<<6)|(s[i]&0x3f);
    }
    *text+=n; return cp;
}
static void text(int x,int y,const char *s,u32 color,int maxchars) {
    for (int k=0;*s && k<maxchars;k++,x+=FONT_W) {
        uint32_t cp=next_utf8(&s);
        unsigned index=cp<128 ? cp : cp>=0x400 && cp<0x500 ? 128+cp-0x400 : '?';
        for (int yy=0;yy<FONT_H;yy++) for (int xx=0;xx<FONT_W;xx++) {
            if (x+xx<0 || x+xx>=1280 || y+yy<0 || y+yy>=720) continue;
            unsigned alpha=font_data[(index*FONT_H+yy)*FONT_W+xx]; if (!alpha) continue;
            u32 *dest=&pixels[(size_t)(y+yy)*pixel_stride+x+xx],old=*dest;
            unsigned r=((color&255)*alpha+(old&255)*(255-alpha))/255;
            unsigned g=(((color>>8)&255)*alpha+((old>>8)&255)*(255-alpha))/255;
            unsigned b=(((color>>16)&255)*alpha+((old>>16)&255)*(255-alpha))/255;
            *dest=RGBA8_MAXALPHA(r,g,b);
        }
    }
}
static void begin_frame(void) { u32 stride; pixels=framebufferBegin(&framebuffer,&stride); pixel_stride=stride/4; }
static void menu(void) {
    begin_frame(); box(0,0,1280,720,RGBA8_MAXALPHA(14,18,26));
    box(0,0,1280,83,RGBA8_MAXALPHA(25,32,46));
    text(32,12,"ULTRA BOOTSCREEN MANAGER 1.1.1",WHITE,75);
    text(32,45,tr("L/R: раздел   Y: RU/EN   +: выход","L/R: tabs   Y: RU/EN   +: exit"),MUTED,75);
    text(32,102,tab==0 ? tr("[ЗАСТАВКИ]   БЭКАПЫ   ИНСТРУКЦИЯ","[BOOTSCREENS]   BACKUPS   HELP") :
        tab==1 ? tr("ЗАСТАВКИ   [БЭКАПЫ]   ИНСТРУКЦИЯ","BOOTSCREENS   [BACKUPS]   HELP") :
        tr("ЗАСТАВКИ   БЭКАПЫ   [ИНСТРУКЦИЯ]","BOOTSCREENS   BACKUPS   [HELP]"),BLUE,75);
    if (tab==2) {
        const char *ru[]={
            "1. Положи NRO на SD в папку:","   /switch/UltraBootscreenManager/",
            "2. Свои заставки положи рядом с NRO в bootscreens:","   /switch/UltraBootscreenManager/bootscreens/",
            "3. Нужен BMP 720x1280, 32 bit, без сжатия.",
            "   Сделай его на ПК в Ultra Bootscreen Editor.",
            "4. Три встроенные картинки доступны без SD-файлов.",
            "5. A: установить. X: посмотреть. B: назад.",
            "6. Бэкапы: раздел L/R, X: просмотр, A: восстановить.",
            "   Оригинал UltraNX встроен; original.bmp - бэкап SD.",
            "   Перед каждой заменой создаётся отдельный бэкап.",
            "7. После установки A: перезагрузка, B: остаться.",
            "8. Y переключает RU/EN и сохраняет выбор.",
            "Файл заставки: /bootloader/res/bootscreen.bmp",
            "Бэкапы: /switch/UltraBootscreenManager/backups/",
            "Если ошибка SD: сохрани backups и rollback на ПК."
        };
        const char *en[]={
            "1. Place this NRO on your SD card in:","   /switch/UltraBootscreenManager/",
            "2. Add your BMPs to bootscreens next to this NRO:","   /switch/UltraBootscreenManager/bootscreens/",
            "3. Format: BMP 720x1280, 32 bit, uncompressed.",
            "   Convert on PC using Ultra Bootscreen Editor.",
            "4. Three built-in screens need no external images.",
            "5. A: install. X: preview. B: back.",
            "6. Backups tab: L/R, X: preview, A: restore.",
            "   UltraNX original is built-in; original.bmp is SD backup.",
            "   A separate backup is made before every change.",
            "7. After install: A to reboot, B to stay.",
            "8. Y switches RU/EN and saves your preference.",
            "Target: /bootloader/res/bootscreen.bmp",
            "Backups: /switch/UltraBootscreenManager/backups/",
            "SD error: keep backups and rollback safe on PC."
        };
        for (int i=0;i<16;i++) text(32,146+i*28,russian ? ru[i] : en[i],WHITE,76);
    } else {
        if (!count) text(32,170,tr("Здесь пока пусто. Смотри раздел Инструкция.","Empty folder. See the Help tab for instructions."),MUTED,76);
        int first=(selected/PAGE_SIZE)*PAGE_SIZE;
        for (int i=first;i<count && i<first+PAGE_SIZE;i++) {
            int y=154+(i-first)*34;
            if (i==selected) box(24,y-2,1232,32,RGBA8_MAXALPHA(35,65,110));
            char label[320];
            if (entries[i].builtin) snprintf(label,sizeof(label),"%s %s [%s]",i==selected ? ">" : " ",title_for(entries[i].path,entries[i].name),tr("встроено","built-in"));
            else if (!strcmp(entries[i].name,"original.bmp")) snprintf(label,sizeof(label),"%s original.bmp [%s]",i==selected ? ">" : " ",tr("первый оригинал","first original"));
            else snprintf(label,sizeof(label),"%s %s",i==selected ? ">" : " ",entries[i].name);
            text(32,y,label,WHITE,75);
        }
        char position[160]; snprintf(position,sizeof(position),"%d / %d    %s",count ? selected+1 : 0,count,tr("Крестовина: выбор   ZL/ZR: страница","D-pad: select   ZL/ZR: page"));
        text(32,514,position,MUTED,76);
        text(32,549,tab==0 ? tr("A: установить   X: посмотреть   B: назад","A: install   X: preview   B: back") : tr("A: восстановить   X: посмотреть   B: назад","A: restore   X: preview   B: back"),BLUE,76);
        text(32,586,tab==0 ? "/switch/UltraBootscreenManager/bootscreens/" : "/switch/UltraBootscreenManager/backups/",MUTED,76);
    }
    text(32,646,translated_status(),locked ? (u32)RGBA8_MAXALPHA(255,140,130) : MUTED,76);
    if (reboot_error) { char code[40]; snprintf(code,sizeof(code),"Result: 0x%08X",reboot_error); text(32,678,code,MUTED,76); }
    if (pending) {
        box(48,230,1184,242,RGBA8_MAXALPHA(34,43,63));
        text(74,250,pending==2 ? tr("Перезагрузить Switch сейчас?","Restart your Switch now?") :
            tab==1 ? tr("Восстановить выбранный бэкап?","Restore this backup?") : tr("Установить выбранную заставку?","Install this bootscreen?"),WHITE,71);
        text(74,294,title_for(pending_path,pending_name),BLUE,71);
        text(74,338,pending==2 ? tr("Заставка применится при следующей загрузке.","The bootscreen appears on the next boot.") :
            tr("Текущая заставка будет сохранена в backups.","The current screen will be saved in backups."),MUTED,71);
        text(74,408,pending==2 ? tr("A: перезагрузить   B: остаться   Y: RU/EN","A: reboot   B: stay   Y: RU/EN") : tr("A: подтвердить   B: отмена   Y: RU/EN","A: confirm   B: cancel   Y: RU/EN"),WHITE,71);
    }
    framebufferEnd(&framebuffer);
}
static void preview_file(PadState *pad,const char *path) {
    size_t n=0; unsigned char *p=read_all(path,&n);
    if (!p || !valid(p,n)) { free(p); snprintf(status,sizeof(status),"Invalid or damaged BMP. Preview unavailable."); return; }
    while (appletMainLoop()) {
        padUpdate(pad); u64 down=padGetButtonsDown(pad);
        if (down&(HidNpadButton_B|HidNpadButton_X|HidNpadButton_Plus)) break;
        if (down&HidNpadButton_Y) { russian=!russian; save_language(); }
        begin_frame(); const unsigned char *bmp=p+le32(p+10);
        for (int y=0;y<720;y++) for (int x=0;x<1280;x++) {
            const unsigned char *b=bmp+((size_t)x*720+y)*4;
            pixels[(size_t)y*pixel_stride+x]=RGBA8_MAXALPHA(b[2],b[1],b[0]);
        }
        framebufferEnd(&framebuffer);
    }
    free(p);
}
static void reboot(void) {
    if (!commit()) { snprintf(status,sizeof(status),"SD commit failed. Exit and inspect backups on PC."); return; }
    Result rc=bpcInitialize();
    if (R_SUCCEEDED(rc)) { rc=bpcRebootSystem(); bpcExit(); }
    reboot_error=rc;
    if (R_FAILED(rc)) snprintf(status,sizeof(status),"Reboot unavailable. Restart your Switch manually.");
}
int main(int argc,char **argv) {
    (void)argc; (void)argv;
    if (R_FAILED(romfsInit())) return 1;
    size_t n=0; font_data=read_all("romfs:/font.bin",&n);
    if (!font_data || n!=FONT_SIZE) { free(font_data); romfsExit(); return 1; }
    if (R_FAILED(framebufferCreate(&framebuffer,nwindowGetDefault(),1280,720,PIXEL_FORMAT_RGBA_8888,2))) { free(font_data); romfsExit(); return 1; }
    if (R_FAILED(framebufferMakeLinear(&framebuffer))) { framebufferClose(&framebuffer); free(font_data); romfsExit(); return 1; }
    load_language();
    if (!mkdir_ok(ROOT) || !mkdir_ok(SCREENS) || !mkdir_ok(BACKUPS)) snprintf(status,sizeof(status),"Cannot create folders. Check free space on SD.");
    recover(); load_entries();
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad); u64 down=padGetButtonsDown(&pad);
        if (down&HidNpadButton_Plus) break;
        if (down&HidNpadButton_Y) { russian=!russian; save_language(); }
        if (pending) {
            if (down&HidNpadButton_B) pending=0;
            else if (down&HidNpadButton_A) {
                if (pending==2) { pending=0; reboot(); }
                else {
                    pending=install(pending_path) ? 2 : 0;
                    load_entries();
                }
            }
        } else {
            if (down&(HidNpadButton_L|HidNpadButton_R)) {
                tab=(tab+((down&HidNpadButton_L) ? 2 : 1))%3; load_entries();
            } else if ((down&HidNpadButton_B) && tab!=0) { tab=0; load_entries(); }
            else if (tab!=2 && count) {
                if (down&(HidNpadButton_Up|HidNpadButton_Down)) selected=(selected+count+((down&HidNpadButton_Up) ? -1 : 1))%count;
                if (down&HidNpadButton_ZL) selected=selected>=PAGE_SIZE ? selected-PAGE_SIZE : 0;
                if (down&HidNpadButton_ZR) selected=selected+PAGE_SIZE<count ? selected+PAGE_SIZE : count-1;
                if (down&HidNpadButton_X) preview_file(&pad,entries[selected].path);
                if (!locked && (down&HidNpadButton_A)) {
                    snprintf(pending_path,sizeof(pending_path),"%s",entries[selected].path);
                    snprintf(pending_name,sizeof(pending_name),"%s",entries[selected].name); pending=1;
                }
            }
        }
        menu();
    }
    framebufferClose(&framebuffer); free(font_data); romfsExit(); return 0;
}
