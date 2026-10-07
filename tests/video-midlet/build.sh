#!/bin/sh
# MIDlet test video (MMAPI VideoControl): MIDlet-1 vẽ đè lên Canvas, MIDlet-2 trong Form
# res/clip.3gp: hình test + tiếng 440Hz, tạo bằng
#   ffmpeg -f lavfi -i testsrc=size=176x144:rate=12 -f lavfi -i sine=frequency=440:sample_rate=8000 \
#          -t 5 -c:v h263 -q:v 14 -c:a aac -ac 1 -ar 8000 -b:a 12k res/clip.3gp
# Đóng gói: ./build.sh <classlib dir> <output.jar>
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$1" -Xlint:-options -nowarn -encoding UTF-8 -d "$OUT" $(find src -name '*.java')
cp res/clip.3gp "$OUT/"
jar cfm "$2" MANIFEST.MF -C "$OUT" .
rm -rf "$OUT"
