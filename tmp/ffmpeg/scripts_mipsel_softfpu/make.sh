#!/bin/bash

FFMPEG_PATH=$1
CONFIGURE_PATH=$2
SYSROOT=$3
FFMPEG_CFLAGS=$4
FFMPEG_LDFLAGS=$5
CFLAGS=$FFMPEG_CFLAGS
LDFLAGS=$FFMPEG_LDFLAGS

# patch hls
echo "HLS FIX"
sed -i '/for (i = 0; i < s->nb_streams; i++) {/c\for (i = 0; i < s->nb_streams && s->streams[i]->id < c->n_playlists; i++) {' $FFMPEG_PATH/libavformat/hls.c

cd $FFMPEG_PATH

mkdir -p usr/lib

source $CONFIGURE_PATH

make
make install
