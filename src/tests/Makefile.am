MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I$(top_srcdir)/src/lib/ecore \
-I$(top_srcdir)/src/lib/ecore_con \
@EINA_CFLAGS@ \
@CHECK_CFLAGS@

if EFL_ENABLE_TESTS

noinst_PROGRAMS = ecore_suite

check_PROGRAMS = ecore_suite

ecore_suite_SOURCES = \
ecore_suite.c \
ecore_test_ecore.c \
ecore_test_ecore_con.c

ecore_suite_LDADD = \
@CHECK_LIBS@ \
$(top_builddir)/src/lib/ecore/libecore.la \
$(top_builddir)/src/lib/ecore_con/libecore_con.la

endif

EXTRA_DIST = ecore_suite.h
