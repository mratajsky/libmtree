dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])

# =======================================================================
# Versioning
# =======================================================================

m4_define(libmtree_major_version, 0)
m4_define(libmtree_minor_version, 9)
m4_define(libmtree_micro_version, 0)
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
AC_HEADER_ASSERT
AC_HEADER_MAJOR
if test "$ac_cv_header_sys_types_h_makedev" = "yes" -o \
        "$ac_cv_header_sys_mkdev_h" = "yes" -o \
        "$ac_cv_header_sys_sysmacros.h" = "yes"; then
    AC_DEFINE([HAVE_MAKEDEV], [1],
              [Define to 1 if you have the `makedev' function])
    AC_DEFINE([HAVE_MAJOR], [1],
              [Define to 1 if you have the `major' function])
    AC_DEFINE([HAVE_MINOR], [1],
              [Define to 1 if you have the `minor' function])
fi
AC_CHECK_HEADERS([errno.h fcntl.h], [], [
    AC_MSG_ERROR([Required header not found.])
])
AC_SYS_LARGEFILE

LT_PREREQ([2.2])
LT_INIT

# =======================================================================
# Check for required packages
# =======================================================================
PKG_PROG_PKG_CONFIG

# =======================================================================
# Portability checks
# =======================================================================
AC_FUNC_STRERROR_R

AC_CHECK_FUNCS([fpathconf dirfd])

AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([sys/cdefs.h])

# Required by compat
AC_CHECK_FUNCS([getline])

# Provided by compat
AC_CHECK_FUNCS([fgetln])

# Use platform's fparseln if possible, FreeBSD has it in libutil
AC_SEARCH_LIBS([fparseln], [util], [
    AC_DEFINE([HAVE_FPARSELN], [1],
              [Define to 1 if you have the fparseln function])
    if test "$ac_cv_search_fparseln" = "-lutil"; then
        AC_DEFINE(HAVE_FPARSELN_IN_LIBUTIL, [1],
                  [Define to 1 if you have the fparseln function in libutil])
    fi])

# Some systems don't have st_mtim or st_flags
AC_CHECK_MEMBERS([struct stat.st_mtim, struct stat.st_mtime])
AC_CHECK_MEMBERS([struct stat.st_flags])
AC_CHECK_MEMBERS([struct stat.st_rdev])

# BSD only
AC_CHECK_FUNCS([pwcache_userdb pwcache_groupdb])
AC_CHECK_FUNCS([fflagstostr strtofflags])
AC_CHECK_FUNCS([uid_from_user user_from_uid])
AC_CHECK_FUNCS([gid_from_group group_from_gid])

# Digests
AC_CHECK_LIB(md, MD5Init)

# FreeBSD has:
#  md5.h sha.h sha256.h sha512.h ripemd.h (lacks sha384)
# NetBSD/OpenBSD have:
#  md5.h sha1.h sha2.h rmd160.h
AC_CHECK_HEADERS([md5.h sha.h sha1.h sha2.h sha256.h sha512.h ripemd.h rmd160.h])

PKG_CHECK_MODULES([NETTLE], [nettle], have_nettle=yes, have_nettle=no)
if test "x$have_nettle" = "xyes"; then
    AC_DEFINE(HAVE_NETTLE, [], [Define if we have libnettle support])
    AC_SUBST(HAVE_NETTLE)
    AC_SUBST(NETTLE_CFLAGS)
    AC_SUBST(NETTLE_LIBS)
fi

# These are for mtree(8):
AC_CHECK_FUNCS([lchflags lchmod])
AC_CHECK_FUNCS([utimensat])

# =======================================================================
# Compiler options
# =======================================================================
CFLAGS="$CFLAGS -pedantic -Wall -Wextra -Wmissing-declarations -Wmissing-prototypes"
AC_SUBST(CFLAGS)

# =======================================================================
AC_CONFIG_FILES([
    Makefile
    compat/Makefile
    libmtree/Makefile
    libmtree/tests/Makefile
    mtree/Makefile
    data/Makefile
    data/libmtree.pc
    doc/Makefile
])

AC_OUTPUT
