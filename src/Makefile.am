AM_CPPFLAGS = @DBUS_CFLAGS@ @GLIB_CFLAGS@
AM_CFLAGS = -std=gnu99

lib_LTLIBRARIES=libhal.la

libhal_la_SOURCES = libhal.c libhal.h

libhal_la_LIBADD = @DBUS_LIBS@ @GLIB_LIBS@

libhal_la_LDFLAGS = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

clean-local :
	rm -f *~

