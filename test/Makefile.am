AM_CPPFLAGS = @DBUS_CFLAGS@
AM_CFLAGS = -std=gnu99 -I../src
AM_LDFLAGS = @DBUS_LIBS@
LTVALGRIND = $(top_srcdir)/libtool --execute $(VALGRIND)

TESTS = flash
TESTS_ENVIRONMENT = FLASH="$(LTVALGRIND) flash"

noinst_PROGRAMS = flash
flash_SOURCES = flash.c
flash_LDADD = ../src/libhal.la
flash_LDFLAGS = -static

clean-local :
	rm -f *~
