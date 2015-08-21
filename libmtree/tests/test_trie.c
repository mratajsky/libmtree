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

#define TRIE_STRINGS 10

static void
test_trie_strings(void)
{
	struct mtree_trie	*trie;
	char			 buf[16];
	char			*s;
	size_t			 i;
	int			 ret;

	trie = mtree_trie_create();
	TEST_ASSERT_ERRNO(trie != NULL);
	if (trie == NULL)
		return;
	TEST_ASSERT(mtree_trie_count(trie) == 0);

	for (i = 0; i < TRIE_STRINGS; i++) {
		snprintf(buf, sizeof(buf), "%zu", i);
		s = strdup(buf);
		TEST_ASSERT_ERRNO(s != NULL);
		if (s == NULL)
			continue;
		ret = mtree_trie_insert(trie, buf, s);
		TEST_ASSERT_ERRNO(ret != -1);
		if (ret != -1)
			TEST_ASSERT_MSG(ret == 0, "key: %s", buf);
	}
	TEST_ASSERT(mtree_trie_count(trie) == TRIE_STRINGS);

	/*
	 * Make sure all the items can be found.
	 */
	for (i = 0; i < TRIE_STRINGS + 1; i++) {
		snprintf(buf, sizeof(buf), "%zu", i);
		if (i < TRIE_STRINGS) {
			s = mtree_trie_find(trie, buf);
			TEST_ASSERT_MSG(s != NULL, "key: %s", buf);
		} else
			TEST_ASSERT(mtree_trie_find(trie, buf) == NULL);
	}

	/*
	 * Replace the first item, the function returns 1 on successful
	 * replacement.
	 */
	// XXX leaks the old 0
	ret = mtree_trie_insert(trie, "0", strdup("xy"));
	TEST_ASSERT_ERRNO(ret == 0 || ret == 1);
	if (ret != 1) {
		TEST_ASSERT(ret == 1);
		s = mtree_trie_find(trie, "0");
		TEST_ASSERT(s != NULL);
		if (s != NULL)
			TEST_ASSERT_STRCMP(s, "xy");
	}
	mtree_trie_free(trie, free);
}

void
test_trie()
{
	TEST_RUN(test_trie_strings, "mtree_trie");
}
