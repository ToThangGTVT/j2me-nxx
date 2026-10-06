#!/bin/sh
# MIDlet test mạng. Trước khi chạy cần:
#   python3 echo_server.py &                      (echo TCP ở 127.0.0.1:5555)
#   mkdir -p www && echo hi > www/hello.txt && (cd www && python3 -m http.server 8765 --bind 127.0.0.1 &)
# Đóng gói: ./build.sh <classlib dir> <output.jar>
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$1" -Xlint:-options -nowarn -encoding UTF-8 -d "$OUT" $(find src -name '*.java')
jar cfm "$2" MANIFEST.MF -C "$OUT" .
rm -rf "$OUT"
