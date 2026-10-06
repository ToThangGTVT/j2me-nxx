#!/bin/sh
# MIDlet test HTTPS / ssl:// (kết nối example.com, cần Internet)
# Đóng gói: ./build.sh <classlib dir> <output.jar>
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$1" -Xlint:-options -nowarn -encoding UTF-8 -d "$OUT" $(find src -name '*.java')
jar cfm "$2" MANIFEST.MF -C "$OUT" .
rm -rf "$OUT"
