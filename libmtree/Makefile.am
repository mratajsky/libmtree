SUBDIRS = tests

lib_LTLIBRARIES = libmtree.la

AM_CPPFLAGS =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/compat			\
	-I$(top_srcdir)/libmtree

libmtree_includedir = $(includedir)

libmtree_include_HEADERS =			\
	mtree.h					\
	mtree_file.h

libmtree_la_SOURCES =				\
	mtree.c					\
	mtree_cksum.c				\
	mtree_device.c				\
	mtree_digest.c				\
	mtree_entry.c				\
	mtree_reader.c 				\
	mtree_spec.c 				\
	mtree_spec_diff.c 			\
	mtree_trie.c 				\
	mtree_utils.c 				\
	mtree_writer.c 				\
	mtree_private.h

libmtree_la_CFLAGS =				\
	$(NETTLE_CFLAGS)
libmtree_la_LIBADD =				\
	$(NETTLE_LIBS)				\
	$(top_builddir)/compat/libcompat.la

libmtree_la_LDFLAGS =				\
	-version-info $(LT_VERSION)		\
	-no-undefined				\
	-export-dynamic
