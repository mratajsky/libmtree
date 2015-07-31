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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mtree_private.h"

struct mtree_trie *
mtree_trie_create(void)
{
	struct mtree_trie *trie;

	trie = calloc(1, sizeof(struct mtree_trie));
	if (trie == NULL)
		return (NULL);
	trie->key   = "";
	trie->left  = trie;
	trie->right = trie;
	return (trie);
}

static void
free_nodes(struct mtree_trie *u)
{

	if (u->left != NULL && u->left->bit > u->bit)
		free_nodes(u->left);
	if (u->right != NULL && u->right->bit > u->bit)
		free_nodes(u->right);

	free(u->key);
	free(u);
}

void
mtree_trie_free(struct mtree_trie *trie)
{

	assert(trie != NULL);

	free_nodes(trie->left);
	free(trie);
}

static struct mtree_trie *
create_node(const char *key, void *item)
{
	struct mtree_trie *u;

	u = mtree_trie_create();
	if (u == NULL)
		return (NULL);

	u->item    = item;
	u->key_len = strlen(key);
	u->key     = strdup(key);
	if (u->key == NULL) {
		mtree_trie_free(u);
		return (NULL);
	}
	return (u);
}

/*
 * Get value of the nth bit in the string key.
 */
#define KEY_BIT(key, key_len, n)				\
	((((n) >> 3) >= (key_len))				\
		? 0						\
		: (((key)[(n) >> 3] >> (7 - (n & 7))) & 1))

static struct mtree_trie *
find_node(struct mtree_trie *trie, const char *key)
{
	struct mtree_trie	*u;
	size_t			 len;
	size_t			 d;

	if (trie->left == NULL)
		return (NULL);

	u   = trie->left;
	len = strlen(key);
	do {
		d = u->bit;
		u = KEY_BIT(key, len, d) == 0 ? u->left : u->right;
	} while (u != NULL && u->bit > d);

	return (u);
}

static struct mtree_trie *
insert_node(struct mtree_trie *u, struct mtree_trie *node, struct mtree_trie *prev)
{

	if (u->bit >= node->bit || u->bit <= prev->bit) {
		if (node->left == node)
			node->right = u;
		else
			node->left = u;
		return (node);
	}
	if (KEY_BIT(node->key, node->key_len, u->bit) == 0)
		u->left = insert_node(u->left, node, u);
	else
		u->right = insert_node(u->right, node, u);
	return (u);
}

/*
 * Insert an item to the tree.
 * Return 0 if a new node was created, 1 if a node was updated, or -1 on error.
 */
int
mtree_trie_insert(struct mtree_trie *trie, const char *key, void *item)
{
	struct mtree_trie	*u;
	struct mtree_trie	*node;
	size_t			 bit;

	assert(trie != NULL);
	assert(item != NULL);
	assert(key != NULL && *key != '\0');

	u = find_node(trie, key);
	if (u != NULL && strcmp(u->key, key) == 0) {
		/*
		 * Already in the trie, just update the item.
		 */
		u->item = item;
		return (1);
	}
	if ((node = create_node(key, item)) == NULL)
		return (-1);

	if (u == NULL) {
		/*
		 * Trie is empty, insert to the left under the head.
		 */
		for (bit = 0; KEY_BIT(key, node->key_len, bit) == 0; bit++)
			;
		node->bit   = bit;
		node->left  = trie;
		node->right = node;
		trie->left  = node;
	} else {
		for (bit = 0;; bit++) {
			int kbit = KEY_BIT(key, node->key_len, bit);

			if (kbit != KEY_BIT(u->key, u->key_len, bit)) {
				if (kbit == 0)
					node->right = NULL;
				else
					node->left = NULL;
				node->bit = bit;
				break;
			}
		}
		trie->left = insert_node(trie->left, node, trie);
	}
	return (0);
}

static size_t
count_nodes(struct mtree_trie *u)
{
	size_t count;

	count = 0;
	if (u->left != NULL && u->left->bit > u->bit)
		count += count_nodes(u->left);
	if (u->right != NULL && u->right->bit > u->bit)
		count += count_nodes(u->right);
	return (count + 1);
}

/*
 * Count number of items in the tree.
 */
size_t
mtree_trie_count(struct mtree_trie *trie)
{

	assert(trie != NULL);

	return (trie->left == NULL ? 0 : count_nodes(trie->left));
}

/*
 * Find an item in the tree.
 */
void *
mtree_trie_find(struct mtree_trie *trie, const char *key)
{
	struct mtree_trie *u;

	assert(trie != NULL);
	assert(key != NULL && *key != '\0');

	u = find_node(trie, key);
	if (u != NULL && strcmp(u->key, key) == 0)
		return (u->item);

	return (NULL);
}
