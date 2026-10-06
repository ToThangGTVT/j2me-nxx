// Native cho j2menx.FileIO: thao tác file thật (đường dẫn đã được Java giới hạn trong sandbox)
#include "midp.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../vm/vm.h"

static bool get_path(VMThread *t, Object *s, char *out, size_t size) {
    if (!s) {
        throw_null(t);
        return false;
    }
    jstring_to_cstr(s, out, size);
    return true;
}

#define PATH_ARG(i, name)                           \
    char name[1024];                                \
    if (!get_path(t, args[i].l, name, sizeof(name))) \
        return NATIVE_EXCEPTION

static NativeResult F_exists0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    struct stat st;
    ret->i = stat(p, &st) == 0;
    return NATIVE_OK;
}

static NativeResult F_isDir0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    struct stat st;
    ret->i = stat(p, &st) == 0 && S_ISDIR(st.st_mode);
    return NATIVE_OK;
}

static NativeResult F_size0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    struct stat st;
    ret->j = stat(p, &st) == 0 ? (jlong)st.st_size : -1;
    return NATIVE_OK;
}

static NativeResult F_modified0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    struct stat st;
    ret->j = stat(p, &st) == 0 ? (jlong)st.st_mtime * 1000 : 0;
    return NATIVE_OK;
}

static NativeResult F_list0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    DIR *d = opendir(p);
    char **names = NULL;
    int count = 0, cap = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            char full[1300];
            snprintf(full, sizeof(full), "%s/%s", p, e->d_name);
            struct stat st;
            bool dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
            if (count == cap) {
                cap = cap ? cap * 2 : 32;
                names = realloc(names, sizeof(char *) * cap);
            }
            size_t len = strlen(e->d_name);
            names[count] = malloc(len + 2);
            memcpy(names[count], e->d_name, len);
            names[count][len] = dir ? '/' : '\0';
            names[count][len + 1] = '\0';
            count++;
        }
        closedir(d);
    }
    Class *sc = class_load(t, "java/lang/String");
    Object *arr = sc ? heap_alloc_array(t, class_array_of(t, sc), count) : NULL;
    for (int i = 0; i < count; i++) {
        if (arr)
            ARRAY_DATA(arr, Object *)[i] = jstring_new_utf8(t, names[i]);
        free(names[i]);
    }
    free(names);
    if (!arr)
        return NATIVE_EXCEPTION;
    ret->l = arr;
    return NATIVE_OK;
}

static NativeResult F_mkdir0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    ret->i = mkdir(p, 0777) == 0;
    return NATIVE_OK;
}

static NativeResult F_mkdirs0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    for (char *s = p + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            mkdir(p, 0777);
            *s = '/';
        }
    }
    mkdir(p, 0777);
    struct stat st;
    ret->i = stat(p, &st) == 0 && S_ISDIR(st.st_mode);
    return NATIVE_OK;
}

static NativeResult F_create0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    FILE *f = fopen(p, "ab");
    ret->i = f != NULL;
    if (f)
        fclose(f);
    return NATIVE_OK;
}

static NativeResult F_delete0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    struct stat st;
    if (stat(p, &st) == 0 && S_ISDIR(st.st_mode))
        ret->i = rmdir(p) == 0;
    else
        ret->i = remove(p) == 0;
    return NATIVE_OK;
}

static NativeResult F_rename0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, from);
    PATH_ARG(1, to);
    ret->i = rename(from, to) == 0;
    return NATIVE_OK;
}

static NativeResult F_read0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    FILE *f = fopen(p, "rb");
    if (!f) {
        ret->l = NULL;
        return NATIVE_OK;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 64L * 1024 * 1024) {
        fclose(f);
        throw_new(t, "java/io/IOException", "File qua lon");
        return NATIVE_EXCEPTION;
    }
    Object *arr = heap_new_prim_array(t, 'B', (jint)n);
    if (arr && n > 0 && fread(ARRAY_DATA(arr, uint8_t), 1, (size_t)n, f) != (size_t)n)
        arr = NULL;
    fclose(f);
    ret->l = arr;
    return arr || t->exception == NULL ? NATIVE_OK : NATIVE_EXCEPTION;
}

static NativeResult F_write0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    Object *data = args[1].l;
    jint len = args[2].i;
    jlong off = args[3].j;
    bool truncate = args[5].i != 0;
    if (!data || len < 0 || len > ARRAY_LEN(data) || off < 0) {
        ret->i = 0;
        return NATIVE_OK;
    }
    FILE *f = fopen(p, truncate ? "wb" : "r+b");
    if (!f)
        f = fopen(p, "wb");
    if (!f) {
        ret->i = 0;
        return NATIVE_OK;
    }
    bool ok = truncate || fseek(f, (long)off, SEEK_SET) == 0;
    ok = ok && fwrite(ARRAY_DATA(data, uint8_t), 1, (size_t)len, f) == (size_t)len;
    ok = fclose(f) == 0 && ok;
    ret->i = ok;
    return NATIVE_OK;
}

static NativeResult F_truncate0(VMThread *t, Value *args, Value *ret) {
    PATH_ARG(0, p);
    ret->i = truncate(p, (off_t)args[1].j) == 0;
    return NATIVE_OK;
}

void midp_fileio_register(void) {
    const char *F = "j2menx/FileIO";
    native_register(F, "exists0", "(Ljava/lang/String;)Z", F_exists0);
    native_register(F, "isDir0", "(Ljava/lang/String;)Z", F_isDir0);
    native_register(F, "size0", "(Ljava/lang/String;)J", F_size0);
    native_register(F, "modified0", "(Ljava/lang/String;)J", F_modified0);
    native_register(F, "list0", "(Ljava/lang/String;)[Ljava/lang/String;", F_list0);
    native_register(F, "mkdir0", "(Ljava/lang/String;)Z", F_mkdir0);
    native_register(F, "mkdirs0", "(Ljava/lang/String;)Z", F_mkdirs0);
    native_register(F, "create0", "(Ljava/lang/String;)Z", F_create0);
    native_register(F, "delete0", "(Ljava/lang/String;)Z", F_delete0);
    native_register(F, "rename0", "(Ljava/lang/String;Ljava/lang/String;)Z", F_rename0);
    native_register(F, "read0", "(Ljava/lang/String;)[B", F_read0);
    native_register(F, "write0", "(Ljava/lang/String;[BIJZ)Z", F_write0);
    native_register(F, "truncate0", "(Ljava/lang/String;J)Z", F_truncate0);
}
