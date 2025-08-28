AM_CFLAGS = \
	-Wall

AM_CPPFLAGS += \
	@PYTHON_CPPFLAGS@ \
	$(FFMPEG_CFLAGS)
	-include Python.h

AM_CXXFLAGS += \
	-Wall \
	@ENIGMA2_CFLAGS@

AM_LDFLAGS  += $(FFMPEG_LIBS) \
    -lavformat -lavcodec -lavutil -lswresample -lswscale

plugindir = $(libdir)/enigma2/python/Plugins/SystemPlugins/ServiceHisilicon

plugin_LTLIBRARIES = servicehisilicon.la

servicehisilicon_la_SOURCES += \
	servicehisiliconrecord.cpp \
	servicehisilicon.cpp \
	pythonmodule.cpp

servicehisilicon_la_LDFLAGS += \
	-avoid-version \
	-module \
	-shared
