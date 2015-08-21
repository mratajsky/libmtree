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
#include <unistd.h>

#include "test.h"

#include "libmtree/mtree.h"
#include "libmtree/mtree_file.h"
#include "libmtree/mtree_private.h"

#define CKSUM_TOTAL	710351703U
#define CKSUM_FILE	"/tmp/mtree-test-cksum"

static struct test_cksum {
	const char	*str;
	uint32_t	 cksum;
} cksums[] = {
	{ "test data1", 32275850UL },
	{ "test data2", 1929847385UL },
};

static void
test_cksum_memory(void)
{
	struct mtree_cksum	*cksum;
	uint32_t		 crc;

	cksum = mtree_cksum_create(MTREE_CKSUM_DEFAULT_INIT);
	TEST_ASSERT_ERRNO(cksum != NULL);
	if (cksum == NULL)
		return;

	mtree_cksum_update(cksum, (const unsigned char *) cksums[0].str,
	    strlen(cksums[0].str));

	crc = mtree_cksum_get_result(cksum);
	TEST_ASSERT_VALCMP(crc, cksums[0].cksum, "%u");

	/*
	 * Use the result as intermediate value and include another piece
	 * piece data.
	 */
	mtree_cksum_reset(cksum, ~crc);
	mtree_cksum_update(cksum, (const unsigned char *) cksums[1].str,
	    strlen(cksums[1].str));

	crc = mtree_cksum_get_result(cksum);
	TEST_ASSERT_VALCMP(crc, CKSUM_TOTAL, "%u");

	mtree_cksum_free(cksum);
}

static int
write_file(const char *path, const char *str)
{
	FILE	*fp;
	int	 ret;

	fp = fopen(path, "w");
	TEST_ASSERT_ERRNO(fp != NULL);
	if (fp == NULL)
		return (-1);

	ret = fputs(str, fp);
	TEST_ASSERT_ERRNO(ret != EOF);
	if (ret == EOF) {
		fclose(fp);
		goto done;
	}
	ret = fclose(fp);
	TEST_ASSERT_ERRNO(ret != EOF);
done:
	if (ret == EOF) {
		unlink(path);
		ret = -1;
	} else
		ret = 0;
	return (ret);
}

static void
test_cksum_file(void)
{
	uint32_t	crc;
	int		ret;

	if (write_file(CKSUM_FILE, cksums[0].str) != 0)
		return;

	ret = mtree_cksum_path(CKSUM_FILE, &crc);
	TEST_ASSERT_ERRNO(ret == 0);
	if (ret == 0)
		TEST_ASSERT_VALCMP(crc, cksums[0].cksum, "%u");

	unlink(CKSUM_FILE);
}

void
test_cksum()
{
	TEST_RUN(test_cksum_memory, "mtree_cksum_memory");
	TEST_RUN(test_cksum_file, "mtree_cksum_file");
}
