#!/bin/sh
# MIDlet test M3G (JSR-184): MIDlet-1 khối lập phương, MIDlet-2 nạp res/scene.m3g
# (tạo lại scene.m3g: python3 make_m3g.py, ghi vào res/)
# Đóng gói: ./build.sh <classlib dir> <output.jar>
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$1" -Xlint:-options -nowarn -encoding UTF-8 -d "$OUT" $(find src -name '*.java')
cp res/scene.m3g "$OUT/"
jar cfm "$2" MANIFEST.MF -C "$OUT" .
rm -rf "$OUT"
