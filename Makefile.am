# http://people.gnome.org/~walters/docs/build-api.txt
.buildapi-allow-builddir:

-include $(INTROSPECTION_MAKEFILE)

bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
noinst_LTLIBRARIES =
dist_gwkjsjs_DATA =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
TEST_PROGS =
check_PROGRAMS = $(TEST_PROGS)
INTROSPECTION_GIRS =
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwkjs\"

gwkjsjsdir = @gwkjsjsdir@
gwkjsoverridedir = $(gwkjsjsdir)/overrides

gwkjs_public_includedir = $(includedir)/gwkjs-1.0
gwkjs_module_includedir = $(includedir)/gwkjs-1.0

########################################################################
nobase_gwkjs_public_include_HEADERS =	\
	gwkjs/context.h		\
	gwkjs/gwkjs.h

nobase_gwkjs_module_include_HEADERS =	\
	gwkjs/gwkjs-module.h	\
	gwkjs/compat.h		\
	gwkjs/coverage.h \
	gwkjs/byteArray.h		\
	gwkjs/importer.h		\
	gwkjs/jsapi-util.h	\
	gwkjs/exceptions.h	\
	gwkjs/runtime.h		\
	gwkjs/type-module.h	\
	gwkjs/mem.h		\
	gwkjs/native.h	\
	gi/ns.h	        \
	gi/object.h	\
	gi/foreign.h	\
	gi/fundamental.h	\
	gi/param.h	\
	gi/repo.h	\
	gi/union.h	\
	gi/value.h	\
	gi/arg.h	\
	gi/boxed.h	\
	gi/closure.h	\
	gi/enumeration.h	\
	gi/function.h	\
	gi/keep-alive.h	\
	gi/interface.h	\
	gi/gtype.h	\
	gi/gerror.h

noinst_HEADERS +=		\
	gwkjs/jsapi-private.h	\
	gwkjs/context-private.h	\
	gi/proxyutils.h		\
	util/crash.h		\
	util/hash-x32.h		\
	util/error.h		\
	util/glib.h		\
	util/log.h		\
	util/misc.h

########################################################################
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gwkjs-1.0.pc gwkjs-internals-1.0.pc

EXTRA_DIST += 			\
	gwkjs-1.0.pc.in           \
	gwkjs-internals-1.0.pc.in

########################################################################
gwkjs_directory_defines = 				\
	-DGWKJS_TOP_SRCDIR=\"$(top_srcdir)\"		\
	-DGWKJS_TOP_BUILDDIR=\"$(top_builddir)\"		\
	-DGWKJS_JS_DIR=\"$(gwkjsjsdir)\"                    \
	-DPKGLIBDIR=\"$(pkglibdir)\"

########################################################################
lib_LTLIBRARIES += libgwkjs.la

libgwkjs_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)		\
	$(GWKJS_CFLAGS)	\
	$(gwkjs_directory_defines)\
	-I$(top_srcdir)/gi	\
	-DGWKJS_COMPILATION
libgwkjs_la_LDFLAGS = 		\
	$(EXTRA_LINK_FLAGS)	\
	-export-symbols-regex "^[^_]" -version-info 0:0:0	\
	-no-undefined \
	-rdynamic
libgwkjs_la_LIBADD = 		\
	$(GWKJS_LIBS)

if ENABLE_GTK
libgwkjs_la_CPPFLAGS += $(GWKJS_GTK_CFLAGS)
libgwkjs_la_LIBADD += $(GWKJS_GTK_LIBS)
endif

libgwkjs_la_SOURCES =		\
	gwkjs/byteArray.cpp		\
	gwkjs/context.cpp		\
	gwkjs/importer.cpp		\
	gwkjs/gi.h		\
	gwkjs/gi.cpp		\
	gwkjs/coverage-internal.h	\
	gwkjs/coverage.cpp \
	gwkjs/exceptions.cpp \
	gwkjs/jsapi-private.cpp	\
	gwkjs/jsapi-util.cpp	\
	gwkjs/jsapi-dynamic-class.cpp \
	gwkjs/jsapi-util-array.cpp	\
	gwkjs/jsapi-util-error.cpp	\
	gwkjs/jsapi-util-string.cpp	\
	gwkjs/mem.cpp		\
	gwkjs/native.cpp		\
	gwkjs/runtime.cpp		\
	gwkjs/stack.cpp		\
	gwkjs/type-module.cpp	\
	modules/modules.cpp	\
	modules/modules.h	\
	util/error.cpp		\
	util/hash-x32.cpp		\
	util/glib.cpp		\
	util/crash.cpp		\
	util/log.cpp		\
	util/misc.cpp

# For historical reasons, some files live in gi/
libgwkjs_la_SOURCES += \
	gi/gwkjs_gi_trace.h \
	gi/arg.cpp	\
	gi/boxed.cpp	\
	gi/closure.cpp	\
	gi/enumeration.cpp	\
	gi/function.cpp	\
	gi/keep-alive.cpp	\
	gi/ns.cpp	\
	gi/object.cpp	\
	gi/foreign.cpp	\
	gi/fundamental.cpp	\
	gi/param.cpp	\
	gi/proxyutils.cpp	\
        gi/repo.cpp	\
	gi/union.cpp	\
        gi/value.cpp	\
	gi/interface.cpp	\
	gi/gtype.cpp	\
	gi/gerror.cpp

# Also, these files used to be a separate library
libgwkjs_private_source_files = \
	libgwkjs-private/gwkjs-gdbus-wrapper.cpp	\
	libgwkjs-private/gwkjs-gdbus-wrapper.h	\
	libgwkjs-private/gwkjs-util.cpp		\
	libgwkjs-private/gwkjs-util.h		\
	libgwkjs-private/gwkjs-gtk-util.h

if ENABLE_GTK
libgwkjs_private_source_files += 			\
	libgwkjs-private/gwkjs-gtk-util.c		\
	$(NULL)
endif

libgwkjs_la_SOURCES += $(libgwkjs_private_source_files)

GwkjsPrivate-1.0.gir: libgwkjs.la
GwkjsPrivate_1_0_gir_LIBS = libgwkjs.la
GwkjsPrivate_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
GwkjsPrivate_1_0_gir_FILES = $(libgwkjs_private_source_files)
GwkjsPrivate_1_0_gir_SCANNERFLAGS = --identifier-prefix=Gwkjs --symbol-prefix=gwkjs_ --warn-all

if ENABLE_GTK
GwkjsPrivate_1_0_gir_INCLUDES += Gtk-3.0
endif

INTROSPECTION_GIRS += GwkjsPrivate-1.0.gir

if ENABLE_DTRACE
gwkjs_gi_probes.h: gi/gwkjs_gi_probes.d
	$(DTRACE) -C -h -s $< -o $@
gwkjs_gi_probes.o: gi/gwkjs_gi_probes.d
	$(DTRACE) -G -s $< -o $@
BUILT_SOURCES += gwkjs_gi_probes.h gwkjs_gi_probes.o
libgwkjs_la_LIBADD += gwkjs_gi_probes.o
endif
EXTRA_DIST += gi/gwkjs_gi_probes.d

tapset_in_files = gwkjs/gwkjs.stp.in
EXTRA_DIST += $(tapset_in_files)
if ENABLE_SYSTEMTAP
gwkjs/gwkjs.stp: gwkjs/gwkjs.stp.in Makefile
	sed -e s,@EXPANDED_LIBDIR@,$(libdir), < $< > $@.tmp && mv $@.tmp $@
tapsetdir   = $(DESTDIR)$(datadir)/systemtap/tapset
tapset_DATA = $(tapset_in_files:.stp.in=.stp)
endif

include Makefile-modules.am
include Makefile-examples.am

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(INTROSPECTION_GIRS) $(typelib_DATA)

########################################################################
bin_PROGRAMS += gwkjs-console

gwkjs_console_CPPFLAGS = 		\
	$(AM_CPPFLAGS)		\
        $(GOBJECT_CFLAGS)
gwkjs_console_LDADD =		\
	$(JS_LIBS)		\
	$(GOBJECT_LIBS)		\
	libgwkjs.la
gwkjs_console_LDFLAGS = -rdynamic
gwkjs_console_SOURCES = gwkjs/console.cpp

install-exec-hook:
	(cd $(DESTDIR)$(bindir) && ln -sf gwkjs-console$(EXEEXT) gwkjs$(EXEEXT))

include Makefile-test.am
include Makefile-insttest.am

EXTRA_DIST += autogen.sh COPYING.LGPL

# Colin's handy Makefile bits for:
# 1) stuffing tarballs with pre-generated scripts from your workstation
# 2) bumping configure.ac version post-release
# 3) tagging correctly in git
# 4) uploading to gnome.org
# To use:
#  $ make check
#  $ make dist
#  $ make prepare-minor-release

# Customize to taste
TAG_PREFIX=GWKJS_
COMPRESSION=.bz2


PACKAGE=@PACKAGE@
VERSION=@VERSION@
DISTNAME=$(PACKAGE)-$(VERSION).tar$(COMPRESSION)
TAG_VERSION=$(shell echo $(VERSION) |sed s/\\\./_/g)

prepare-release-tag: Makefile
	git tag -m "Tag $(TAG_VERSION)" -a $(TAG_PREFIX)$(TAG_VERSION)

prepare-minor-release: $(DISTNAME) prepare-release-tag Makefile
	env top_srcdir=$(top_srcdir) python $(top_srcdir)/verbump.py

upload-release: $(DISTNAME) Makefile
	git log origin/master..master
	@echo -n "Ok to push? [y/N] "; read ans; test x$$ans == xy || exit 1
	git push --tags origin master:master
	scp $(DISTNAME) master.gnome.org:
	ssh master.gnome.org install-module $(DISTNAME)
