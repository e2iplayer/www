AUTOMAKE_OPTIONS = subdir-objects
AM_CFLAGS = -pipe -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE

CXXFLAGS = -Wall

AM_CPPFLAGS = \
	-I../git/include

lib_LTLIBRARIES = libexteplayer3.la

libexteplayer3_la_SOURCES = \
    container/container.c \
    container/container_ffmpeg.c \
    manager/audio.c \
    manager/manager.c \
    manager/video.c \
    output/linuxdvb_mipsel.c \
    output/output.c \
    output/writer/mipsel/writer.c \
    output/writer/common/pes.c \
    output/writer/common/misc.c \
    output/writer/mipsel/aac.c \
    output/writer/mipsel/ac3.c \
    output/writer/mipsel/mp3.c \
    output/writer/mipsel/pcm.c \
    output/writer/mipsel/lpcm.c \
    output/writer/mipsel/dts.c \
    output/writer/mipsel/amr.c \
    output/writer/mipsel/wma.c \
    output/writer/mipsel/h264.c \
    output/writer/mipsel/h263.c \
    output/writer/mipsel/mpeg2.c \
    output/writer/mipsel/mpeg4.c \
    output/writer/mipsel/divx3.c \
    output/writer/mipsel/vc1.c \
    playback/playback.c

libexteplayer3_la_LIBADD = -lpthread -lavformat -lavcodec -lavutil -lswresample

bin_PROGRAMS = exteplayer3
exteplayer3_SOURCES = main/exteplayer.c
exteplayer3_LDADD = -lexteplayer3 -lpthread
exteplayer3_DEPENDENCIES = libexteplayer3.la
