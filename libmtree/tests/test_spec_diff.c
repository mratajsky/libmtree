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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "libmtree/mtree.h"
#include "libmtree/mtree_private.h"

static void
test_diff(void)
{
	struct mtree_entry	*e;
	struct mtree_entry	*e1, *e2;
	struct mtree_spec	*spec1, *spec2;
	struct mtree_entry	*s1only, *s2only;
	struct mtree_spec_diff	*sd;

	e1 = e2 = NULL;

	/* (Almost) matching entries, one of them has an extra keyword. */
	e = mtree_entry_create("./match");
	mtree_entry_set_type(e, MTREE_ENTRY_FILE);
	mtree_entry_set_keywords(e, MTREE_KEYWORD_TYPE | MTREE_KEYWORD_NOCHANGE, 0);
	e1 = mtree_entry_append(e1, e);

	e = mtree_entry_create("./match");
	mtree_entry_set_type(e, MTREE_ENTRY_FILE);
	e2 = mtree_entry_append(e2, e);

	/* Different types. */
	e = mtree_entry_create("./diff");
	mtree_entry_set_type(e, MTREE_ENTRY_BLOCK);
	mtree_entry_set_uid(e, 123);
	e1 = mtree_entry_append(e1, e);

	e = mtree_entry_create("./diff");
	mtree_entry_set_type(e, MTREE_ENTRY_CHAR);
	mtree_entry_set_uid(e, 123);
	e2 = mtree_entry_append(e2, e);

	e = mtree_entry_create("./s1only");
	e1 = mtree_entry_append(e1, e);

	e = mtree_entry_create("./s2only");
	e2 = mtree_entry_append(e2, e);

	spec1 = mtree_spec_create();
	mtree_spec_set_entries(spec1, e1);
	spec2 = mtree_spec_create();
	mtree_spec_set_entries(spec2, e2);

	sd = mtree_spec_diff_create(spec1, spec2, MTREE_KEYWORD_MASK_ALL, 0);

	s1only = mtree_spec_diff_get_spec1_only(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(s1only), 1UL, "%zu");
	TEST_ASSERT_STRCMP(mtree_entry_get_path(s1only), "./s1only");

	s2only = mtree_spec_diff_get_spec2_only(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(s2only), 1UL, "%zu");
	TEST_ASSERT_STRCMP(mtree_entry_get_path(s2only), "./s2only");

	/* No matching here as the first entry has an extra keywords. */
	e = mtree_spec_diff_get_matching(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(e), 0UL, "%zu");
	e = mtree_spec_diff_get_different(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(e), 4UL, "%zu");

	/* Order should be preserved. */
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./match");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    MTREE_KEYWORD_NOCHANGE, "%ld");
	e = mtree_entry_get_next(e);
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./match");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    0, "%ld");

	e = mtree_entry_get_next(e);
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./diff");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    MTREE_KEYWORD_TYPE, "%ld");
	TEST_ASSERT_VALCMP(mtree_entry_get_type(e), MTREE_ENTRY_BLOCK, "%x");
	e = mtree_entry_get_next(e);
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./diff");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    MTREE_KEYWORD_TYPE, "%ld");
	TEST_ASSERT_VALCMP(mtree_entry_get_type(e), MTREE_ENTRY_CHAR, "%x");

	mtree_spec_diff_free(sd);

	/* This should also match the "./match" entries. */
	sd = mtree_spec_diff_create(spec1, spec2, MTREE_KEYWORD_MASK_ALL,
	    MTREE_SPEC_DIFF_MATCH_EXTRA_KEYWORDS);

	s1only = mtree_spec_diff_get_spec1_only(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(s1only), 1UL, "%zu");
	TEST_ASSERT_STRCMP(mtree_entry_get_path(s1only), "./s1only");

	s2only = mtree_spec_diff_get_spec2_only(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(s2only), 1UL, "%zu");
	TEST_ASSERT_STRCMP(mtree_entry_get_path(s2only), "./s2only");

	e = mtree_spec_diff_get_matching(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(e), 2UL, "%zu");
	/* Order should be preserved. */
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./match");
	e = mtree_entry_get_next(e);
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./match");

	e = mtree_spec_diff_get_different(sd);
	TEST_ASSERT_VALCMP(mtree_entry_count(e), 2UL, "%zu");

	/* Order should be preserved. */
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./diff");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    MTREE_KEYWORD_TYPE, "%ld");
	e = mtree_entry_get_next(e);
	TEST_ASSERT_STRCMP(mtree_entry_get_path(e), "./diff");
	TEST_ASSERT_VALCMP(mtree_entry_get_keywords(e),
	    MTREE_KEYWORD_TYPE, "%ld");

	mtree_spec_diff_free(sd);
	mtree_spec_free(spec1);
	mtree_spec_free(spec2);
}

void
test_spec_diff()
{
	TEST_RUN(test_diff, "mtree_spec_diff");
}
