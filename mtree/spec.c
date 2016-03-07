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
#include <mtree.h>
#include <mtree_file.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "local.h"

struct mtree_spec *
create_spec(void)
{
	struct mtree_spec	*spec;
	int			 options;

	spec = mtree_spec_create();
	if (spec == NULL)
		mtree_err("memory allocation error");

	options = MTREE_READ_MERGE;
	if (Sflag)
		options |= MTREE_READ_SORT;
	if (Mflag)
		options |= MTREE_READ_MERGE_DIFFERENT_TYPES;
	if (dflag) {
		options |= MTREE_READ_SKIP_ALL;
		options &= ~MTREE_READ_SKIP_DIR;
	}
	if (Lflag)
		options |= MTREE_READ_PATH_FOLLOW_SYMLINKS;
	if (xflag)
		options |= MTREE_READ_PATH_DONT_CROSS_MOUNT;

	mtree_spec_set_read_path_keywords(spec, keywords);
	mtree_spec_set_read_options(spec, options);
	return (spec);
}

struct mtree_spec *
create_spec_with_default_filter(void)
{
	struct mtree_spec *spec;

	spec = create_spec();
	mtree_spec_set_read_filter(spec, filter_spec, NULL);
	return (spec);
}

int
filter_spec(struct mtree_entry *entry, void *user_data)
{
	const char *name;
	const char *path;

	/* Unused */
	(void)user_data;

	name = mtree_entry_get_name(entry);
	path = mtree_entry_get_path(entry);
	if (check_excludes(name, path))
		return (MTREE_ENTRY_SKIP | MTREE_ENTRY_SKIP_CHILDREN);
	if (!find_only(path))
		return (MTREE_ENTRY_SKIP | MTREE_ENTRY_SKIP_CHILDREN);

	return (MTREE_ENTRY_KEEP);
}

struct mtree_spec *
read_spec(FILE *fp)
{
	struct mtree_spec *spec;

	assert(fp != NULL);

	spec = create_spec();
	if (mtree_spec_read_spec_file(spec, fp) != 0)
		mtree_err("%s", mtree_spec_get_read_error(spec));

	return (spec);
}

int
compare_spec(FILE *f1, FILE *f2, FILE *fw)
{
	struct mtree_spec	*spec1, *spec2;
	struct mtree_spec_diff	*diff;
	int			 ret;

	assert(f1 != NULL);
	assert(f2 != NULL);
	assert(fw != NULL);

	spec1 = read_spec(f1);
	spec2 = read_spec(f2);

	diff = mtree_spec_diff_create(spec1, spec2, MTREE_KEYWORD_MASK_ALL, 0);
	if (diff != NULL) {
		if (mtree_spec_diff_get_different(diff) != NULL)
			ret = MISMATCHEXIT;
		else
			ret = mtree_spec_diff_write_file(diff, fw);
	} else
		ret = -1;

	mtree_spec_free(spec1);
	mtree_spec_free(spec2);
	return (ret);
}

void
read_write_spec(FILE *fr, FILE *fw, int path_last)
{
	struct mtree_spec *spec;

	assert(fr != NULL);
	assert(fw != NULL);

	spec = read_spec(fr);
	if (path_last)
		mtree_spec_set_write_format(spec, MTREE_FORMAT_2_0_PATH_LAST);
	else
		mtree_spec_set_write_format(spec, MTREE_FORMAT_2_0);

	if (mtree_spec_write_file(spec, fw) != 0)
		mtree_err("%s", strerror(errno));

	mtree_spec_free(spec);
}

static void
write_spec_header(FILE *fp, const char *tree)
{
	char		 host[MAXHOSTNAMELEN + 1];
	const char	*user;
	time_t		 clocktime;

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
		user != NULL ? user : "<unknown>",
		host,
		tree != NULL ? tree : "<unknown>", ctime(&clocktime));
}

void
write_spec_tree(FILE *fw, const char *tree)
{
	struct mtree_spec	*spec;
	int			 options;

	assert(fw != NULL);

	spec = create_spec_with_default_filter();
	if (tree == NULL)
		tree = ".";

	if (mtree_spec_read_path(spec, tree) != 0)
		mtree_err("%s", mtree_spec_get_read_error(spec));

	if (!nflag)
		write_spec_header(fw, tree);

	options =
	    MTREE_WRITE_USE_SET |
	    MTREE_WRITE_INDENT |
	    MTREE_WRITE_SPLIT_LONG_LINES;
	if (jflag)
		options |= MTREE_WRITE_INDENT_LEVEL;
	if (!nflag)
		options |= MTREE_WRITE_DIR_COMMENTS;
	if (!bflag)
		options |= MTREE_WRITE_DIR_BLANK_LINES;
	if (flavor == FLAVOR_NETBSD6)
		options |= MTREE_WRITE_ENCODE_CSTYLE;

	mtree_spec_set_write_options(spec, options);
	mtree_spec_set_write_format(spec, MTREE_FORMAT_1_0);

	if (mtree_spec_write_file(spec, fw) != 0)
		mtree_err("%s", strerror(errno));

	mtree_spec_free(spec);
}
