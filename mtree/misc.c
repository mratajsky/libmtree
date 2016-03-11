/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#include <grp.h>
#include <mtree.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "local.h"

long
parse_keyword(const char *name)
{
	long keyword;

	assert(name != NULL);

	if (strcmp(name, "all") == 0)
		return (MTREE_KEYWORD_MASK_ALL &
		    ~(MTREE_KEYWORD_IGNORE |
		      MTREE_KEYWORD_OPTIONAL |
		      MTREE_KEYWORD_NOCHANGE));

	keyword = mtree_keyword_parse(name);
	if (keyword == 0)
		mtree_err("unknown keyword `%s'", name);

	return (keyword);
}

static void
add_tag(taglist *list, char *elem)
{

#define	TAG_CHUNK 20

	if ((list->count % TAG_CHUNK) == 0) {
		char **new;

		new = realloc(list->list, (list->count + TAG_CHUNK) * sizeof(char *));
		if (new == NULL)
			mtree_err("memory allocation error");
		list->list = new;
	}
	list->list[list->count] = elem;
	list->count++;
}

void
parse_tags(taglist *list, char *args)
{
	char *p;

	if (args == NULL) {
		add_tag(list, NULL);
		return;
	}
	while ((p = strsep(&args, ",")) != NULL) {
		char	*e;
		int	 len;

		if (*p == '\0')
			continue;
		len = strlen(p) + 3;	/* "," + p + ",\0" */
		if ((e = malloc(len)) == NULL)
			mtree_err("memory allocation error");

		snprintf(e, len, ",%s,", p);
		add_tag(list, e);
	}
}

/*
 * matchtags
 *	returns 0 if there's a match from the exclude list in the node's tags,
 *	or there's an include list and no match.
 *	return 1 otherwise.
 */
int
match_tags(const char *tags)
{
	int i;

	if (tags != NULL) {
		for (i = 0; i < exclude_tags.count; i++)
			if (strstr(tags, exclude_tags.list[i]))
				break;
		if (i < exclude_tags.count)
			return (0);

		for (i = 0; i < include_tags.count; i++)
			if (strstr(tags, include_tags.list[i]))
				break;
		if (i > 0 && i == include_tags.count)
			return (0);
	} else if (include_tags.count > 0)
		return (0);

	return (1);
}

char *
convert_flags_to_string(uint32_t flags, const char *def)
{
	char *str = NULL;

#ifdef HAVE_FFLAGSTOSTR
	str = fflagstostr((u_long) flags);
	if (str == NULL)
		mtree_err("fflagstostr: %s", strerror(errno));
	if (str[0] == '\0') {
		free(str);
		if (def != NULL) {
			str = strdup(def);
			if (str == NULL)
				mtree_err("memory allocation error");
		} else
			str = NULL;
	}
#else
	(void)flags;
	(void)def;
#endif
	return (str);
}

int
convert_string_to_flags(const char *s, uint32_t *flags)
{

#ifdef HAVE_STRTOFFLAGS
	char     *tmp = (char *) s;
	u_long	  fl;

	if (s == NULL || strcmp(s, "none") == 0) {
		*flags = 0;
		return (0);
	}
	if (strtofflags(&tmp, &fl, NULL) == 0) {
		*flags = (uint32_t) fl;
		return (0);
	} else
		return (-1);
#else
	(void)s;
	(void)flags;
	return (0);
#endif
}

int
convert_gname_to_gid(const char *gname, gid_t *gid)
{
#ifdef HAVE_GID_FROM_GROUP
	return (gid_from_group(gname, gid));
#else
	struct group *gr;

	errno = 0;
	if ((gr = getgrnam(gname)) == NULL) {
		if (errno != 0)
			mtree_err("getgrnam: %s", strerror(errno));
		return (-1);
	} else {
		*gid = gr->gr_gid;
		return (0);
	}
#endif
}

int
convert_uname_to_uid(const char *uname, uid_t *uid)
{
#ifdef HAVE_UID_FROM_USER
	return (uid_from_user(uname, uid));
#else
	struct passwd *pw;

	errno = 0;
	if ((pw = getpwnam(uname)) == NULL) {
		if (errno != 0)
			mtree_err("getpwnam: %s", strerror(errno));
		return (-1);
	} else {
		*uid = pw->pw_uid;
		return (0);
	}
#endif
}
