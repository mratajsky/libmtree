lib_LTLIBRARIES = libmtree.la

AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libmtree

libmtree_includedir = $(includedir)/libmtree

libmtree_include_HEADERS =			\
	mtree.h					\
	mtree_file.h

libmtree_la_SOURCES =				\
	mtree.c					\
	mtree_crc.c				\
	mtree_entry.c				\
	mtree_reader.c 				\
	mtree_spec.c 				\
	mtree_utils.c 				\
	mtree_private.h

libmtree_la_LDFLAGS =				\
	-version-info $(LT_VERSION)		\
	-no-undefined				\
	-export-dynamic
