AM_CFLAGS = \
	-Wall

AM_CPPFLAGS = \
	@PYTHON_CPPFLAGS@ \
	-include Python.h

AM_CXXFLAGS = \
	-Wall \
	@ENIGMA2_CFLAGS@ \
	@FFMPEG_CFLAGS@

plugindir = $(libdir)/enigma2/python/Plugins/SystemPlugins/ServiceHisilicon

plugin_LTLIBRARIES = servicehisilicon.la

servicehisilicon_la_SOURCES = \
	servicehisiliconrecord.cpp \
	servicehisilicon.cpp \
	pythonmodule.cpp

servicehisilicon_la_LDFLAGS = \
	-avoid-version \
	-module \
	-shared
	
servicehisilicon_la_LIBADD = -lswscale -ldl -lpthread -lavformat -lavcodec -lavutil -lswresample
