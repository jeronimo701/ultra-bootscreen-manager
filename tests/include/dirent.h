#pragma once
#include <io.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct dirent { char d_name[260]; };
typedef struct { intptr_t handle; struct _finddata_t data; int first; struct dirent entry; } DIR;
static DIR *opendir(const char *path) {
    char pattern[1100]; snprintf(pattern,sizeof(pattern),"%s/*",path);
    DIR *d=calloc(1,sizeof(DIR)); if (!d) return NULL;
    d->handle=_findfirst(pattern,&d->data); d->first=1;
    if (d->handle==-1) { free(d);return NULL; } return d;
}
static struct dirent *readdir(DIR *d) {
    if (!d->first && _findnext(d->handle,&d->data)) return NULL;
    d->first=0; snprintf(d->entry.d_name,sizeof(d->entry.d_name),"%s",d->data.name); return &d->entry;
}
static int closedir(DIR *d) { int rc=_findclose(d->handle);free(d);return rc; }
