#!/bin/bash
ANCESTORS=${1:-2}
BASE=`pwd`
for i in `seq 1 $ANCESTORS`; do
    BASE="$BASE/.."
done
BASE=`readlink -f "$BASE"`

link_vdagent() {
    if file "$1" | grep -q x86-64; then
        ln -s `dirname "$1"` bin/vdagent_x64
    else
        ln -s `dirname "$1"` bin/vdagent_x86
    fi
}
export -f link_vdagent

cd spice-nsis
wget -nc http://nsis.sourceforge.net/mediawiki/images/c/c9/NSIS_Simple_Service_Plugin_1.30.zip
rm -f SimpleSC.dll
unzip NSIS_Simple_Service_Plugin_1.30.zip SimpleSC.dll
rm -fr bin
mkdir bin
find "$BASE" -name vdagent.exe -type f -exec bash -c 'link_vdagent "$@"' bash \{\} \;
rm -fr drivers
mkdir -p drivers/all
cd drivers
ln -s all virtio
ln -s all qxl
cd all
7z x ../../../virtio-win.iso
