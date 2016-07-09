#!/bin/bash

set -e

function usage {
   echo "Usage:"
   echo "$0 platform ffmpeg_ver"
   echo "platform:       mipsel | mipsel_softfpu | armv7 | armv5t"
   echo "ffmpeg_ver:     2.8.5 | 3.0"
   exit 1
}

if [ "$#" -ne 2 ]; 
then
    usage
fi

EPLATFORM=$1
FFMPEG_VERSION=$2

if [ "$EPLATFORM" != "mipsel" -a "$EPLATFORM" != "mipsel_softfpu" -a "$EPLATFORM" != "armv7" -a "$EPLATFORM" != "armv5t" ];
then
    echo "Please give supported platform (mipsel|mipsel_softfpu|armv7|armv5t) version!"
    usage
fi

if [ "$FFMPEG_VERSION" != "2.8.5" -a "$FFMPEG_VERSION" != "3.0" ];
then
    echo "Please give supported ffmpeg (2.8.5|3.0) version!"
    usage
fi

case "$EPLATFORM" in
    mipsel)
        BASE_PATH="/mnt/new2/xspeedlx1/build-enviroment/builds/openatv/release/et4x00/tmp/sysroots/"
        export TOOLCHAIN_NAME="mipsel-oe-linux"
        export PATH=$BASE_PATH"i686-linux/usr/bin/mipsel-oe-linux/":$PATH
        export SYSROOT=$BASE_PATH"et4x00"
        CFLAGS="  -mel -mabi=32 -march=mips32 "
        FFMPEG_CFLAGS=" -mel -mabi=32 -march=mips32 "
        ;;
    mipsel_softfpu)
        BASE_PATH="/mnt/new2/softFPU/openpli/build/tmp/sysroots/"
        export TOOLCHAIN_NAME="mipsel-oe-linux"
        export PATH=$BASE_PATH"i686-linux/usr/bin/mipsel-oe-linux/":$PATH
        export SYSROOT=$BASE_PATH"et4x00"
        CFLAGS="  -mel -mabi=32 -msoft-float -march=mips32 "
        FFMPEG_CFLAGS=" -mel -mabi=32 -msoft-float -march=mips32 "
        ;;
    armv7)
        BASE_PATH="/mnt/new2/vusolo4k/openvuplus_3.0/build/vusolo4k/tmp/sysroots/"
        export TOOLCHAIN_NAME="arm-oe-linux-gnueabi"
        export PATH=$BASE_PATH"i686-linux/usr/bin/arm-oe-linux-gnueabi/":$PATH
        export SYSROOT=$BASE_PATH"vusolo4k"
        CFLAGS=" -march=armv7-a -mfloat-abi=hard -mfpu=neon "
        ;;
    armv5t)
        BASE_PATH="/mnt/new2/openatv2/build-enviroment/builds/openatv/release/cube/tmp/sysroots/"
        export TOOLCHAIN_NAME="arm-oe-linux-gnueabi"
        export PATH=$BASE_PATH"i686-linux/usr/bin/arm-oe-linux-gnueabi/":$PATH
        export SYSROOT=$BASE_PATH"cube"
        CFLAGS=" -mfloat-abi=softfp -mtune=cortex-a9 -mfpu=vfpv3-d16 "
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
SOURCE_FILES+=" output/linuxdvb_mipsel.c"
SOURCE_FILES+=" output/output_subtitle.c"
SOURCE_FILES+=" output/output.c"

SOURCE_FILES+=" output/writer/mipsel/writer.c"
SOURCE_FILES+=" output/writer/common/pes.c"
SOURCE_FILES+=" output/writer/common/misc.c"

SOURCE_FILES+=" output/writer/mipsel/aac.c"  
SOURCE_FILES+=" output/writer/mipsel/ac3.c"  
SOURCE_FILES+=" output/writer/mipsel/mp3.c"  
SOURCE_FILES+=" output/writer/mipsel/pcm.c"  
SOURCE_FILES+=" output/writer/mipsel/lpcm.c"  
SOURCE_FILES+=" output/writer/mipsel/dts.c"  
SOURCE_FILES+=" output/writer/mipsel/amr.c"
SOURCE_FILES+=" output/writer/mipsel/wma.c"

SOURCE_FILES+=" output/writer/mipsel/h264.c"
SOURCE_FILES+=" output/writer/mipsel/h263.c"
SOURCE_FILES+=" output/writer/mipsel/mpeg2.c"
SOURCE_FILES+=" output/writer/mipsel/mpeg4.c"
SOURCE_FILES+=" output/writer/mipsel/divx3.c"
SOURCE_FILES+=" output/writer/mipsel/vc1.c"  
#SOURCE_FILES+=" output/writer/mipsel/wmv.c" 
SOURCE_FILES+=" playback/playback.c"

CURR_PATH=$PWD

EXTEPLAYER3_OUT_FILE=$CURR_PATH"/tmp/out/$EPLATFORM/exteplayer3_ffmpeg"$FFMPEG_VERSION

function buildFFmpeg 
{
    FFMPEG_VERSION=$1
    FFMPEG_BASE_PATH=$CURR_PATH"/tmp/ffmpeg/"
    FFMPEG_PATH=$FFMPEG_BASE_PATH"tmp/ffmpeg-"$FFMPEG_VERSION
    
    SOURCE_URL="http://ffmpeg.org/releases/ffmpeg-"$FFMPEG_VERSION".tar.gz"
    OUT_FILE=$FFMPEG_PATH".tar.gz"
    
    if [ "true" == "$2" ];
    then
        if [ -d $FFMPEG_PATH ] && [ "true" == "$3"  ];
        then
            rm -rf $FFMPEG_PATH
        fi
        
        if [ ! -f $OUT_FILE ];
        then
            wget $SOURCE_URL -O $OUT_FILE
        fi
        
        if [ "true" == "$3" ];
        then
            tar -zxf $OUT_FILE -C $FFMPEG_BASE_PATH"tmp/"
        fi
        
        CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        
        $FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT "$FFMPEG_CFLAGS"
    else
        #CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        #$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT
        echo "Skip ffmpeg build"
    fi
}

buildFFmpeg $FFMPEG_VERSION "true" "true" # "false" "true"

rm -rf $EXTEPLAYER3_OUT_FILE

echo "FFMPEG_PATH = $FFMPEG_PATH"
"$CROSS_COMPILE"gcc -fdata-sections -ffunction-sections -Wl,--gc-sections -Os $CFLAGS --sysroot=$SYSROOT $LDFLAGS $CPPFLAGS -I"$CURR_PATH"/include  -I$FFMPEG_PATH/usr/include/ -L$FFMPEG_PATH/usr/lib/ $SOURCE_FILES -o $EXTEPLAYER3_OUT_FILE -Wfatal-errors -lpthread -lavformat -lavcodec -lavutil -lswresample 
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

    
