#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/wia_ll.h>
#include <libinsane/util.h>

#define LIS_UNIT_TESTS
#include "../src/bases/twain/capabilities.h"
#include "util.h"


static void tests_twain_check_capabilities(void)
{
	const struct lis_twain_cap *all_caps;
	int nb_caps, i, j;

	all_caps = lis_twain_get_all_caps(&nb_caps);

	for (i = 0 ; i < nb_caps ; i++) {
		for (j = i + 1 ; j < nb_caps ; j++) {
			if (strcasecmp(all_caps[i].name, all_caps[j].name) == 0) {
				fprintf(stderr,
					"ERROR: Duplicated TWAIN capability name: %s (%d | %d)",
					all_caps[i].name, all_caps[i].id, all_caps[j].id
				);
				LIS_ASSERT_NOT_EQUAL(strcasecmp(all_caps[i].name, all_caps[j].name), 0);
			}
		}
	}

	for (i = 0 ; i < nb_caps ; i++) {
		for (j = i + 1 ; j < nb_caps ; j++) {
			if (all_caps[i].id == all_caps[j].id) {
				fprintf(stderr,
					"ERROR: Duplicated TWAIN capability id: %d (%s | %s)",
					all_caps[i].id, all_caps[i].name, all_caps[j].name
				);
				LIS_ASSERT_NOT_EQUAL(all_caps[i].id, all_caps[j].id);
			}
		}
	}
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("TWAIN", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "check capabilities", tests_twain_check_capabilities) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
