SUMMARY = "exteplayer3 - media player for E2"
DESCRIPTION = "Core of movie player for E2 based on the libeplayer using the ffmpeg solution"
SECTION = "multimedia"
LICENSE = "GPL-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

DEPENDS = "ffmpeg"
RDEPENDS_${PN} = "ffmpeg"

SRCREV = "4ee0d534fd23e97c08ac18fe1f51333e7af78b84"
SRC_URI = "git://github.com/samsamsam-iptvplayer/exteplayer3.git;branch=master"

S = "${WORKDIR}/git/"

SOURCE_FILES = "main/exteplayer.c"
SOURCE_FILES =+ "container/container.c"
SOURCE_FILES =+ "container/container_ffmpeg.c"
SOURCE_FILES =+ "manager/manager.c"
SOURCE_FILES =+ "manager/audio.c"
SOURCE_FILES =+ "manager/video.c"
SOURCE_FILES =+ "manager/subtitle.c"
SOURCE_FILES =+ "output/linuxdvb_mipsel.c"
SOURCE_FILES =+" output/output_subtitle.c"
SOURCE_FILES =+ "output/output.c"

SOURCE_FILES =+ "output/writer/mipsel/writer.c"
SOURCE_FILES =+ "output/writer/common/pes.c"
SOURCE_FILES =+ "output/writer/common/misc.c"

SOURCE_FILES =+ "output/writer/mipsel/aac.c"
SOURCE_FILES =+ "output/writer/mipsel/ac3.c"
SOURCE_FILES =+ "output/writer/mipsel/mp3.c"
SOURCE_FILES =+ "output/writer/mipsel/pcm.c"
SOURCE_FILES =+ "output/writer/mipsel/lpcm.c"
SOURCE_FILES =+ "output/writer/mipsel/dts.c"
SOURCE_FILES =+ "output/writer/mipsel/amr.c"
SOURCE_FILES =+ "output/writer/mipsel/wma.c"

SOURCE_FILES =+ "output/writer/mipsel/h264.c"
SOURCE_FILES =+ "output/writer/mipsel/h263.c"
SOURCE_FILES =+ "output/writer/mipsel/mpeg2.c"
SOURCE_FILES =+ "output/writer/mipsel/mpeg4.c"
SOURCE_FILES =+ "output/writer/mipsel/divx3.c"
SOURCE_FILES =+ "output/writer/mipsel/vc1.c"
SOURCE_FILES =+ "playback/playback.c"

do_compile() {
    ${CC} ${SOURCE_FILES} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -I${S}/include -I${D}/${libdir} -I${D}/${includedir} -lpthread -lavformat -lavcodec -lavutil -lswresample -o exteplayer3
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/exteplayer3 ${D}${bindir}
}

