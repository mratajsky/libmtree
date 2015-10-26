AM_CPPFLAGS =			\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/compat

TESTS = libmtree_test
check_PROGRAMS = libmtree_test
libmtree_test_SOURCES =		\
	test.c			\
	test.h			\
	test_cksum.c		\
	test_digest.c		\
	test_entry.c		\
	test_misc.c		\
	test_spec_diff.c	\
	test_trie.c

libmtree_test_LDADD =				\
	$(top_builddir)/compat/libcompat.la	\
	$(top_builddir)/libmtree/libmtree.la
