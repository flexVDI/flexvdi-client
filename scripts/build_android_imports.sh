#!/bin/bash

walk_libs() {
    ndk-depends --print-paths -L "$LIB_DIR" -L "$LIB_DIR"/gstreamer-1.0 "$1" | \
            tr -d ' ' | cut -d '>' -f 2 | while read so_path; do
        local so_name=$(basename "$so_path")
        if echo $so_path | grep -q '^$/system'; then
            continue # System lib
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
walk_libs "$LIB_DIR"/libflexvdi-client.so
walk_libs "$LIB_DIR"/libgnutls.so

CERBERO_PLUGINS="coreelements coretracers adder app audioconvert audiomixer audiorate \
    audioresample audiotestsrc gio pango rawparse typefindfunctions videoconvert \
    videorate videoscale videotestsrc volume autodetect videofilter playback \
    deinterlace androidmedia opengl opensles"
for plugin in $CERBERO_PLUGINS; do
    walk_libs "$LIB_DIR"/gstreamer-1.0/libgst${plugin}.so
done

BUILD_TYPE="$2"
if [ "$BUILD_TYPE" != "Debug" ]; then
    for lib in libs/*; do llvm-strip --strip-all $lib; done
fi
