bin_PROGRAMS=xrestrict rectest

AM_CFLAGS=--pedantic -Wall -std=c99 $(X11_CFLAGS) $(XRANDR_CFLAGS) $(XINPUT_CFLAGS)
xrestrict_LDADD=$(X11_LIBS) $(X11_LIBS) $(XINPUT_LIBS) $(XRANDR_LIBS)
rectest_LDADD=$(X11_LIBS) $(X11_LIBS) $(XINPUT_LIBS) $(XRANDR_LIBS)

xrestrict_SOURCES=xrestrict.h xrestrict.c \
input.h input.c \
display.h display.c

rectest_SOURCES=input.h input.c rectest.c
