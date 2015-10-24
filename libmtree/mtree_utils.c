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

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include "mtree.h"
#include "mtree_private.h"

/* Characters encoded by vis(3). */
static const char vis_extra[] = {
	' ', '\t', '\n', '\\', '#', '*', '=', '?', '[', '\0'
};

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
int64_t
mtree_atol8(const char *p, const char **endptr)
{
	int64_t	l, limit, last_digit_limit;
	int	digit, base;

	assert(p != NULL);

	base = 8;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	l = 0;
	digit = *p - '0';
	while (digit >= 0 && digit < base) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = INT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	if (endptr != NULL)
		*endptr = p;
	return (l);
}

int64_t
mtree_atol10(const char *p, const char **endptr)
{
	int64_t	l, limit, last_digit_limit;
	int	base, digit, sign;

	assert(p != NULL);

	base = 10;
	if (*p == '-') {
		sign = -1;
		limit = ((uint64_t)(INT64_MAX) + 1) / base;
		last_digit_limit = ((uint64_t)(INT64_MAX) + 1) % base;
		++p;
	} else {
		sign = 1;
		limit = INT64_MAX / base;
		last_digit_limit = INT64_MAX % base;
	}

	l = 0;
	digit = *p - '0';
	while (digit >= 0 && digit < base) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			while (*p >= '0' && *p <= '9')
				++p;
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	if (endptr != NULL)
		*endptr = p;

	if (l > limit || (l == limit && digit > last_digit_limit))
		return ((sign < 0) ? INT64_MIN : INT64_MAX);
	else
		return ((sign < 0) ? -l : l);
}

/* Parse a hex digit. */
static int
parsehex(char c)
{

	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - ('a' - 10));
	else if (c >= 'A' && c <= 'F')
		return (c - ('A' - 10));
	else
		return (-1);
}

int64_t
mtree_atol16(const char *p, const char **endptr)
{
	int64_t	l, limit, last_digit_limit;
	int	base, digit, sign;

	assert(p != NULL);

	base = 16;
	if (*p == '-') {
		sign = -1;
		limit = ((uint64_t)(INT64_MAX) + 1) / base;
		last_digit_limit = ((uint64_t)(INT64_MAX) + 1) % base;
		++p;
	} else {
		sign = 1;
		limit = INT64_MAX / base;
		last_digit_limit = INT64_MAX % base;
	}

	l = 0;
	digit = parsehex(*p);
	while (digit >= 0 && digit < base) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			while (parsehex(*p) >= 0)
				++p;
			break;
		}
		l = (l * base) + digit;
		digit = parsehex(*++p);
	}
	if (endptr != NULL)
		*endptr = p;

	if (l > limit || (l == limit && digit > last_digit_limit))
		return ((sign < 0) ? INT64_MIN : INT64_MAX);
	else
		return ((sign < 0) ? -l : l);
}

int64_t
mtree_atol(const char *p, const char **endptr)
{
	if (*p != '0')
		return (mtree_atol10(p, endptr));
	if (p[1] == 'x' || p[1] == 'X') {
		p += 2;
		return (mtree_atol16(p, endptr));
	}
	return (mtree_atol8(p, endptr));
}

int
mtree_cleanup_path(const char *path, char **ppart, char **npart)
{
	char	*p, *n, *dirname;
	char	 buf[MAXPATHLEN];
	size_t	 len;

	len = strlen(path);
	if (len >= sizeof(buf)) {
		errno = ENOBUFS;
		return (-1);
	}
	p = dirname = strcpy(buf, path);

	/* Remove leading '/' and '../' elements. */
	while (*p != '\0') {
		if (p[0] == '/') {
			p++;
			len--;
		} else if (p[0] == '.' && p[1] == '.' && p[2] == '/') {
			p += 3;
			len -= 3;
		} else
			break;
	}
	if (p != dirname) {
		memmove(dirname, p, len+1);
		p = dirname;
	}

	/* Remove "/","/." and "/.." elements from tail. */
	while (len > 0) {
		size_t ll = len;

		if (len > 0 && p[len-1] == '/') {
			p[len-1] = '\0';
			len--;
		}
		if (len > 1 && p[len-2] == '/' && p[len-1] == '.') {
			p[len-2] = '\0';
			len -= 2;
		}
		if (len > 2 && p[len-3] == '/' && p[len-2] == '.' &&
		    p[len-1] == '.') {
			p[len-3] = '\0';
			len -= 3;
		}
		if (ll == len)
			break;
	}
	while (*p != '\0') {
		if (p[0] == '.' && p[1] == '.' && p[2] == '/') {
			strcpy(p, p+3);
		} else if (p[0] == '/') {
			if (p[1] == '/') {
				/* Convert '//' --> '/' */
				strcpy(p, p+1);
			} else if (p[1] == '.' && p[2] == '/') {
				/* Convert '/./' --> '/' */
				strcpy(p, p+2);
			} else if (p[1] == '.' && p[2] == '.' && p[3] == '/') {
				/* Convert 'dir/dir1/../dir2/'
				 *     --> 'dir/dir2/'
				 */
				char *rp = p - 1;
				while (rp >= dirname) {
					if (*rp == '/')
						break;
					--rp;
				}
				if (rp > dirname) {
					strcpy(rp, p+3);
					p = rp;
				} else {
					strcpy(dirname, p+4);
					p = dirname;
				}
			} else
				p++;
		} else
			p++;
	}
	p = dirname;
	len = strlen(p);

	/*
	 * Prefix the path with "./" if it isn't prefix already, empty path
	 * is converted to ".".
	 */
	if (strcmp(dirname, ".") != 0 && strncmp(dirname, "./", 2) != 0) {
		len += 2;
		if (*dirname != '\0')
			len++;
		p = malloc(len);
		p[0] = '.';
		if (*dirname != '\0') {
			p[1] = '/';
			strcpy(p + 2, dirname);
		} else
			strcpy(p + 1, dirname);
	} else
		p = strdup(dirname);

	*ppart = p;

	if ((n = strrchr(p, '/')) != NULL)
		n++;
	if (n == NULL || *n == '\0')
		n = p;
	*npart = strdup(n);
	return (0);
}

/*
 * Free *dst and copy src into *dst. If src is NULL, set *dst to NULL as well.
 */
int
mtree_copy_string(char **dst, const char *src)
{

	assert(dst != NULL);

	free(*dst);
	if (src != NULL) {
		if ((*dst = strdup(src)) == NULL)
			return (-1);
	} else
		*dst = NULL;
	return (0);
}

/*
 * Return path to the current working directory.
 */
char *
mtree_getcwd(void)
{
	char	*wd;
	size_t	 size = 256;

	for (;;) {
		wd = malloc(size);
		if (wd == NULL)
			return (NULL);
		if (getcwd(wd, size) == NULL) {
			if (errno == ERANGE) {
				size <<= 1;
				continue;
			}
			return (NULL);
		} else
			break;
	}
	return (wd);
}

/*
 * Convert group ID into group name.
 */
char *
mtree_gname_from_gid(gid_t gid)
{
#ifdef HAVE_GROUP_FROM_GID
	const char *gname;

	gname = group_from_gid(gid, 1);
	if (gname == NULL)
		return (NULL);
	return (strdup(gname));
#else
	struct group *grent;

	errno = 0;
	grent = getgrgid(gid);
	if (grent == NULL)
		return (NULL);
	return (strdup(grent->gr_name));
#endif
}

char *
mtree_readlink(const char *path)
{
	struct stat	 st;
	char		*link;
	ssize_t		 n;

	if (lstat(path, &st) == -1)
		return (NULL);
	if (!S_ISLNK(st.st_mode)) {
		errno = EINVAL;
		return (NULL);
	}
	link = malloc(st.st_size + 1);
	if (link == NULL)
		return (NULL);

	n = readlink(path, link, st.st_size + 1);
	if (n == -1) {
		free(link);
		return (NULL);
	}
	link[n] = '\0';

	return (link);
}

/*
 * Convert user ID into user name.
 */
char *
mtree_uname_from_uid(uid_t uid)
{
#ifdef HAVE_USER_FROM_UID
	const char *uname;

	uname = user_from_uid(uid, 1);
	if (uname == NULL)
		return (NULL);
	return (strdup(uname));
#else
	struct passwd *pwent;

	errno = 0;
	pwent = getpwuid(uid);
	if (pwent == NULL)
		return (NULL);
	return (strdup(pwent->pw_name));
#endif
}

/*
 * Encode path in vis(3) format.
 */
char *
mtree_vispath(const char *path, int cstyle)
{
	char	*encoded;
	size_t	 len;
	int	 style;

	assert(path != NULL);

	len = 4 * strlen(path) + 1;
	if ((encoded = malloc(len)) == NULL)
		return (NULL);

	if (cstyle)
		style = VIS_CSTYLE;
	else
		style = VIS_OCTAL;
	if (strsnvis(encoded, len, path, style, vis_extra) == -1) {
		free(encoded);
		return (NULL);
	}
	return (encoded);
}
