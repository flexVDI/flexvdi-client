#!/bin/bash

walk_libs() {
    ndk-depends "$1" | while read so_name; do
        local so_path="$LIB_DIR"/$so_name
        if [ ! -f "$so_path" ]; then
            case "$so_name" in
                libm.so|libc.so|libdl.so|liblog.so);;
                *) echo Missing $so_name;;
            esac
        elif [ ! -f libs/$so_name ]; then
            echo Copying $so_name
            cp "$so_path" libs
            walk_libs "$so_path"
        fi
    done
}

LIB_DIR="$1"/lib
if [ ! -d "$LIB_DIR" ]; then
    echo "Cannot find lib dir at $LIB_DIR"
    exit 1
fi
rm -fr libs
mkdir -p libs
cp "$LIB_DIR"/libflexvdi-client.so libs
walk_libs "$LIB_DIR"/libflexvdi-client.so

BUILD_TYPE="$2"
if [ "$BUILD_TYPE" != "Debug" ]; then
    for lib in libs/*; do llvm-strip --strip-all $lib; done
fi
