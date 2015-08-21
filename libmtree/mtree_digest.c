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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"

/*
 * Sizes of resulting byte arrays for various digest types.
 */
#define DIGEST_SIZE_MD5		16
#define DIGEST_SIZE_SHA1	20
#define DIGEST_SIZE_SHA256	32
#define DIGEST_SIZE_SHA384	48
#define DIGEST_SIZE_SHA512	64
#define DIGEST_SIZE_RMD160	20
#define DIGEST_SIZE_MAX		DIGEST_SIZE_SHA512

/* =========================================================================
 * MD5
 * ========================================================================= */
#if defined(HAVE_MD5_H)
#  include <md5.h>
#  define MTREE_MD5_INIT(ctxp)			MD5Init(ctxp)
#  define MTREE_MD5_FILE(path)			MD5File(path, NULL)
#  define MTREE_MD5_UPDATE(ctxp, data, len)	MD5Update(ctxp, data, len)
#  define MTREE_MD5_FINAL(ctxp, out)		MD5Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  include <nettle/md5.h>
#  define MTREE_MD5_INIT(ctxp)			md5_init(ctxp)
#  define MTREE_MD5_UPDATE(ctxp, data, len)	md5_update(ctxp, len, data)
#  define MTREE_MD5_FINAL(ctxp, out)		md5_digest(ctxp, DIGEST_SIZE_MD5, out)
#endif

/* =========================================================================
 * SHA1
 * ========================================================================= */
#if defined(HAVE_SHA_H)
#  include <sha.h>
#  define MTREE_SHA1_INIT(ctxp)			SHA_Init(ctxp)
#  define MTREE_SHA1_FILE(path)			SHA_File(path, NULL)
#  define MTREE_SHA1_UPDATE(ctxp, data, len)	SHA_Update(ctxp, data, len)
#  define MTREE_SHA1_FINAL(ctxp, out)		SHA_Final(out, ctxp)
#elif defined(HAVE_SHA1_H)
#  include <sha1.h>
#  define MTREE_SHA1_INIT(ctxp)			SHA1Init(ctxp)
#  define MTREE_SHA1_FILE(path)			SHA1File(path, NULL)
#  define MTREE_SHA1_UPDATE(ctxp, data, len)	SHA1Update(ctxp, data, len)
#  define MTREE_SHA1_FINAL(ctxp, out)		SHA1Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  include <nettle/sha1.h>
#  define MTREE_SHA1_INIT(ctxp)			sha1_init(ctxp)
#  define MTREE_SHA1_UPDATE(ctxp, data, len)	sha1_update(ctxp, len, data)
#  define MTREE_SHA1_FINAL(ctxp, out)		sha1_digest(ctxp, DIGEST_SIZE_SHA1, out)
#endif

/* =========================================================================
 * SHA256, SHA384, SHA512
 * =========================================================================
 * NetBSD and nettle provide support for SHA256, SHA384 and SHA512 in sha2.h.
 *
 * FreeBSD provides SHA256 and SHA512 in sha256.h and sha512.h, support for
 * SHA384 may still be gained through nettle.
 */
#if defined(HAVE_SHA2_H)
#  include <sha2.h>
#else
#  if defined(HAVE_SHA256_H)
#    include <sha256.h>
#  endif
#  if defined(HAVE_SHA512_H)
#    include <sha512.h>
#  endif
#  if defined(HAVE_NETTLE)
#    include <nettle/sha2.h>
#  endif
#endif

#if defined(HAVE_SHA2_H)
#  define MTREE_SHA384_INIT(ctxp)		SHA384_Init(ctxp)
#  define MTREE_SHA384_FILE(path)		SHA384_File(path, NULL)
#  define MTREE_SHA384_UPDATE(ctxp, data, len)	SHA384_Update(ctxp, data, len)
#  define MTREE_SHA384_FINAL(ctxp, out)		SHA384_Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  define MTREE_SHA384_INIT(ctxp)		sha384_init(ctxp)
#  define MTREE_SHA384_UPDATE(ctxp, data, len)	sha384_update(ctxp, len, data)
#  define MTREE_SHA384_FINAL(ctxp, out)		sha384_digest(ctxp, DIGEST_SIZE_SHA384, out)
#endif

#if defined(HAVE_SHA2_H) || defined(HAVE_SHA256_H)
#  define MTREE_SHA256_INIT(ctxp)		SHA256_Init(ctxp)
#  define MTREE_SHA256_FILE(path)		SHA256_File(path, NULL)
#  define MTREE_SHA256_UPDATE(ctxp, data, len)	SHA256_Update(ctxp, data, len)
#  define MTREE_SHA256_FINAL(ctxp, out)		SHA256_Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  define MTREE_SHA256_INIT(ctxp)		sha256_init(ctxp)
#  define MTREE_SHA256_UPDATE(ctxp, data, len)	sha256_update(ctxp, len, data)
#  define MTREE_SHA256_FINAL(ctxp, out)		sha256_digest(ctxp, DIGEST_SIZE_SHA256, out)
#endif

#if defined(HAVE_SHA2_H) || defined(HAVE_SHA512_H)
#  define MTREE_SHA512_INIT(ctxp)		SHA512_Init(ctxp)
#  define MTREE_SHA512_FILE(path)		SHA512_File(path, NULL)
#  define MTREE_SHA512_UPDATE(ctxp, data, len)	SHA512_Update(ctxp, data, len)
#  define MTREE_SHA512_FINAL(ctxp, out)		SHA512_Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  define MTREE_SHA512_INIT(ctxp)		sha512_init(ctxp)
#  define MTREE_SHA512_UPDATE(ctxp, data, len)	sha512_update(ctxp, len, data)
#  define MTREE_SHA512_FINAL(ctxp, out)		sha512_digest(ctxp, DIGEST_SIZE_SHA512, out)
#endif

/* =========================================================================
 * RMD160
 * ========================================================================= */
#if defined(HAVE_RIPEMD_H)
#  include <ripemd.h>
#  define MTREE_RMD160_INIT(ctxp)		RIPEMD160_Init(ctxp)
#  define MTREE_RMD160_FILE(path)		RIPEMD160_File(path, NULL)
#  define MTREE_RMD160_UPDATE(ctxp, data, len)	RIPEMD160_Update(ctxp, data, len)
#  define MTREE_RMD160_FINAL(ctxp, out)		RIPEMD160_Final(out, ctxp)
#elif defined(HAVE_RMD160_H)
#  include <rmd160.h>
#  define MTREE_RMD160_INIT(ctxp)		RMD160Init(ctxp)
#  define MTREE_RMD160_FILE(path)		RMD160File(path, NULL)
#  define MTREE_RMD160_UPDATE(ctxp, data, len)	RMD160Update(ctxp, data, len)
#  define MTREE_RMD160_FINAL(ctxp, out)		RMD160Final(out, ctxp)
#elif defined(HAVE_NETTLE)
#  include <nettle/ripemd160.h>
#  define MTREE_RMD160_INIT(ctxp)		ripemd160_init(ctxp)
#  define MTREE_RMD160_UPDATE(ctxp, data, len)	ripemd160_update(ctxp, len, data)
#  define MTREE_RMD160_FINAL(ctxp, out)		ripemd160_digest(ctxp, DIGEST_SIZE_RMD160, out)
#endif

/*
 * mtree_digest
 */
struct mtree_digest {
	int				 types;
	int				 done;
	struct {
		char			 *md5;
		char			 *sha1;
		char			 *sha256;
		char			 *sha384;
		char			 *sha512;
		char			 *rmd160;
	} result;

	struct {
#if defined(HAVE_MD5_H)
		MD5_CTX			 md5;
#elif defined(HAVE_NETTLE)
		struct md5_ctx		 md5;
#endif
#if defined(HAVE_SHA_H)
		SHA_CTX			 sha1;
#elif defined(HAVE_SHA1_H)
		SHA1_CTX		 sha1;
#elif defined(HAVE_NETTLE)
		struct sha1_ctx		 sha1;
#endif
#if defined(HAVE_SHA2_H) || defined(HAVE_SHA256_H)
		SHA256_CTX		 sha256;
#elif defined(HAVE_NETTLE)
		struct sha256_ctx	 sha256;
#endif
#if defined(HAVE_SHA2_H)
		SHA384_CTX		 sha384;
#elif defined(HAVE_NETTLE)
		struct sha384_ctx	 sha384;
#endif
#if defined(HAVE_SHA2_H) || defined(HAVE_SHA512_H)
		SHA512_CTX		 sha512;
#elif defined(HAVE_NETTLE)
		struct sha512_ctx	 sha512;
#endif
#if defined(HAVE_RIPEMD_H)
		RIPEMD160_CTX		 rmd160;
#elif defined(HAVE_RMD160_H)
		RMD160_CTX		 rmd160;
#elif defined(HAVE_NETTLE)
		struct ripemd160_ctx	 rmd160;
#endif
		/*
		 * Avoid potential compilation error when no digests are
		 * available.
		 */
		char			 unused;
	} ctx;
};

/*
 * Initialize all the requested digests.
 */
static void
digest_init(struct mtree_digest *digest)
{

#ifdef MTREE_MD5_INIT
	if (digest->types & MTREE_DIGEST_MD5)
		MTREE_MD5_INIT(&digest->ctx.md5);
#endif
#ifdef MTREE_SHA1_INIT
	if (digest->types & MTREE_DIGEST_SHA1)
		MTREE_SHA1_INIT(&digest->ctx.sha1);
#endif
#ifdef MTREE_SHA256_INIT
	if (digest->types & MTREE_DIGEST_SHA256)
		MTREE_SHA256_INIT(&digest->ctx.sha256);
#endif
#ifdef MTREE_SHA384_INIT
	if (digest->types & MTREE_DIGEST_SHA384)
		MTREE_SHA384_INIT(&digest->ctx.sha384);
#endif
#ifdef MTREE_SHA512_INIT
	if (digest->types & MTREE_DIGEST_SHA512)
		MTREE_SHA512_INIT(&digest->ctx.sha512);
#endif
#ifdef MTREE_RMD160_INIT
	if (digest->types & MTREE_DIGEST_RMD160)
		MTREE_RMD160_INIT(&digest->ctx.rmd160);
#endif
}

/*
 * Create a new mtree_digest and initialize the digest's context.
 */
struct mtree_digest *
mtree_digest_create(int types)
{
	struct mtree_digest *digest;

	digest = calloc(1, sizeof(struct mtree_digest));
	if (digest != NULL) {
		digest->types = mtree_digest_get_available_types() & types;
		digest_init(digest);
	}
	return (digest);
}

/*
 * Free the given mtree_digest.
 */
void
mtree_digest_free(struct mtree_digest *digest)
{

	assert(digest != NULL);

#ifdef MTREE_MD5_INIT
	free(digest->result.md5);
#endif
#ifdef MTREE_SHA1_INIT
	free(digest->result.sha1);
#endif
#ifdef MTREE_SHA256_INIT
	free(digest->result.sha256);
#endif
#ifdef MTREE_SHA384_INIT
	free(digest->result.sha384);
#endif
#ifdef MTREE_SHA512_INIT
	free(digest->result.sha512);
#endif
#ifdef MTREE_RMD160_INIT
	free(digest->result.rmd160);
#endif
	free(digest);
}

/*
 * Reset the given mtree_digest to its initial state.
 */
void
mtree_digest_reset(struct mtree_digest *digest)
{

	assert(digest != NULL);

#ifdef MTREE_MD5_INIT
	free(digest->result.md5);
	digest->result.md5 = NULL;
#endif
#ifdef MTREE_SHA1_INIT
	free(digest->result.sha1);
	digest->result.sha1 = NULL;
#endif
#ifdef MTREE_SHA256_INIT
	free(digest->result.sha256);
	digest->result.sha256 = NULL;
#endif
#ifdef MTREE_SHA384_INIT
	free(digest->result.sha384);
	digest->result.sha384 = NULL;
#endif
#ifdef MTREE_SHA512_INIT
	free(digest->result.sha512);
	digest->result.sha512 = NULL;
#endif
#ifdef MTREE_RMD160_INIT
	free(digest->result.rmd160);
	digest->result.rmd160 = NULL;
#endif
	digest_init(digest);
}

/*
 * Get types of the given mtree_digest.
 */
int
mtree_digest_get_types(struct mtree_digest *digest)
{

	assert(digest != NULL);

	return (digest->types);
}

/*
 * Get all the supported digest types.
 */
int
mtree_digest_get_available_types(void)
{
	int types = 0;

#ifdef MTREE_MD5_INIT
	types |= MTREE_DIGEST_MD5;
#endif
#ifdef MTREE_SHA1_INIT
	types |= MTREE_DIGEST_SHA1;
#endif
#ifdef MTREE_SHA256_INIT
	types |= MTREE_DIGEST_SHA256;
#endif
#ifdef MTREE_SHA384_INIT
	types |= MTREE_DIGEST_SHA384;
#endif
#ifdef MTREE_SHA512_INIT
	types |= MTREE_DIGEST_SHA512;
#endif
#ifdef MTREE_RMD160_INIT
	types |= MTREE_DIGEST_RMD160;
#endif
	return (types);
}

/*
 * Update the digest with the given data.
 */
void
mtree_digest_update(struct mtree_digest *digest, const unsigned char *data,
    size_t len)
{

	assert(digest != NULL);

	if (len == 0)
		return;

	assert(data != NULL);
#ifdef MTREE_MD5_UPDATE
	if (digest->types & MTREE_DIGEST_MD5 &&
	    digest->result.md5 == NULL)
		MTREE_MD5_UPDATE(&digest->ctx.md5, data, len);
#endif
#ifdef MTREE_SHA1_UPDATE
	if (digest->types & MTREE_DIGEST_SHA1 &&
	    digest->result.sha1 == NULL)
		MTREE_SHA1_UPDATE(&digest->ctx.sha1, data, len);
#endif
#ifdef MTREE_SHA256_UPDATE
	if (digest->types & MTREE_DIGEST_SHA256 &&
	    digest->result.sha256 == NULL)
		MTREE_SHA256_UPDATE(&digest->ctx.sha256, data, len);
#endif
#ifdef MTREE_SHA384_UPDATE
	if (digest->types & MTREE_DIGEST_SHA384 &&
	    digest->result.sha384 == NULL)
		MTREE_SHA384_UPDATE(&digest->ctx.sha384, data, len);
#endif
#ifdef MTREE_SHA512_UPDATE
	if (digest->types & MTREE_DIGEST_SHA512 &&
	    digest->result.sha512 == NULL)
		MTREE_SHA512_UPDATE(&digest->ctx.sha512, data, len);
#endif
#ifdef MTREE_RMD160_UPDATE
	if (digest->types & MTREE_DIGEST_RMD160 &&
	    digest->result.rmd160 == NULL)
		MTREE_RMD160_UPDATE(&digest->ctx.rmd160, data, len);
#endif
}

/*
 * Convert the byte sequence to a string of hexadecimal numbers.
 */
static char *
digest_result(unsigned char *digest, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	char	*s;
	size_t	 i;

	s = malloc(2 * len + 1);
	if (s != NULL) {
		for (i = 0; i < len; i++) {
			s[i + i] = hex[digest[i] >> 4];
			s[i + i + 1] = hex[digest[i] & 0x0F];
		}
		s[i + i] = '\0';
	}
	return (s);
}

/*
 * Get the resulting digest of the given type.
 */
const char *
mtree_digest_get_result(struct mtree_digest *digest, int type)
{
	unsigned char bytes[DIGEST_SIZE_MAX];

	assert(digest != NULL);

	switch (type) {
#ifdef MTREE_MD5_FINAL
	case MTREE_DIGEST_MD5:
		if (digest->result.md5 == NULL) {
			MTREE_MD5_FINAL(&digest->ctx.md5, bytes);
			digest->result.md5 = digest_result(bytes,
			    DIGEST_SIZE_MD5);
		}
		return (digest->result.md5);
#endif
#ifdef MTREE_SHA1_FINAL
	case MTREE_DIGEST_SHA1:
		if (digest->result.sha1 == NULL) {
			MTREE_SHA1_FINAL(&digest->ctx.sha1, bytes);
			digest->result.sha1 = digest_result(bytes,
			    DIGEST_SIZE_SHA1);
		}
		return (digest->result.sha1);
#endif
#ifdef MTREE_SHA256_FINAL
	case MTREE_DIGEST_SHA256:
		if (digest->result.sha256 == NULL) {
			MTREE_SHA256_FINAL(&digest->ctx.sha256, bytes);
			digest->result.sha256 = digest_result(bytes,
			    DIGEST_SIZE_SHA256);
		}
		return (digest->result.sha256);
#endif
#ifdef MTREE_SHA384_FINAL
	case MTREE_DIGEST_SHA384:
		if (digest->result.sha384 == NULL) {
			MTREE_SHA384_FINAL(&digest->ctx.sha384, bytes);
			digest->result.sha384 = digest_result(bytes,
			    DIGEST_SIZE_SHA384);
		}
		return (digest->result.sha384);
#endif
#ifdef MTREE_SHA512_FINAL
	case MTREE_DIGEST_SHA512:
		if (digest->result.sha512 == NULL) {
			MTREE_SHA512_FINAL(&digest->ctx.sha512, bytes);
			digest->result.sha512 = digest_result(bytes,
			    DIGEST_SIZE_SHA512);
		}
		return (digest->result.sha512);
#endif
#ifdef MTREE_RMD160_FINAL
	case MTREE_DIGEST_RMD160:
		if (digest->result.rmd160 == NULL) {
			MTREE_RMD160_FINAL(&digest->ctx.rmd160, bytes);
			digest->result.rmd160 = digest_result(bytes,
			    DIGEST_SIZE_RMD160);
		}
		return (digest->result.rmd160);
#endif
	default:
		errno = EINVAL;
		return (NULL);
	}
}

/*
 * Calculate digest of bytes read from the given file descriptor.
 */
char *
mtree_digest_fd(int type, int fd)
{
	struct mtree_digest	*digest;
	unsigned char		 buf[16 * 1024];
	char			*result;
	int			 n;

	/*
	 * Make sure the requested type is available and that only one
	 * type is selected.
	 */
	if (type == 0 ||
	    (type & ~(1 << (ffs(type) - 1))) != 0 ||
	    (type & mtree_digest_get_available_types()) == 0) {
		errno = EINVAL;
		return (NULL);
	}
	digest = mtree_digest_create(type);
	if (digest == NULL)
		return (NULL);

	while ((n = read(fd, buf, sizeof(buf))) > 0)
		mtree_digest_update(digest, buf, n);

	result = NULL;
	if (n == 0) {
		const char *r;

		r = mtree_digest_get_result(digest, type);
		if (r != NULL)
			result = strdup(r);
	}
	mtree_digest_free(digest);
	return (result);
}

/*
 * Calculate digest of bytes read from the given file path.
 */
char *
mtree_digest_path(int type, const char *path)
{
	char	*result;
	int	 fd;

	assert(path != NULL);

	/*
	 * Make sure type is supported, the "FILE" function may not be
	 * available for all supported types, but "INIT" has to be.
	 */
	switch (type) {
#ifdef MTREE_MD5_INIT
	case MTREE_DIGEST_MD5:
#ifdef MTREE_MD5_FILE
		return (MTREE_MD5_FILE(path));
#endif
		break;
#endif
#ifdef MTREE_SHA1_INIT
	case MTREE_DIGEST_SHA1:
#ifdef MTREE_SHA1_FILE
		return (MTREE_SHA1_FILE(path));
#endif
		break;
#endif
#ifdef MTREE_SHA256_INIT
	case MTREE_DIGEST_SHA256:
#ifdef MTREE_SHA256_FILE
		return (MTREE_SHA256_FILE(path));
#endif
		break;
#endif
#ifdef MTREE_SHA384_INIT
	case MTREE_DIGEST_SHA384:
#ifdef MTREE_SHA384_FILE
		return (MTREE_SHA384_FILE(path));
#endif
		break;
#endif
#ifdef MTREE_SHA512_INIT
	case MTREE_DIGEST_SHA512:
#ifdef MTREE_SHA512_FILE
		return (MTREE_SHA512_FILE(path));
#endif
		break;
#endif
#ifdef MTREE_RMD160_INIT
	case MTREE_DIGEST_RMD160:
#ifdef MTREE_RMD160_FILE
		return (MTREE_RMD160_FILE(path));
#endif
		break;
#endif
	default:
		errno = EINVAL;
		return (NULL);
	}
	/*
	 * Digest type is supported, but there is no "FILE" function available
	 * for the type.
	 */
	result = NULL;
	fd = open(path, O_RDONLY);
	if (fd != -1) {
		result = mtree_digest_fd(type, fd);
		if (result == NULL) {
			int err = errno;

			close(fd);
			errno = err;
		} else
			close(fd);
	}
	return (result);
}
