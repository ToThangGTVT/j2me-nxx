#!/bin/sh
# Đóng gói MIDlet test âm thanh: ./build.sh <classlib dir> <output.jar>
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$1" -Xlint:-options -nowarn -encoding UTF-8 -d "$OUT" $(find src -name '*.java')
cp res/song.mid res/beep.wav res/tone.mp3 "$OUT/"
jar cfm "$2" res/MANIFEST.MF -C "$OUT" .
rm -rf "$OUT"
