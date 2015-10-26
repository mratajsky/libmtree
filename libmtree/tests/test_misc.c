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

#include <inttypes.h>
#include <stdlib.h>

#include "test.h"

#include "libmtree/mtree.h"
#include "libmtree/mtree_private.h"

static const struct test_cleanup_path {
	const char *input;
	const char *path;
	const char *name;
} input_cleanup_paths[] = {
	{ "",			".",		"." },
	{ ".",			".",		"." },
	{ "a",			"./a",		"a" },
	{ "a/b",		"./a/b",	"b" },
	{ "//../../a/b",	"./a/b",	"b" },
	{ "././.././a/./b",	"./a/b",	"b" },
	{ "a/../b", 		"./b",		"b" },
	{ "./a/./../../../b",	"./b",		"b" },
	{ "a/b/././.",		"./a/b",	"b" },
	{ "a/b/../././../../",	"./a/b",	"b" },
	{ NULL, NULL, NULL }
};

static void
test_basic(void)
{

	/*
	 * Check both the standard and non-standard cases of the basic
	 * functions defined in mtree.c.
	 */
	TEST_ASSERT_VALCMP(mtree_entry_type_parse("socket"),
	    MTREE_ENTRY_SOCKET, "0x%08x");
	TEST_ASSERT_VALCMP(mtree_entry_type_parse("invalid"),
	    MTREE_ENTRY_UNKNOWN, "0x%08x");

	TEST_ASSERT_VALCMP(mtree_entry_type_from_mode(S_IFCHR),
	    MTREE_ENTRY_CHAR, "0x%08x");
	TEST_ASSERT_VALCMP(mtree_entry_type_from_mode(S_IFMT),
	    MTREE_ENTRY_UNKNOWN, "0x%08x");
	TEST_ASSERT_VALCMP(mtree_entry_type_from_mode(0),
	    MTREE_ENTRY_UNKNOWN, "0x%08x");

	TEST_ASSERT_STRCMP(mtree_entry_type_string(MTREE_ENTRY_FILE), "file");
	TEST_ASSERT_VALCMP(mtree_entry_type_string(0xF), NULL, "%p");

	TEST_ASSERT_VALCMP(mtree_keyword_parse("uname"),
	    (uint64_t)MTREE_KEYWORD_UNAME, "0x%08lx");
	TEST_ASSERT_VALCMP(mtree_keyword_parse("invalid"), (uint64_t)0, "0x%" PRIx64);

	TEST_ASSERT_STRCMP(mtree_keyword_string(MTREE_KEYWORD_CONTENTS),
	    "contents");
	TEST_ASSERT_VALCMP(mtree_keyword_string(0xF), NULL, "%p");
}

static void
test_atol()
{
	const char	*s;
	const char	*endptr;
	int64_t		 n;

	/* Test conversions. */
	s = "0123";
	n = mtree_atol8(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0123, "%" PRIo64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "01238";
	n = mtree_atol8(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0123, "%" PRIo64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)1, "%zu");

	s = "0123";
	n = mtree_atol10(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)123, "%" PRId64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "0123a";
	n = mtree_atol10(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)123, "%" PRId64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)1, "%zu");

	s = "abc";
	n = mtree_atol16(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0xabc, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "ABC";
	n = mtree_atol16(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0xabc, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");

	s = "123";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)123, "%" PRId64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "0123";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0123, "%" PRIo64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "0xabc";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, (int64_t)0xabc, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");

	/* Test limits. */
	s = "9223372036854775807";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, INT64_MAX, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "9223372036854775808";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, INT64_MAX, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "-9223372036854775808";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, INT64_MIN, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
	s = "-9223372036854775809";
	n = mtree_atol(s, &endptr);
	TEST_ASSERT_VALCMP(n, INT64_MIN, "0x%" PRIx64);
	TEST_ASSERT_VALCMP(strlen(endptr), (size_t)0, "%zu");
}

static void
test_cleanup_path()
{
	const struct test_cleanup_path	*tcp;
	char				*path, *name;
	int				 ret;

	for (tcp = input_cleanup_paths; tcp->input != NULL; tcp++) {
		ret = mtree_cleanup_path(tcp->input, &path, &name);
		TEST_ASSERT_MSG(ret == 0, "tcp->input: \"%s\"", tcp->input);
		if (ret != 0)
			continue;

		TEST_ASSERT_STRCMP(path, tcp->path);
		TEST_ASSERT_STRCMP(name, tcp->name);
		free(path);
		free(name);
	}
}

static void
test_vispath()
{
	const char	*s;
	char		 unvis[64];
	char		*vis;

	s = " \t\n\\#*=?[]";
	vis = mtree_vispath(s, 0);
	TEST_ASSERT_STRCMP(vis, "\\040\\011\\012\\134\\043\\052\\075\\077\\133]");
	strnunvis(unvis, sizeof(unvis), vis);
	TEST_ASSERT_STRCMP(unvis, s);
	free(vis);
	vis = mtree_vispath(s, 1);
	TEST_ASSERT_STRCMP(vis, "\\s\\t\\n\\\\\\#\\*\\=\\?\\[]");
	strnunvis(unvis, sizeof(unvis), vis);
	TEST_ASSERT_STRCMP(unvis, s);
	free(vis);
}

void
test_mtree_misc()
{
	TEST_RUN(test_basic, "mtree_basic");
	TEST_RUN(test_atol, "mtree_atol");
	TEST_RUN(test_cleanup_path, "mtree_cleanup_path");
	TEST_RUN(test_vispath, "mtree_vispath");
}
