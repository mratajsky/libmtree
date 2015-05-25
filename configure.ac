dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])

# =======================================================================
# Versioning
# =======================================================================

m4_define(libmtree_major_version, 0)
m4_define(libmtree_minor_version, 0)
m4_define(libmtree_micro_version, 1)
m4_define(libmtree_version,
         [libmtree_major_version.libmtree_minor_version.libmtree_micro_version])

AC_INIT([libmtree],
        [libmtree_version],
        [http://www.freebsd.org])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=0:0:0

AC_DEFINE(LIBMTREE_MAJOR_VERSION, libmtree_major_version, [libmtree major version])
AC_DEFINE(LIBMTREE_MINOR_VERSION, libmtree_minor_version, [libmtree minor version])
AC_DEFINE(LIBMTREE_MICRO_VERSION, libmtree_micro_version, [libmtree micro version])

AC_SUBST(LIBMTREE_MAJOR_VERSION, libmtree_major_version)
AC_SUBST(LIBMTREE_MINOR_VERSION, libmtree_minor_version)
AC_SUBST(LIBMTREE_MICRO_VERSION, libmtree_micro_version)
AC_SUBST(LIBMTREE_VERSION,       libmtree_version)

AC_SUBST(LT_VERSION)

# =======================================================================
# Initialize and do basic checks
# =======================================================================

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([libmtree/mtree.c])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz foreign])

# Support silent build rules with automake >= 1.11
# Disable by either passing --disable-silent-rules to configure or passing
# V=1 to make
m4_ifdef([AM_SILENT_RULES],
         [AM_SILENT_RULES([yes])],
         [AM_DEFAULT_VERBOSITY=1 AC_SUBST(AM_DEFAULT_VERBOSITY)])

# Checks for required programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_STDC
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([string.h], [], [
    AC_MSG_ERROR([Required header not found.])
])

LT_PREREQ([2.2])
LT_INIT

# =======================================================================
# Check for required packages
# =======================================================================
PKG_PROG_PKG_CONFIG

# =======================================================================
# Compiler options
# =======================================================================
CFLAGS="$CFLAGS -pedantic -Wall -Wextra -Wmissing-declarations -Wmissing-prototypes"
AC_SUBST(CFLAGS)

# =======================================================================
AC_CONFIG_FILES([
    Makefile
    libmtree/Makefile
    mtree/Makefile
    data/Makefile
    data/libmtree.pc
    doc/Makefile
])

AC_OUTPUT
