lib_LTLIBRARIES = libmtree.la

AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libmtree

libmtree_includedir = $(includedir)/libmtree

libmtree_include_HEADERS =			\
	mtree.h

libmtree_la_SOURCES =				\
	mtree.c

libmtree_la_LDFLAGS =				\
	-version-info $(LT_VERSION)		\
	-no-undefined				\
	-export-dynamic
