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

#ifndef _TEST_H_
#define _TEST_H_

extern int tests;
extern int tests_failed;
extern int tests_skipped;
extern int tests_current;
extern int tests_current_failed;
extern int tests_current_skipped;

/*
 * Test functions.
 */
void test_cksum(void);
void test_digest(void);
void test_misc(void);
void test_spec_diff(void);
void test_trie(void);

/*
 * Run the test function with the given name.
 */
#define TEST_RUN(func, name) do {					\
	printf("Testing %s... ", name);					\
	tests_current = 0;						\
	tests_current_failed = 0;					\
	tests_current_skipped = 0;					\
	func();								\
	if (tests_current_failed != 0 || tests_current_skipped != 0)	\
		printf("\t");						\
	if (tests_current_failed == 0)					\
		printf("PASS (%d tests passed", tests_current);		\
	else								\
		printf("FAIL (%d out of %d tests failed",		\
		    tests_current_failed,				\
		    tests_current);					\
	if (tests_current_skipped > 0)					\
		printf(", %d skipped)\n", tests_current_skipped);	\
	else	printf(")\n");						\
	if (tests_current_failed != 0 || tests_current_skipped != 0)	\
		printf("\n");						\
	tests += tests_current;						\
	tests_failed += tests_current_failed;				\
	tests_skipped += tests_current_skipped;				\
} while (0)

#define TEST_SKIP(format, ...) do {					\
	_TEST_SKIPPED(format, __VA_ARGS__);				\
} while (0)

/*
 * Test the given expression and fail the test if it evaluates to 0.
 */
#define TEST_ASSERT(expr) do {						\
	tests_current++;						\
	if (!(expr))							\
		_TEST_FAILED(#expr);					\
} while (0)

/*
 * As above, but also print the given message indented by 2 tabs.
 */
#define TEST_ASSERT_MSG(expr, format, ...) do {				\
	tests_current++;						\
	if (!(expr))							\
		_TEST_FAILED_MSG(#expr, format, __VA_ARGS__);		\
} while (0)

/*
 * As above, but print the textual value of errno in case the expression
 * evaluates to 0.
 */
#define TEST_ASSERT_ERRNO(expr)						\
	TEST_ASSERT_MSG(expr, "errno: %s", strerror(errno))

/*
 * Compare the given numbers and fail the test if they don't match.
 */
#define TEST_ASSERT_VALCMP(a, b, fmt)					\
	TEST_ASSERT_MSG((a) == (b), fmt " != " fmt, a, b)

/*
 * Compare the given strings and fail the test if they don't match.
 */
#define TEST_ASSERT_STRCMP(a, b)					\
	TEST_ASSERT_MSG(strcmp(a, b) == 0, "\"%s\" != \"%s\"", a, b)

/*
 * Internal.
 */
#define _TEST_FAILED(exprstr) do {					\
	if (tests_current_failed++ == 0 && tests_current_skipped == 0)	\
		printf("\n");						\
	printf("\t%s:%d: failed: %s\n", __FILE__, __LINE__, exprstr);	\
} while (0)

#define _TEST_FAILED_MSG(exprstr, format, ...) do {			\
	_TEST_FAILED(exprstr);						\
	printf("\t\t" format "\n", __VA_ARGS__ );			\
} while (0)

#define _TEST_SKIPPED(format, ...) do {					\
	if (tests_current_skipped++ == 0 && tests_current_failed == 0)	\
		printf("\n");						\
	printf("\t%s:%d: skipped: " format "\n", __FILE__, __LINE__,	\
	    __VA_ARGS__);						\
} while (0)

#endif /* _TEST_H_ */
