// Backup save game: mỗi backup là 1 file zip trong <data>/backups/<id ứng dụng>/<thời gian>.zip,
// gồm file game (.jar / .jad), save RMS, file của ứng dụng (FileConnection), tuỳ chọn riêng và backup.ini.
// Đường dẫn trong zip giống trong thư mục j2me-nxx trên thẻ nhớ: games/..., rms/<id>/..., files/<id>/...,
// options/<id>.ini
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char file[48];          // "20261014-153000.zip"
    char when[24];          // "2026-10-14 15:30:00"
    long size;              // byte
} BackupInfo;

typedef struct {
    BackupInfo *items;      // mới nhất trước
    int count;
} BackupList;

// Thông tin trong backup.ini của file zip
typedef struct {
    char id[192];
    char title[128];
    char game[256];         // file game tương đối trong thư mục games ("RPG/abc.jar"), "" nếu không có
    char created[24];       // "2026-10-14 15:30:00"
} BackupMeta;

// id dùng được làm tên thư mục (không có / \ .. ký tự điều khiển)
bool backup_valid_id(const char *id);
// Đường dẫn file backup; false nếu id / tên file không hợp lệ
bool backup_file_path(const char *id, const char *file, char *out, size_t size);

void backup_list(const char *id, BackupList *out);
void backup_list_free(BackupList *list);

// Các id có ít nhất 1 backup (mảng malloc, giải phóng bằng backup_ids_free)
int backup_ids(char ***out);
void backup_ids_free(char **ids, int count);

bool backup_read_meta(const char *zip_path, BackupMeta *out);

// Ứng dụng có save / file / tuỳ chọn riêng để backup không
bool backup_has_data(const char *id);

// Tạo backup mới. game: file game tương đối trong thư mục games (NULL / "" = không kèm file game).
// file_out: tên file backup vừa tạo
bool backup_create(const char *id, const char *game, const char *title, char *file_out, size_t file_size,
                   char *err, size_t err_size);
bool backup_delete(const char *id, const char *file);

// Áp dụng backup: thay save, file, tuỳ chọn riêng của ứng dụng bằng bản trong backup;
// chép file game vào thư mục games nếu chưa có
bool backup_apply(const char *id, const char *file, char *err, size_t err_size);

// Nhận file zip (điện thoại gửi lên) vào thư mục backup của ứng dụng ghi trong backup.ini.
// zip_path được chuyển đi (hoặc xoá nếu trùng). id_out / file_out: nơi lưu
bool backup_import(const char *zip_path, char *id_out, size_t id_size, char *file_out, size_t file_size,
                   char *err, size_t err_size);
