// Host-test ABI shim only. Production always includes the real OHOS SDK header.
#pragma once
#include <stdint.h>
typedef struct NativeChildProcess_Fd {
    char *fdName;
    int32_t fd;
    struct NativeChildProcess_Fd *next;
} NativeChildProcess_Fd;
typedef struct { NativeChildProcess_Fd *head; } NativeChildProcess_FdList;
typedef struct { char *entryParams; NativeChildProcess_FdList fdList; } NativeChildProcess_Args;
