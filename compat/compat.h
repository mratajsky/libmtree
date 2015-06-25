/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _COMPAT_H_
#define _COMPAT_H_

#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/param.h>
#include <sys/types.h>

#include <stdio.h>
#ifdef HAVE_FPARSELN_IN_LIBUTIL
#include <libutil.h>
#endif

#ifndef MAXPATHLEN
# define MAXPATHLEN		1024
#endif

#ifndef __arraycount
# define __arraycount(x)	(sizeof(x) / sizeof(*(x)))
#endif

#ifndef __BEGIN_DECLS
# ifdef __cplusplus
#  define __BEGIN_DECLS	extern "C" {
# else
#  define __BEGIN_DECLS
# endif
#endif
#ifndef __END_DECLS
# ifdef __cplusplus
#  define __END_DECLS	}
# else
#  define __END_DECLS
# endif
#endif

#ifndef HAVE_FPARSELN
/*
 * fparseln() specific operation flags.
 */
#define FPARSELN_UNESCESC	0x01
#define FPARSELN_UNESCCONT	0x02
#define FPARSELN_UNESCCOMM	0x04
#define FPARSELN_UNESCREST	0x08
#define FPARSELN_UNESCALL	0x0f
#endif /* HAVE_FPARSELN */

/*
 * to select alternate encoding format
 */
#define	VIS_OCTAL	0x0001	/* use octal \ddd format */
#define	VIS_CSTYLE	0x0002	/* use \[nrft0..] where appropiate */

/*
 * to alter set of characters encoded (default is to encode all
 * non-graphic except space, tab, and newline).
 */
#define	VIS_SP		0x0004	/* also encode space */
#define	VIS_TAB		0x0008	/* also encode tab */
#define	VIS_NL		0x0010	/* also encode newline */
#define	VIS_WHITE	(VIS_SP | VIS_TAB | VIS_NL)
#define	VIS_SAFE	0x0020	/* only encode "unsafe" characters */

/*
 * other
 */
#define	VIS_NOSLASH	0x0040	/* inhibit printing '\' */
#define	VIS_HTTP1808	0x0080	/* http-style escape % hex hex */
#define	VIS_HTTPSTYLE	0x0080	/* http-style escape % hex hex */
#define	VIS_MIMESTYLE	0x0100	/* mime-style escape = HEX HEX */
#define	VIS_HTTP1866	0x0200	/* http-style &#num; or &string; */
#define	VIS_NOESCAPE	0x0400	/* don't decode `\' */
#define	_VIS_END	0x0800	/* for unvis */
#define	VIS_GLOB	0x1000	/* encode glob(3) magic characters */
#define	VIS_SHELL	0x2000	/* encode shell special characters [not glob] */
#define	VIS_META	(VIS_WHITE | VIS_GLOB | VIS_SHELL)

/*
 * unvis return codes
 */
#define	UNVIS_VALID	 1	/* character valid */
#define	UNVIS_VALIDPUSH	 2	/* character valid, push back passed char */
#define	UNVIS_NOCHAR	 3	/* valid sequence, no character produced */
#define	UNVIS_SYNBAD	-1	/* unrecognized escape sequence */
#define	UNVIS_ERROR	-2	/* decoder in unknown state (unrecoverable) */

/*
 * unvis flags
 */
#define	UNVIS_END	_VIS_END	/* no more characters */

__BEGIN_DECLS

#ifndef HAVE_FGETLN
char	*fgetln(FILE *fp, size_t *lenp);
#endif
#ifndef HAVE_FPARSELN
char	*fparseln(FILE *, size_t *, size_t *, const char[3], int);
#endif

char	*vis(char *, int, int, int);
char	*nvis(char *, size_t, int, int, int);

char	*svis(char *, int, int, int, const char *);
char	*snvis(char *, size_t, int, int, int, const char *);

int	 strvis(char *, const char *, int);
int	 strnvis(char *, size_t, const char *, int);

int	 strsvis(char *, const char *, int, const char *);
int	 strsnvis(char *, size_t, const char *, int, const char *);

int	 strvisx(char *, const char *, size_t, int);
int	 strnvisx(char *, size_t, const char *, size_t, int);
int 	 strenvisx(char *, size_t, const char *, size_t, int, int *);

int	 strsvisx(char *, const char *, size_t, int, const char *);
int	 strsnvisx(char *, size_t, const char *, size_t, int, const char *);
int	 strsenvisx(char *, size_t, const char *, size_t , int, const char *, int *);

int	 strunvis(char *, const char *);
int	 strnunvis(char *, size_t, const char *);

int	 strunvisx(char *, const char *, int);
int	 strnunvisx(char *, size_t, const char *, int);

int	 unvis(char *, int, int *, int);

__END_DECLS

#endif /* _COMPAT_H_ */
