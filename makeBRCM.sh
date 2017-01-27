#!/bin/bash

set -e

function usage {
   echo "Usage:"
   echo "$0 platform ffmpeg_ver"
   echo "platform:       mipsel | mipsel_softfpu | armv7 | armv5t"
   echo "ffmpeg_ver:     2.8.5 | 3.0.5 | 3.1.1 | 3.2.2"
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

if [ "$FFMPEG_VERSION" != "2.8.5" -a "$FFMPEG_VERSION" != "3.0.5" -a "$FFMPEG_VERSION" != "3.1.1" -a "$FFMPEG_VERSION" != "3.2.2" ];
then
    echo "Please give supported ffmpeg (2.8.5|3.0.5|3.1.1|3.2.2) version!"
    usage
fi

case "$EPLATFORM" in
    mipsel)
        BASE_PATH="/mnt/new2/xspeedlx1/build-enviroment/builds/openatv/release/et4x00/tmp/sysroots/"
        export TOOLCHAIN_NAME="mipsel-oe-linux"
        export PATH=$BASE_PATH"i686-linux/usr/bin/mipsel-oe-linux/":$PATH
        export SYSROOT=$BASE_PATH"et4x00"
        CFLAGS="  -mel -mabi=32 -march=mips32 "
        FFMPEG_CFLAGS=" -mel -mabi=32 -march=mips32 -I$SYSROOT/usr/include/libxml2/"
        FFMPEG_LDFLAGS=" -lrtmp -lxml2 "
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
        if [ 0 -eq 1 ];
        then 
            echo "ARMv7 toolchain openvuplus_3.0"
            BASE_PATH="/mnt/new2/vusolo4k/openvuplus_3.0/build/vusolo4k/tmp/sysroots/"
            export TOOLCHAIN_NAME="arm-oe-linux-gnueabi"
            export PATH=$BASE_PATH"i686-linux/usr/bin/arm-oe-linux-gnueabi/":$PATH
            export SYSROOT=$BASE_PATH"vusolo4k"
        else
            echo "ARMv7 toolchain openpli_4.0"
            BASE_PATH="/mnt/new2/openpli_micro/openpli-oe-core/build/tmp/sysroots/"
            export TOOLCHAIN_NAME="arm-oe-linux-gnueabi"
            export PATH=$BASE_PATH"i686-linux/usr/bin/arm-oe-linux-gnueabi/":$PATH
            export SYSROOT=$BASE_PATH"hd51"

        fi
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
SOURCE_FILES+=" output/writer/mipsel/vp.c"
#SOURCE_FILES+=" output/writer/mipsel/wmv.c" 
SOURCE_FILES+=" playback/playback.c"

CURR_PATH=$PWD

EXTEPLAYER3_OUT_FILE=$CURR_PATH"/tmp/out/$EPLATFORM/exteplayer3_ffmpeg"$FFMPEG_VERSION

function buildFFmpeg 
{
    FFMPEG_VERSION=$1
    FFMPEG_BASE_PATH=$CURR_PATH"/tmp/ffmpeg/"
    mkdir -p $FFMPEG_BASE_PATH"tmp/$EPLATFORM/"
    FFMPEG_PATH=$FFMPEG_BASE_PATH"tmp/$EPLATFORM/ffmpeg-"$FFMPEG_VERSION
    FFMPEG_PATCHES_PATH=$FFMPEG_BASE_PATH"patches/"$FFMPEG_VERSION"/"
    
    SOURCE_URL="http://ffmpeg.org/releases/ffmpeg-"$FFMPEG_VERSION".tar.gz"
    OUT_FILE=$FFMPEG_BASE_PATH"tmp/ffmpeg-"$FFMPEG_VERSION".tar.gz"
    
    echo "FFMPEG PATH: $FFMPEG_PATH"
    
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
            
            if [ -d $FFMPEG_PATCHES_PATH ];
            then
                echo "Applay patches for ffmpeg version $FFMPEG_VERSION if any"
                cd $FFMPEG_PATH
                for i in "$FFMPEG_PATCHES_PATH"*.patch; do patch -p1 < $i; done
                cd $CURR_PATH
            fi
        fi
        
        CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        
        $FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT "$FFMPEG_CFLAGS" "$FFMPEG_LDFLAGS"
    else
        #CONFIGURE_PATH=$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/configure_"$FFMPEG_VERSION".sh"
        #$FFMPEG_BASE_PATH"/scripts_$EPLATFORM/make.sh" $FFMPEG_PATH $CONFIGURE_PATH $SYSROOT
        echo "Skip ffmpeg build"
    fi
}

# rebuild ffmpeg libs, force rebuild
buildFFmpeg $FFMPEG_VERSION "true" "true"

rm -rf $EXTEPLAYER3_OUT_FILE

echo "FFMPEG_PATH = $FFMPEG_PATH"
"$CROSS_COMPILE"gcc -fdata-sections -ffunction-sections -Wl,--gc-sections -Os $CFLAGS --sysroot=$SYSROOT $LDFLAGS $CPPFLAGS -I"$CURR_PATH"/include  -I$FFMPEG_PATH/usr/include/ -L$FFMPEG_PATH/usr/lib/ $SOURCE_FILES -o $EXTEPLAYER3_OUT_FILE -Wfatal-errors -lpthread -lavformat -lavcodec -lavutil -lswresample 
"$CROSS_COMPILE"strip -s $EXTEPLAYER3_OUT_FILE

#exit 0

FFMPEG_PACK_TMP=tmp/ffmpeg/tmp/ffmpeg"$FFMPEG_VERSION"_$EPLATFORM
rm -rf $FFMPEG_PACK_TMP 
mkdir $FFMPEG_PACK_TMP

cp -R tmp/ffmpeg/tmp/$EPLATFORM/ffmpeg-"$FFMPEG_VERSION"/usr $FFMPEG_PACK_TMP"/usr"
cd  $FFMPEG_PACK_TMP
rm -Rf usr/include
rm -Rf usr/share
rm -Rf usr/lib/pk*
echo ">>>>>>>>>>>>>>>>>>>>>>"
tar -zcvf ../ffmpeg"$FFMPEG_VERSION"_$EPLATFORM.tar.gz usr

    
