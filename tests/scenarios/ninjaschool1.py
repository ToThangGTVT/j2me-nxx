#!/usr/bin/env python3
"""Ninja School 1 (TeaMobi): chơi tự động bay nhảy liên tục trên bản desktop, đo khựng hình.

Bắt đầu từ save ngay sau đoạn mở đầu (ninjaschool1-save/), qua 2 hộp hỏi (âm thanh Bật, hình Cao),
Chơi tiếp, rồi chạy thẳng sang phải và nhảy đôi liên tục. Đầu mỗi chu kỳ 4s bấm OK (đóng hộp
"bị trọng thương" khi chết) rồi Quay lại (đóng Menu nếu lỡ mở).

    python3 tests/scenarios/ninjaschool1.py NinjaSchool1.jar [giây chơi, mặc định 300]

Log J2ME_NX_PROF ở <data>/play.log, ảnh chụp mỗi 10s ở <data>/shots/.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
START = 14000  # ms: đã vào màn chơi


def keys(play_ms):
    k = ["4000:-6", "6000:-6", "12000:-5"]
    t = START
    while t < START + play_ms:
        k += [f"{t}:-6", f"{t + 300}:-7", f"{t + 600}:-4:3350"]   # OK, Quay lại, giữ phải
        for j in range(4):
            a = t + 650 + j * 800
            k += [f"{a}:-1:250", f"{a + 350}:-1:200"]             # nhảy + nhảy lần 2 trên không
        t += 4000
    return ",".join(k)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    jar = os.path.abspath(sys.argv[1])
    play_ms = int(sys.argv[2]) * 1000 if len(sys.argv) > 2 else 300000
    data = tempfile.mkdtemp(prefix="ns1-")
    rms = os.path.join(data, "rms", os.path.splitext(os.path.basename(jar))[0])
    shutil.copytree(os.path.join(HERE, "ninjaschool1-save"), rms)
    shots = os.path.join(data, "shots")
    os.makedirs(shots)
    end = START + play_ms
    env = dict(os.environ,
               J2ME_NX_DATA=data,
               J2ME_NX_PROF="1",
               J2ME_NX_KEYS=keys(play_ms),
               J2ME_NX_SHOTS=",".join(f"{t}:{shots}/{t // 1000:04d}.bmp" for t in range(START + 2000, end, 10000)),
               J2ME_NX_QUIT=str(end + 1000))
    log = os.path.join(data, "play.log")
    with open(log, "w") as f:
        subprocess.run([os.path.join(ROOT, "build-desktop", "j2me-nxx"), jar], env=env, stdout=f, stderr=f)

    # Mỗi giây 1 dòng "fps .. vm max .. game gap max .. frame max ..": tìm giây có khung hình kéo dài
    worst = []
    for line in open(log, errors="replace"):
        m = re.search(r"\[\s*([\d.]+)\] fps (\d+).*vm max ([\d.]+) ms\s+game gap max (\d+) ms\s+frame max ([\d.]+) ms", line)
        if m and float(m.group(1)) * 1000 > START:
            worst.append((int(m.group(4)), line.strip()))
    worst.sort(reverse=True)
    print(f"log: {log}")
    print(f"{len(worst)} giây đo, khung chậm nhất:")
    for gap, line in worst[:5]:
        print("  " + line)
    stalls = [w for w in worst if w[0] >= 200]
    print(f"khựng >= 200ms: {len(stalls)} lần")
    return 1 if stalls else 0


if __name__ == "__main__":
    sys.exit(main())
