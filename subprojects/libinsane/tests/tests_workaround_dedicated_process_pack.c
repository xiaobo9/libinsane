#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include "main.h"
#include "util.h"

#include "../src/workarounds/dedicated_process/pack.h"


static void tests_serialize_integers(void)
{
	size_t s;
	int in[2];
	void *out;

	s = lis_compute_packed_size("di", 22, 24);
	LIS_ASSERT_EQUAL(s, sizeof(int) * 2);

	out = in;
	lis_pack(&out, "di", 22, 24);
	LIS_ASSERT_EQUAL(out, in + 2);
}


static void tests_deserialize_integers(void)
{
	int in[2] = { 122, 124 };
	const void *in_ptr = in;
	int a, b;

	lis_unpack(&in_ptr, "id", &a, &b);
	LIS_ASSERT_EQUAL(a, 122);
	LIS_ASSERT_EQUAL(b, 124);
	LIS_ASSERT_EQUAL(in_ptr, in + 2);
}


static void tests_serialize_doubles(void)
{
	size_t s;
	double a = 22.5, b = 23.5;
	double in[2];
	void *out;

	s = lis_compute_packed_size("ff", a, b);
	LIS_ASSERT_EQUAL(s, sizeof(double) * 2);

	out = in;
	lis_pack(&out, "ff", a, b);
	LIS_ASSERT_EQUAL(out, in + 2);
}


static void tests_deserialize_doubles(void)
{
	double in[2] = { 32.5, 444.5 };
	const void *in_ptr = in;
	double a, b;

	lis_unpack(&in_ptr, "ff", &a, &b);
	LIS_ASSERT_EQUAL(a, 32.5);
	LIS_ASSERT_EQUAL(b, 444.5);
	LIS_ASSERT_EQUAL(in_ptr, in + 2);
}


static void tests_ptrs(void)
{
	size_t s;
	char *a_in = "abc";
	char *b_in = "def";
	char *a_out = NULL;
	char *b_out = NULL;
	char buf[2 * sizeof(char *)];
	const void *buf_ptr = buf;
	void *out;

	s = lis_compute_packed_size("pp", a_in, b_in);
	LIS_ASSERT_EQUAL(s, 2 * sizeof(char *));

	out = buf;
	lis_pack(&out, "pp", a_in, b_in);
	LIS_ASSERT_EQUAL(out, buf + (2 * sizeof(char *)));

	lis_unpack(&buf_ptr, "pp", &a_out, &b_out);
	LIS_ASSERT_EQUAL(a_in, a_out);
	LIS_ASSERT_EQUAL(b_in, b_out);
}


static void tests_strings(void)
{
	size_t s;
	char *a = "abc";
	char *b = "def";
	char buf[sizeof("abc") + sizeof("def")];
	const void *buf_ptr = buf;
	void *out;

	s = lis_compute_packed_size("ss", a, b);
	LIS_ASSERT_EQUAL(s, sizeof("abc") + sizeof("def"));

	out = buf;
	lis_pack(&out, "ss", a, b);
	LIS_ASSERT_EQUAL(strcmp(buf, "abc"), 0);
	LIS_ASSERT_EQUAL(strcmp(buf + sizeof("abc"), "def"), 0);

	a = NULL;
	b = NULL;
	lis_unpack(&buf_ptr, "ss", &a, &b);
	LIS_ASSERT_EQUAL(strcmp(a, "abc"), 0);
	LIS_ASSERT_EQUAL(strcmp(b, "def"), 0);
	LIS_ASSERT_EQUAL(buf_ptr, buf + sizeof("abc") + sizeof("def"));
}


static void tests_strings_null(void)
{
	size_t s;
	char *a = NULL;
	char *b = NULL;
	char buf[2 * sizeof("")] = { 0 };
	const void *buf_ptr = buf;
	void *out;

	s = lis_compute_packed_size("ss", a, b);
	LIS_ASSERT_EQUAL(s, 2 * sizeof(""));

	out = buf;
	lis_pack(&out, "ss", a, b);
	LIS_ASSERT_EQUAL(buf[0], '\0');
	LIS_ASSERT_EQUAL(buf[1], '\0');

	a = NULL;
	b = NULL;
	lis_unpack(&buf_ptr, "ss", &a, &b);
	LIS_ASSERT_EQUAL(a[0], '\0');
	LIS_ASSERT_EQUAL(b[0], '\0');
	LIS_ASSERT_EQUAL(buf_ptr, buf + sizeof("") + sizeof(""));
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Workaround dedicated process serialization", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_serialize_integers()", tests_serialize_integers) == NULL
			|| CU_add_test(suite, "tests_deserialize_integers()", tests_deserialize_integers) == NULL
			|| CU_add_test(suite, "tests_ptrs()", tests_ptrs) == NULL
			|| CU_add_test(suite, "tests_strings()", tests_strings) == NULL
			|| CU_add_test(suite, "tests_strings_null()", tests_strings_null) == NULL
			|| CU_add_test(suite, "tests_serialize_doubles()", tests_serialize_doubles) == NULL
			|| CU_add_test(suite, "tests_deserialize_doubles()", tests_deserialize_doubles) == NULL
			) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
