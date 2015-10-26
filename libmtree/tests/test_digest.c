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

#include <stdlib.h>
#include <unistd.h>

#include "test.h"

#include "libmtree/mtree.h"
#include "libmtree/mtree_file.h"
#include "libmtree/mtree_private.h"

#define DIGEST_FILE	"/tmp/mtree-test-digest"
#define DIGEST_STR	"test data"

enum {
	DIGEST_MD5,
	DIGEST_RMD160,
	DIGEST_SHA1,
	DIGEST_SHA256,
	DIGEST_SHA384,
	DIGEST_SHA512
};
static int digest_types[] = {
	[DIGEST_MD5]	= MTREE_DIGEST_MD5,
	[DIGEST_RMD160]	= MTREE_DIGEST_RMD160,
	[DIGEST_SHA1]	= MTREE_DIGEST_SHA1,
	[DIGEST_SHA256]	= MTREE_DIGEST_SHA256,
	[DIGEST_SHA384]	= MTREE_DIGEST_SHA384,
	[DIGEST_SHA512]	= MTREE_DIGEST_SHA512
};
static const char *digest_names[] = {
	[DIGEST_MD5]	= "MD5",
	[DIGEST_RMD160]	= "RMD160",
	[DIGEST_SHA1]	= "SHA1",
	[DIGEST_SHA256]	= "SHA256",
	[DIGEST_SHA384]	= "SHA384",
	[DIGEST_SHA512]	= "SHA512"
};
static const char *digest_results[] = {
	[DIGEST_MD5]	= "eb733a00c0c9d336e65691a37ab54293",
	[DIGEST_RMD160]	= "feaf1fb8e0a8cd67d52ac4b437cd0660addd947b",
	[DIGEST_SHA1]	= "f48dd853820860816c75d54d0f584dc863327a7c",
	[DIGEST_SHA256]	= "916f0027a575074ce72a331777c3478d6513f786a591bd892da1a577bf2335f9",
	[DIGEST_SHA384]	= "29901176dc824ac3fd22227677499f02e4e69477ccc501593cc3dc8c6bfef73a08dfdf4a801723c0479b74d6f1abc372",
	[DIGEST_SHA512]	= "0e1e21ecf105ec853d24d728867ad70613c21663a4693074b2a3619c1bd39d66b588c33723bb466c72424e80e3ca63c249078ab347bab9428500e7ee43059d0d"
};

static void
digest_memory(int d)
{
	struct mtree_digest	*digest;
	const char		*result;

	digest = mtree_digest_create(digest_types[d]);
	if (digest == NULL) {
		if (errno == EINVAL) {
			TEST_SKIP("%s not supported", digest_names[d]);
			return;
		}
		TEST_ASSERT_ERRNO(digest != NULL);
		return;
	}
	mtree_digest_update(digest, (unsigned char *)DIGEST_STR, strlen(DIGEST_STR));

	result = mtree_digest_get_result(digest, digest_types[d]);
	TEST_ASSERT_ERRNO(result != NULL);
	if (result != NULL)
		TEST_ASSERT_STRCMP(result, digest_results[d]);

	mtree_digest_free(digest);
}

static void
test_digest_memory(void)
{
	digest_memory(DIGEST_MD5);
	digest_memory(DIGEST_RMD160);
	digest_memory(DIGEST_SHA1);
	digest_memory(DIGEST_SHA256);
	digest_memory(DIGEST_SHA384);
	digest_memory(DIGEST_SHA512);
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
digest_file(int d)
{
	char *result;

	result = mtree_digest_path(digest_types[d], DIGEST_FILE);
	if (result == NULL) {
		if (errno == EINVAL)
			TEST_SKIP("%s not supported", digest_names[d]);
		else
			TEST_ASSERT_ERRNO(result != NULL);
	} else
		TEST_ASSERT_STRCMP(result, digest_results[d]);

	free(result);
}

static void
test_digest_file(void)
{
	if (write_file(DIGEST_FILE, DIGEST_STR) != 0)
		return;

	digest_file(DIGEST_MD5);
	digest_file(DIGEST_RMD160);
	digest_file(DIGEST_SHA1);
	digest_file(DIGEST_SHA256);
	digest_file(DIGEST_SHA384);
	digest_file(DIGEST_SHA512);

	unlink(DIGEST_FILE);
}

void
test_mtree_digest()
{
	TEST_RUN(test_digest_memory, "mtree_digest_memory");
	TEST_RUN(test_digest_file, "mtree_digest_file");
}
