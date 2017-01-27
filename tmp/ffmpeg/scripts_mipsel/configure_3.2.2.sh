#!/bin/bash

./configure \
--sysroot=$SYSROOT \
--cross-prefix=$CROSS_COMPILE \
--prefix=$PWD"/usr/" \
--enable-cross-compile \
--target-os=linux \
--arch=mipsel \
--disable-mipsdsp \
--disable-mipsdspr2 \
--disable-mipsfpu \
--enable-pic \
--enable-shared \
--disable-static \
--disable-debug \
--disable-ffplay \
--disable-ffprobe \
--disable-ffserver \
--disable-outdevs \
--disable-doc \
--disable-htmlpages \
--disable-manpages \
--disable-podpages \
--disable-txtpages \
--disable-lzma \
--enable-openssl \
--enable-zlib \
--enable-cross-compile \
--enable-small \
--disable-sdl \
--disable-xlib \
--disable-d3d11va \
--disable-dxva2 \
--disable-vaapi \
--disable-vda \
--disable-vdpau \
--enable-nonfree \
--extra-cflags=" $FFMPEG_CFLAGS " \
--extra-ldflags=" $FFMPEG_LDFLAGS "

#--disable-optimizations \
#--disable-mmx \
#--disable-stripping 

#--extra-cflags=" -g" \
#--enable-nonfree \
#--enable-bzlib \
#--enable-librtmp \

#libXv.so.1

