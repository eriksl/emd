TARGET	= x86_64
DEBUG	= off
AOUT	= emd
OBJS	= emd.o mp3meta.o emd_state.o program.o http_server.o http_page.o syslog.o mp3reader.o
DEPS	= .emd.d .mp3meta.d .emd_state.d .program.d .http_server.d .http_page.d .syslog.d .mp3reader.d
PROGRAM	= emd

ifeq ($(TARGET), x86_64)
	CPPFLAGS	+= -DMHD_mode_multithread
	LDLIBS		= -lmicrohttpd -lid3tag -lpthread
endif

include common.mak
