// Thuộc tính của MIDlet suite: MANIFEST.MF trong JAR và file .jad đi kèm
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *key, *value;
} ManifestProp;

typedef struct {
    ManifestProp *props;
    int count;
} Manifest;

// Định dạng "Key: Value"; override = true thì ghi đè giá trị đã có
void manifest_parse(Manifest *m, const char *text, bool override);
const char *manifest_get(const Manifest *m, const char *key);
void manifest_free(Manifest *m);

// Nạp JAD cạnh file JAR (nếu có) rồi MANIFEST.MF trong JAR. zip là ZipFile* đã mở.
struct ZipFile;
void manifest_load(Manifest *m, const char *jar_path, struct ZipFile *zip);

// Trường thứ field (0-based) của "MIDlet-<index>: Tên, /icon.png, lớp.Main", đã bỏ khoảng trắng
bool manifest_midlet_entry(const Manifest *m, int index, int field, char *out, size_t size);
// Như trên với MIDlet-1
bool manifest_midlet_field(const Manifest *m, int field, char *out, size_t size);
// Số MIDlet khai báo liên tiếp MIDlet-1, MIDlet-2, ...
int manifest_midlet_count(const Manifest *m);
