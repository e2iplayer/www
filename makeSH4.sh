#!/bin/bash

set -e

function usage {
   echo "Usage:"
   echo "$0 platform ffmpeg_ver"
   echo "platform:       sh4 | sh4_new"
   echo "ffmpeg_ver:     1.0.10 | 1.1.16 | 1.2.12 | 2.0.7 | 2.2.16 | 2.5.11 | 2.6.9 | 2.7.7 | 2.8.7 | 3.0.2 | 3.1.1 "
   exit 1
}

if [ "$#" -ne 2 ]; 
then
    usage
fi

EPLATFORM=$1

if [ "$EPLATFORM" != "sh4" -a "$EPLATFORM" != "sh4_new" ];
then
    echo "Please give supported platform (sh4|sh4_new) version!"
    usage
fi

FFMPEG_VERSION=""
ffmpegVersionArray=( "1.0.10" "1.1.16" "1.2.12" "2.0.7" "2.2.16" "2.5.11" "2.6.9" "2.7.7" "2.8.7" "3.0.2" "3.1.1" )
for i in "${ffmpegVersionArray[@]}"
do
    if [ "$i" == "$2" ];
    then
        FFMPEG_VERSION=$2
        break
    fi
done

if [ "$FFMPEG_VERSION" == "" ];
then
    echo "Please give supported ffmpeg (1.0.10 | 1.1.16 | 1.2.12 | 2.0.7  | 2.2.16 | 2.5.11 | 2.6.9 | 2.7.7 | 2.8.7 | 3.0.2 | 3.1.1) version!"
    usage
fi

case "$EPLATFORM" in
    sh4)
        BASE_PATH="/home/sulge/e2/tdt/tdt/tufsbox/"
        export TOOLCHAIN_NAME="sh4-linux"
        export PATH=$BASE_PATH"devkit/sh4/bin/":$PATH
        export SYSROOT=""
        CFLAGS=" -I$BASE_PATH/cdkroot/ -L$BASE_PATH/cdkroot/ "
        LDFLAGS=" -L$BASE_PATH/cdkroot/ -L$BASE_PATH/cdkroot/ "
       
        OPENSSL_VER="openssl-1.0.2g"
        OPENSSL_PATH="/mnt/new2/new_openssl_sh4/$OPENSSL_VER"
        
        FFMPEG_CFLAGS=" -I$BASE_PATH/cdkroot/ -I$OPENSSL_PATH/include "
        FFMPEG_LDFLAGS=" -I$BASE_PATH/cdkroot/ -L$OPENSSL_PATH "
        ;;
    sh4_new)
        BASE_PATH="/mnt/new2/openatv/build-enviroment/builds/openatv/spark/tmp/sysroots/"
        export TOOLCHAIN_NAME="sh4-oe-linux"
        export PATH=$BASE_PATH"i686-linux/usr/bin/sh4-oe-linux/":$PATH
        export SYSROOT=$BASE_PATH"spark"
        CFLAGS="  "
        FFMPEG_CFLAGS="  "
        ;;
    *)
        usage
        exit 1
esac

CFLAGS="$CFLAGS -pipe -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE "

export CROSS_COMPILE=$TOOLCHAIN_NAME"-"

SOURCE_FILES="main/exteplayer.c"
SOURCE_FILES+=" container/container.c"
SOURCE_FILES+=" container/container_ffmpeg.c"
SOURCE_FILES+=" manager/manager.c"
SOURCE_FILES+=" manager/audio.c"
SOURCE_FILES+=" manager/video.c"
SOURCE_FILES+=" manager/subtitle.c"
SOURCE_FILES+=" output/linuxdvb_sh4.c"
SOURCE_FILES+=" output/output_subtitle.c"
SOURCE_FILES+=" output/output.c"

SOURCE_FILES+=" output/writer/common/pes.c"
SOURCE_FILES+=" output/writer/common/misc.c"

SOURCE_FILES+=" output/writer/sh4/writer.c"
SOURCE_FILES+=" output/writer/sh4/aac.c"
SOURCE_FILES+=" output/writer/sh4/ac3.c"
#SOURCE_FILES+=" output/writer/sh4/divx.c"
SOURCE_FILES+=" output/writer/sh4/divx2.c"
SOURCE_FILES+=" output/writer/sh4/dts.c"
SOURCE_FILES+=" output/writer/sh4/h263.c"
SOURCE_FILES+=" output/writer/sh4/h264.c"
SOURCE_FILES+=" output/writer/sh4/mp3.c"
SOURCE_FILES+=" output/writer/sh4/mpeg2.c"
SOURCE_FILES+=" output/writer/sh4/pcm.c"
SOURCE_FILES+=" output/writer/sh4/vc1.c"
#SOURCE_FILES+=" output/writer/sh4/vorbis.c"
SOURCE_FILES+=" output/writer/sh4/wma.c"
SOURCE_FILES+=" output/writer/sh4/wmv.c"
SOURCE_FILES+=" playback/playback.c"

CURR_PATH=$PWD

EXTEPLAYER3_OUT_FILE=$CURR_PATH"/tmp/out/$EPLATFORM/exteplayer3_ffmpeg"$FFMPEG_VERSION

function buildFFmpeg 
{
    FFMPEG_VERSION=$1
    FFMPEG_BASE_PATH=$CURR_PATH"/tmp/ffmpeg/"
    mkdir -p $FFMPEG_BASE_PATH"tmp/$EPLATFORM/"
    FFMPEG_PATH=$FFMPEG_BASE_PATH"tmp/$EPLATFORM/ffmpeg-"$FFMPEG_VERSION
    
    SOURCE_URL="http://ffmpeg.org/releases/ffmpeg-"$FFMPEG_VERSION".tar.gz"
    OUT_FILE=$FFMPEG_BASE_PATH"tmp/ffmpeg-"$FFMPEG_VERSION".tar.gz"
    
    if [ "true" == "$2" ] || [ ! -d $FFMPEG_PATH ];
    then
        if [ -d $FFMPEG_PATH ] && [ "true" == "$3"  ];
        then
            rm -rf $FFMPEG_PATH
        fi
        
        if [ ! -f $OUT_FILE ];
        then
            wget $SOURCE_URL -O $OUT_FILE
        fi
        
        if [ ! -d $FFMPEG_PATH ];
        then
            tar -zxf $OUT_FILE -C $FFMPEG_BASE_PATH"tmp/$EPLATFORM/"
        fi
        
        CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        
        $FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT "$FFMPEG_CFLAGS"
    else
        #CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        #$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT
        echo "Skip ffmpeg build"
    fi
}

# rebuild ffmpeg libs, force rebuild
buildFFmpeg $FFMPEG_VERSION "false" "false"

rm -rf $EXTEPLAYER3_OUT_FILE

echo "FFMPEG_PATH = $FFMPEG_PATH"
if [ "$SYSROOT" != "" ];
then
    SYSROOT_VAR="--sysroot=$SYSROOT"
else
    SYSROOT_VAR=""
fi
"$CROSS_COMPILE"gcc -fdata-sections -ffunction-sections -Wl,--gc-sections -Os $CFLAGS $SYSROOT_VAR $LDFLAGS $CPPFLAGS -I"$CURR_PATH"/include  -I$FFMPEG_PATH/usr/include/ -L$FFMPEG_PATH/usr/lib/ $SOURCE_FILES -o $EXTEPLAYER3_OUT_FILE -Wfatal-errors -lpthread -lavformat -lavcodec -lavutil -lswresample 
"$CROSS_COMPILE"strip -s $EXTEPLAYER3_OUT_FILE

exit 0

FFMPEG_PACK_TMP=tmp/ffmpeg/tmp/ffmpeg"$FFMPEG_VERSION"_$EPLATFORM
rm -rf $FFMPEG_PACK_TMP 
mkdir $FFMPEG_PACK_TMP

cp -R tmp/ffmpeg/tmp/ffmpeg-"$FFMPEG_VERSION"/usr $FFMPEG_PACK_TMP"/usr"
cd  $FFMPEG_PACK_TMP
rm -Rf usr/include
rm -Rf usr/share
rm -Rf usr/lib/pk*
echo ">>>>>>>>>>>>>>>>>>>>>>"
tar -zcvf ../ffmpeg"$FFMPEG_VERSION"_$EPLATFORM.tar.gz usr

    
