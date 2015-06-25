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
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_private.h"

int
mtree_copy_string(char **dst, const char *src)
{

	assert(dst != NULL);
	assert(src != NULL);

	free(*dst);
	if ((*dst = strdup(src)) == NULL)
		return (-1);

	return (0);
}

char *
mtree_get_gname(gid_t gid)
{
	struct group *grent;

	errno = 0;
	grent = getgrgid(gid);
	if (grent != NULL)
		return (strdup(grent->gr_name));

	return (NULL);
}

char *
mtree_get_link(const char *path)
{
	char buf[MAXPATHLEN];
	ssize_t n;

	assert(path != NULL);

	n = readlink(path, buf, sizeof(buf));
	if (n != -1)
		return (strndup(buf, n));

	return (NULL);
}

char *
mtree_get_uname(uid_t uid)
{
	struct passwd *pwent;

	errno = 0;
	pwent = getpwuid(uid);
	if (pwent != NULL)
		return (strdup(pwent->pw_name));

	return (NULL);
}

/*
 * strsvis(3) encodes path, which must not be longer than MAXPATHLEN
 * characters long, and returns a pointer to a static buffer containing
 * the result
 */
char *
mtree_get_vispath(const char *path)
{
	static const char extra[] = {' ', '\t', '\n', '\\', '#', '*', '?', '[', '\0'};
	char pathbuf[4 * MAXPATHLEN + 1];

	assert(path != NULL);

	// TODO: NetBSD uses VIS_CSTYLE
	if (strsvis(pathbuf, path, VIS_OCTAL, extra) == -1)
		return (NULL);

	return (strdup(pathbuf));
}
