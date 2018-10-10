#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/wia_ll.h>
#include <libinsane/util.h>

#include "../src/bases/wia/properties.h"
#include "util.h"


static void tests_wia_check_properties(void)
{
	const struct lis_wia2lis_property *props;
	size_t nb_props, i, j;

	props = lis_get_all_properties(&nb_props);

	// make sure we don't have twice the same property in the list
	// because it would have undefined effects
	for (i = 0 ; i < nb_props ; i++) {
		for (j = i + 1 ; j < nb_props ; j++) {
			if (props[i].wia.id == props[j].wia.id
					&& props[i].item_type == props[j].item_type) {
				fprintf(stderr,
					"ERROR: WIA property duplicated:"
					"%lu:%s (%ld/%ld ; L%d) == %lu:%s (%ld/%ld ; L%d)",
					props[i].wia.id, props[i].lis.name,
					(long)i, (long)nb_props, props[i].line,
					props[j].wia.id, props[j].lis.name,
					(long)j, (long)nb_props, props[j].line
				);
			}
			if (props[i].item_type == props[j].item_type) {
				LIS_ASSERT_NOT_EQUAL(props[i].wia.id, props[j].wia.id);
			}
		}
	}
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("WIA2 Low-Level", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	/* order of tests may matter because of Sane test backend ... and that makes me a sad panda .. :( */
	if (CU_add_test(suite, "check properties", tests_wia_check_properties) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
