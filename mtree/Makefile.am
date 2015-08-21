AM_CPPFLAGS =			\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/compat	\
	-I$(top_srcdir)/libmtree

bin_PROGRAMS = mtree

mtree_SOURCES =			\
	excludes.c		\
	getid.c			\
	misc.c			\
	only.c			\
	spec.c			\
	verify.c		\
	mtree.c			\
	local.h

mtree_LDADD =					\
	$(top_builddir)/compat/libcompat.la	\
	$(top_builddir)/libmtree/libmtree.la
