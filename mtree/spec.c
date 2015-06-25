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

#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <libmtree/mtree.h>
#include <libmtree/mtree_file.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if HAVE_NETDB_H
/* For MAXHOSTNAMELEN on some platforms. */
# include <netdb.h>
#endif

#include "mtree.h"

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	256
#endif

static mtree_spec *
create_spec(void)
{
	mtree_spec *spec;
	int roptions;

	spec = mtree_spec_create();
	if (spec == NULL)
		return (NULL);

	roptions = 0;
	if (Lflag)
		roptions |= MTREE_READ_PATH_FOLLOW_SYMLINKS;
	if (xflag)
		roptions |= MTREE_READ_PATH_DONT_CROSS_DEV;

	mtree_spec_set_read_keywords(spec, keywords);
	mtree_spec_set_read_options(spec, roptions);
	return (spec);
}

mtree_spec *
read_spec(FILE *fp)
{
	mtree_spec *spec;

	assert(fp != NULL);

	spec = create_spec();
	if (spec == NULL)
		return (NULL);

	if (mtree_spec_read_file(spec, fp) != 0) {
		mtree_spec_free(spec);
		spec = NULL;
	}
	return (spec);
}

int
compare_spec(FILE *f1, FILE *f2, FILE *fw)
{
	mtree_spec *spec1, *spec2;
	mtree_spec_diff *diff;
	int ret;

	assert(f1 != NULL);
	assert(f2 != NULL);
	assert(fw != NULL);

	spec1 = read_spec(f1);
	if (spec1 == NULL)
		return (-1);
	spec2 = read_spec(f2);
	if (spec2 == NULL) {
		mtree_spec_free(spec1);
		return (-1);
	}

	// XXX should return MISMATCHEXIT on difference?
	diff = mtree_spec_diff_create(spec1, spec2);
	if (diff != NULL)
		ret = mtree_spec_diff_write_file(diff, fw);
	else
		ret = -1;

	mtree_spec_free(spec1);
	mtree_spec_free(spec2);
	return (ret);
}

int
read_write_spec(FILE *fr, FILE *fw, int path_last)
{
	mtree_spec *spec;
	int ret;

	assert(fr != NULL);
	assert(fw != NULL);

	spec = read_spec(fr);
	if (spec == NULL)
		return (-1);

	if (path_last)
		mtree_spec_set_write_format(spec, MTREE_FORMAT_2_0_PATH_LAST);
	else
		mtree_spec_set_write_format(spec, MTREE_FORMAT_2_0);

	ret = mtree_spec_write_file(spec, fw);

	mtree_spec_free(spec);
	return (ret);
}

static void
write_spec_header(FILE *fp, const char *tree)
{
	char host[MAXHOSTNAMELEN + 1];
	const char *user;
	time_t clocktime;

	if (gethostname(host, sizeof(host)) == 0)
		host[sizeof(host) - 1] = '\0';
	else
		strcpy(host, "<unknown>");
	if ((user = getlogin()) == NULL) {
		struct passwd *pw;

		if ((pw = getpwuid(getuid())) != NULL)
			user = pw->pw_name;
	}
	time(&clocktime);

	fprintf(fp,
		"#\t   user: %s\n"
		"#\tmachine: %s\n"
		"#\t   tree: %s\n"
		"#\t   date: %s\n",
		user ? user : "<unknown>",
		host,
		tree ? tree : "<unknown>", ctime(&clocktime));
}

int
write_spec_tree(FILE *fw, const char *tree)
{
	mtree_spec *spec;
	int woptions;
	int ret;

	assert(fw != NULL);
	assert(tree != NULL);

	spec = create_spec();
	if (spec == NULL)
		return (-1);
	if (tree == NULL)
		tree = ".";

	ret = mtree_spec_read_path(spec, tree);
	if (ret == 0) {
		if (!nflag)
			write_spec_header(fw, tree);

		woptions =
		    MTREE_WRITE_USE_SET |
		    MTREE_WRITE_INDENT |
		    MTREE_WRITE_SPLIT_LONG_LINES;
		if (jflag)
			woptions |= MTREE_WRITE_INDENT_LEVEL;
		if (!nflag)
			woptions |= MTREE_WRITE_DIR_COMMENTS;
		if (!bflag)
			woptions |= MTREE_WRITE_DIR_BLANK_LINES;

		mtree_spec_set_write_options(spec, woptions);
		mtree_spec_set_write_format(spec, MTREE_FORMAT_1_0);

		ret = mtree_spec_write_file(spec, fw);
	}
	mtree_spec_free(spec);
	return (ret);
}
