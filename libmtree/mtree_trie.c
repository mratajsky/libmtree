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
#include <stdlib.h>
#include <string.h>

#include "mtree.h"
#include "mtree_private.h"

/*
 * mtree_trie_node
 */
struct mtree_trie_node {
	void			*item;
	char			*key;
	size_t			 key_len;
	size_t			 bit;
	struct mtree_trie_node	*left;
	struct mtree_trie_node	*right;
};

static struct mtree_trie_node *create_node(const char *key, void *item);

struct mtree_trie *
mtree_trie_create(mtree_trie_free_fn f)
{
	struct mtree_trie *trie;

	trie = malloc(sizeof(struct mtree_trie));
	if (trie == NULL)
		return (NULL);
	trie->top = create_node(NULL, NULL);
	if (trie->top == NULL) {
		free(trie);
		return (NULL);
	}
	trie->free_fn  = f;
	trie->top->key = "";
	return (trie);
}

static void
free_nodes(struct mtree_trie_node *u, mtree_trie_free_fn f)
{

	if (u->left != NULL && u->left->bit > u->bit)
		free_nodes(u->left, f);
	if (u->right != NULL && u->right->bit > u->bit)
		free_nodes(u->right, f);

	if (f != NULL)
		f(u->item);
	free(u->key);
	free(u);
}

void
mtree_trie_free(struct mtree_trie *trie)
{

	assert(trie != NULL);

	if (trie->top->left != trie->top)
		free_nodes(trie->top->left, trie->free_fn);
	free(trie);
}

static struct mtree_trie_node *
create_node(const char *key, void *item)
{
	struct mtree_trie_node *u;

	u = calloc(1, sizeof(struct mtree_trie_node));
	if (u == NULL)
		return (NULL);
	if (key != NULL) {
		u->key = strdup(key);
		if (u->key == NULL) {
			free(u);
			return (NULL);
		}
		u->key_len = strlen(key);
	}
	u->item = item;
	u->left = u->right = u;
	return (u);
}

/*
 * Get value of the nth bit in the string key.
 */
#define KEY_BIT(key, key_len, n)				\
	((((n) >> 3) >= (key_len))				\
		? 0						\
		: (((key)[(n) >> 3] >> (7 - (n & 7))) & 1))

static struct mtree_trie_node *
find_node(struct mtree_trie *trie, const char *key)
{
	struct mtree_trie_node	*u;
	size_t			 len;
	size_t			 d;

	if (trie->top->left == trie->top)
		return (NULL);

	u   = trie->top->left;
	len = strlen(key);
	do {
		d = u->bit;
		u = KEY_BIT(key, len, d) == 0 ? u->left : u->right;
	} while (u != NULL && u->bit > d);

	return (u);
}

static struct mtree_trie_node *
insert_node(struct mtree_trie_node *u, struct mtree_trie_node *node,
    struct mtree_trie_node *prev)
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
	struct mtree_trie_node	*u;
	struct mtree_trie_node	*node;
	size_t			 bit;

	assert(trie != NULL);
	assert(item != NULL);
	assert(key != NULL && *key != '\0');

	u = find_node(trie, key);
	if (u != NULL && strcmp(u->key, key) == 0) {
		/*
		 * Already in the trie, just update the item.
		 */
		if (trie->free_fn != NULL && u->item != NULL)
			trie->free_fn(u->item);
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
		node->bit  = bit;
		node->left = trie->top;
		trie->top->left = node;
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
		trie->top->left = insert_node(trie->top->left, node, trie->top);
	}
	return (0);
}

static size_t
count_nodes(struct mtree_trie_node *u)
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

	return (trie->top->left == trie->top ? 0 : count_nodes(trie->top->left));
}

/*
 * Find an item in the tree.
 */
void *
mtree_trie_find(struct mtree_trie *trie, const char *key)
{
	struct mtree_trie_node *u;

	assert(trie != NULL);
	assert(key != NULL && *key != '\0');

	u = find_node(trie, key);
	if (u != NULL && strcmp(u->key, key) == 0)
		return (u->item);

	return (NULL);
}
