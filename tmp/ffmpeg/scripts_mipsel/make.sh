#!/bin/bash

FFMPEG_PATH=$1
CONFIGURE_PATH=$2
SYSROOT=$3
FFMPEG_CFLAGS=$4
FFMPEG_LDFLAGS=$5
CFLAGS=$FFMPEG_CFLAGS
LDFLAGS=$FFMPEG_LDFLAGS

# patch hls
echo "################################################"
echo "#             Applay FFMPEG patches            #"
echo "################################################"
echo "HLS FIX"
sed -i '/for (i = 0; i < s->nb_streams; i++) {/c\for (i = 0; i < s->nb_streams && s->streams[i]->id < c->n_playlists; i++) {' $FFMPEG_PATH/libavformat/hls.c

echo "librtmp implementation of RTMP protocol should not disable native one"
sed -i '/!librtmp_protocol/d' $FFMPEG_PATH/configure

echo "change native to ffrtmp to be able to select beetween native and librtmp at runtime"
sed -i '/RTMP_PROTOCOL(rtmp)/c\RTMP_PROTOCOL(ffrtmp)' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/RTMP_PROTOCOL(rtmpe)/c\RTMP_PROTOCOL(ffrtmpe)' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/RTMP_PROTOCOL(rtmps)/c\RTMP_PROTOCOL(ffrtmps)' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/RTMP_PROTOCOL(rtmpt)/c\RTMP_PROTOCOL(ffrtmpt)' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/RTMP_PROTOCOL(rtmpte)/c\RTMP_PROTOCOL(ffrtmpte)' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/RTMP_PROTOCOL(rtmpts)/c\RTMP_PROTOCOL(ffrtmpts)' $FFMPEG_PATH/libavformat/rtmpproto.c

sed -i '/REGISTER_PROTOCOL(RTMP,/c\REGISTER_PROTOCOL(FFRTMP,             ffrtmp);' $FFMPEG_PATH/libavformat/allformats.c
sed -i '/REGISTER_PROTOCOL(RTMPE,/c\REGISTER_PROTOCOL(FFRTMPE,            ffrtmpe);' $FFMPEG_PATH/libavformat/allformats.c
sed -i '/REGISTER_PROTOCOL(RTMPS,/c\REGISTER_PROTOCOL(FFRTMPS,            ffrtmps);' $FFMPEG_PATH/libavformat/allformats.c
sed -i '/REGISTER_PROTOCOL(RTMPT,/c\REGISTER_PROTOCOL(FFRTMPT,            ffrtmpt);' $FFMPEG_PATH/libavformat/allformats.c
sed -i '/REGISTER_PROTOCOL(RTMPTE,/c\REGISTER_PROTOCOL(FFRTMPTE,           ffrtmpte);' $FFMPEG_PATH/libavformat/allformats.c
sed -i '/REGISTER_PROTOCOL(RTMPTS,/c\REGISTER_PROTOCOL(FFRTMPTS,           ffrtmpts);' $FFMPEG_PATH/libavformat/allformats.c

sed -i 's/CONFIG_RTMP_PROTOCOL/CONFIG_FFRTMP_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile
sed -i 's/CONFIG_RTMPE_PROTOCOL/CONFIG_RTMPE_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile
sed -i 's/CONFIG_RTMPS_PROTOCOL/CONFIG_RTMPS_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile
sed -i 's/CONFIG_RTMPT_PROTOCOL/CONFIG_RTMPT_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile
sed -i 's/CONFIG_RTMPTE_PROTOCOL/CONFIG_RTMPTE_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile
sed -i 's/CONFIG_RTMPTS_PROTOCOL/CONFIG_RTMPTS_PROTOCOL/g' $FFMPEG_PATH/libavformat/Makefile

sed -i 's/rtmp_protocol_select/ffrtmp_protocol_select/g' $FFMPEG_PATH/configure
sed -i 's/rtmpe_protocol_select/ffrtmpe_protocol_select/g' $FFMPEG_PATH/configure
sed -i 's/rtmps_protocol_select/ffrtmps_protocol_select/g' $FFMPEG_PATH/configure
sed -i 's/rtmpt_protocol_select/ffrtmpt_protocol_select/g' $FFMPEG_PATH/configure
sed -i 's/rtmpte_protocol_select/ffrtmpte_protocol_select/g' $FFMPEG_PATH/configure
sed -i 's/rtmpts_protocol_select/ffrtmpts_protocol_select/g' $FFMPEG_PATH/configure

sed -i '/char proto/c\char *proto, tmpProto[10], hostname[256], path[1024], auth[100], *fname;' $FFMPEG_PATH/libavformat/rtmpproto.c
sed -i '/av_url_split(proto, sizeof(proto), auth, sizeof(auth),/c\memset(tmpProto, 0, sizeof(tmpProto)); proto = &tmpProto[2]; av_url_split(tmpProto, sizeof(tmpProto), auth, sizeof(auth),' $FFMPEG_PATH/libavformat/rtmpproto.c

# we know that we have librtmp, so skip checking with require_pkg_config
sed -i '/enabled librtmp/d' $FFMPEG_PATH/configure

cd $FFMPEG_PATH

mkdir -p usr/lib

source $CONFIGURE_PATH

make
make install
