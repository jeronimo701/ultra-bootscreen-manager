#define main switch_app_main
#include "../source/app.c"
#undef main

static int failures;
static FILE *report;
static char run_root[160];
static void check(bool value,const char *name) {
    fprintf(report,"%s %s\n",value ? "PASS" : "FAIL",name);
    if (!value) failures++;
}
static void setup(const char *name) {
    snprintf(fixture,sizeof(fixture),"%s/%s",run_root,name);
    _mkdir("verification"); _mkdir("verification/native"); _mkdir(fixture);
    host_mkdir("sdmc:/switch"); host_mkdir(ROOT); host_mkdir(SCREENS); host_mkdir(BACKUPS);
    host_mkdir("sdmc:/bootloader"); host_mkdir("sdmc:/bootloader/res");
    locked=false; russian=true; pending=0; selected=0; tab=0;
    commit_calls=0; fail_commit_at=0; reboot_calls=0; reboot_error=0;
    fail_stage_open=false; fail_original_open=false; fail_stage_rename=false;
    snprintf(status,sizeof(status),"Select a bootscreen. X opens a full-screen preview.");
}
static void script(u64 *buttons,unsigned n) {
    sequence=buttons; sequence_count=n; sequence_index=0; switch_app_main(0,NULL);
}
static void screenshot(const char *path) {
    FILE *f=fopen(path,"wb");
    unsigned char h[54]={0};
    h[0]='B';h[1]='M';
    u32 size=54+1280*720*4,offset=54,dib=40,w=1280,hgt=720;
    memcpy(h+2,&size,4);memcpy(h+10,&offset,4);memcpy(h+14,&dib,4);
    memcpy(h+18,&w,4);memcpy(h+22,&hgt,4);h[26]=1;h[28]=32;
    fwrite(h,1,54,f);
    for(int y=719;y>=0;y--) for(int x=0;x<1280;x++) {
        u32 p=framebuffer.buffer[y*1280+x];
        u32 bgra=((p&255)<<16)|(p&0xff00)|((p>>16)&255)|0xff000000u;
        fwrite(&bgra,4,1,f);
    }
    fclose(f);
}
int main(void) {
    _mkdir("verification"); _mkdir("verification/native");
    snprintf(run_root,sizeof(run_root),"verification/native/run-%lld",(long long)time(NULL));
    _mkdir(run_root);
    report=fopen("verification/native/results.txt","wb");
    if (!report) return 1;

    setup("invalid");
    copy_new(assets[0],LIVE);
    FILE *invalid=fopen(SCREENS "/bad.bmp","wb"); fputs("not a bitmap",invalid); fclose(invalid);
    check(!install(SCREENS "/bad.bmp") && same(LIVE,assets[0]) && !exists(ORIGINAL),"Invalid BMP cannot change current screen or create a backup");

    setup("backup-failure"); copy_new(assets[0],LIVE); fail_original_open=true;
    check(!install(assets[1]) && same(LIVE,assets[0]) && !exists(STAGE),"Backup failure cancels installation");

    setup("stage-failure"); copy_new(assets[0],LIVE); fail_stage_open=true;
    check(!install(assets[1]) && same(LIVE,assets[0]) && same(ORIGINAL,assets[0]),"Staging failure preserves current screen and verified backup");

    setup("rename-failure"); copy_new(assets[0],LIVE); fail_stage_rename=true;
    check(!install(assets[1]) && same(LIVE,assets[0]) && !locked,"Failed final rename restores previous bootscreen");

    setup("install-restore"); copy_new(assets[0],LIVE);
    check(install(assets[1]) && same(LIVE,assets[1]) && same(ORIGINAL,assets[0]),"Successful install and immutable original backup");
    check(install(assets[2]) && same(LIVE,assets[2]) && same(ORIGINAL,assets[0]),"Second installation preserves first original");
    tab=1; load_entries();
    check(count==4 && !strcmp(entries[0].path,STOCK) && !strcmp(entries[1].name,"original.bmp"),"Backup gallery includes embedded UltraNX original, SD original and dated backups");
    check(install(ORIGINAL) && same(LIVE,assets[0]) && same(ORIGINAL,assets[0]),"Restore original also backs up the current screen");

    setup("embedded-original"); tab=1;load_entries();
    check(count==1 && !strcmp(entries[0].path,STOCK),"Embedded original remains available without SD backups");
    copy_new(assets[1],LIVE);
    check(install(STOCK) && same(LIVE,STOCK) && same(ORIGINAL,assets[1]),"Restore embedded UltraNX original byte-for-byte and preserve previous screen");

    setup("recovery"); copy_new(assets[0],ROLLBACK); recover();
    check(same(LIVE,assets[0]) && !exists(ROLLBACK) && !locked,"Interrupted installation restores missing live file");
    copy_new(assets[1],ROLLBACK); recover();
    check(locked && same(LIVE,assets[0]) && same(ROLLBACK,assets[1]),"Ambiguous interrupted installation preserves both files and locks writes");

    setup("custom-gallery"); copy_new(assets[1],SCREENS "/custom.BMP"); host_mkdir(SCREENS "/folder.bmp");
    tab=0; load_entries();
    check(count==4 && !strcmp(entries[3].name,"custom.BMP"),"Custom folder accepts uppercase BMP and ignores directories");
    for(int i=0;i<MAX_ENTRIES+10;i++) append("sample.bmp","sample.bmp",false);
    check(count==MAX_ENTRIES,"Gallery respects 512-entry bound");

    setup("language"); russian=false; save_language(); russian=true; load_language();
    check(!russian,"Language choice persists across sessions");
    russian=true;
    check(strcmp(translated_status(),status)!=0,"Status message is translated into Russian");
    const char *s="ЁяA";
    check(next_utf8(&s)==0x401 && next_utf8(&s)==0x44f && next_utf8(&s)=='A',"UTF-8 decoder handles Cyrillic including Yo");
    s="\xff";
    check(next_utf8(&s)=='?' && !*s,"Malformed UTF-8 is handled safely");

    setup("buttons-cancel"); copy_new(assets[0],LIVE);
    u64 cancel[]={HidNpadButton_R,HidNpadButton_R,HidNpadButton_Y,HidNpadButton_B,HidNpadButton_Down,HidNpadButton_A,HidNpadButton_B,HidNpadButton_Plus};
    script(cancel,sizeof(cancel)/sizeof(cancel[0]));
    check(!russian && tab==0 && selected==1 && reboot_calls==0 && same(LIVE,assets[0]),"Buttons switch language and tabs; cancel performs no installation or reboot");

    setup("buttons-stay"); copy_new(assets[0],LIVE);
    u64 stay[]={HidNpadButton_Down,HidNpadButton_A,HidNpadButton_A,HidNpadButton_B,HidNpadButton_Plus};
    script(stay,sizeof(stay)/sizeof(stay[0]));
    check(same(LIVE,assets[1]) && reboot_calls==0,"Successful install offers reboot; B stays without rebooting");

    setup("buttons-reboot"); copy_new(assets[0],LIVE);
    u64 restart[]={HidNpadButton_Down,HidNpadButton_A,HidNpadButton_A,HidNpadButton_A,HidNpadButton_Plus};
    script(restart,sizeof(restart)/sizeof(restart[0]));
    check(same(LIVE,assets[1]) && reboot_calls==1,"Only explicit confirmation invokes reboot service");

    setup("render");
    size_t length=0; font_data=read_all("romfs:/font.bin",&length);
    check(font_data && length==FONT_SIZE,"Cyrillic font atlas is loaded at expected size");
    framebufferCreate(&framebuffer,NULL,1280,720,1,2);
    tab=0;load_entries();russian=true; menu(); screenshot("verification/native/menu-ru.bmp");
    russian=false;menu();screenshot("verification/native/menu-en.bmp");
    russian=true;tab=2;menu();screenshot("verification/native/help-ru.bmp");
    tab=1;load_entries();menu();screenshot("verification/native/backups-ru.bmp");
    tab=0;pending=2;snprintf(pending_path,sizeof(pending_path),"romfs:/17e.bmp");snprintf(pending_name,sizeof(pending_name),"17e");
    menu();screenshot("verification/native/reboot-ru.bmp");
    framebufferClose(&framebuffer);free(font_data);
    fprintf(report,"%s: %d failures. Switch services mocked; no hardware test.\n",failures ? "FAIL" : "PASS",failures);
    fclose(report);return failures ? 1 : 0;
}
