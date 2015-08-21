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

#ifndef _LIBMTREE_MTREE_FILE_H_
#define _LIBMTREE_MTREE_FILE_H_

#include <stdint.h>
#include <stdio.h>

int	 mtree_cksum_fd(int fd, uint32_t *crc);
int	 mtree_cksum_path(const char *path, uint32_t *crc);

char	*mtree_digest_fd(int types, int fd);
char	*mtree_digest_path(int types, const char *path);

int	 mtree_spec_read_spec_file(struct mtree_spec *spec, FILE *fp);
int	 mtree_spec_read_spec_fd(struct mtree_spec *spec, int fd);

int	 mtree_spec_write_file(struct mtree_spec *spec, FILE *fp);
int	 mtree_spec_write_fd(struct mtree_spec *spec, int fd);

int	 mtree_spec_diff_write_file(struct mtree_spec_diff *sd, FILE *fp);
int	 mtree_spec_diff_write_fd(struct mtree_spec_diff *sd, int fd);

#endif /* !_LIBMTREE_MTREE_FILE_H_ */
