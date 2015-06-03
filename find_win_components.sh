#!/bin/bash
ANCESTORS=${1:-2}
BASE=`pwd`
for i in `seq 1 $ANCESTORS`; do
    BASE="$BASE/.."
done
BASE=`readlink -f "$BASE"`

link_build_dir() {
    if file "$1" | grep -q x86-64; then
        echo "win64 ->" `dirname "$1"`
        ln -s `dirname "$1"` win64
    else
        echo "win32 ->" `dirname "$1"`
        ln -s `dirname "$1"` win32
    fi
}
link_vdagent() {
    if file "$1" | grep -q x86-64; then
        echo "win64/vdagent.exe -> $1"
        ln -s "$1" win64/vdagent.exe
    else
        echo "win32/vdagent.exe -> $1"
        ln -s "$1" win32/vdagent.exe
    fi
}
export -f link_build_dir link_vdagent

rm -f win32 win64
find "$BASE" -name flexvdi-guest-agent.exe -type f -exec bash -c 'link_build_dir "$@"' bash \{\} \;

rm -f win32/vdagent.exe win64/vdagent.exe
find "$BASE" -name vdagent.exe -type f -exec bash -c 'link_vdagent "$@"' bash \{\} \;

