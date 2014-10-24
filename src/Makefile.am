bin_PROGRAMS=xrestrict rectest

AM_CFLAGS=--pedantic -Wall -std=c99 $(X11_CFLAGS) $(XCB_X11_CFLAGS) $(XCB_RANDR_CFLAGS) $(XINPUT_CFLAGS)
xrestrict_LDADD=$(X11_LIBS) $(XCB_X11_LIBS) $(X11_LIBS) $(XINPUT_LIBS) $(XCB_RANDR_LIBS)
rectest_LDADD=$(X11_LIBS) $(XCB_X11_LIBS) $(X11_LIBS) $(XINPUT_LIBS) $(XCB_RANDR_LIBS)

xrestrict_SOURCES=xrestrict.h xrestrict.c \
input.h input.c

rectest_SOURCES=input.h input.c rectest.c
