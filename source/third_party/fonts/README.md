# Font giao diện

`GoogleSans-Regular.ttf` là bản rút gọn của Google Sans (Google Fonts, giấy phép SIL OFL 1.1, xem `OFL.txt`).
Bản gốc: https://github.com/google/fonts/tree/main/ofl/googlesans

Tạo bằng fonttools: lấy nét Regular (`wght=400 GRAD=0 opsz=17`) từ font biến thiên, rồi chỉ giữ
Latin, Latin mở rộng, tiếng Việt, Hy Lạp, Cyrillic và dấu câu thông dụng:

```bash
fonttools varLib.instancer "GoogleSans[GRAD,opsz,wght].ttf" wght=400 GRAD=0 opsz=17 -o full.ttf
pyftsubset full.ttf --output-file=GoogleSans-Regular.ttf --layout-features='*' --name-IDs='*' --notdef-outline \
  --unicodes="U+0020-007E,U+00A0-024F,U+0300-0323,U+0370-03FF,U+0400-04FF,U+1E00-1EFF,U+2000-206F,U+20A0-20CF,U+2100-218F,U+2190-21FF,U+2212,U+25A0-25FF,U+FFFD"
```

"Google" và "Google Sans" là thương hiệu của Google LLC; dự án này không liên quan tới Google.
