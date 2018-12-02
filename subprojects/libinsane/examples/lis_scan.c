#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/safebet.h>
#include <libinsane/util.h> // TODO



static void noop() { }


struct lis_log_callbacks g_log_callbacks = {
	.callbacks = {
		[LIS_LOG_LVL_DEBUG] = noop,
		[LIS_LOG_LVL_INFO] = noop,
		[LIS_LOG_LVL_WARNING] = noop,
		[LIS_LOG_LVL_ERROR] = lis_log_stderr,
	},
};


//! [ExampleLisScan]
static void lets_scan(void)
{
#define CHECK_ERR(call) do { \
		err = call; \
		if (LIS_IS_ERROR(err)) { \
			fprintf( \
					stderr, "%s(L%d): ERROR: %X, %s\n", \
					__FILE__, __LINE__, \
					err, lis_strerror(err) \
					); \
			goto end; \
		} \
	} while(0)


	enum lis_error err;
	struct lis_api *impl = NULL;
	struct lis_device_descriptor **dev_infos;
	struct lis_item *device = NULL;
	struct lis_item **sources;
	struct lis_scan_parameters parameters;
	struct lis_scan_session *scan_session;
	char img_buffer[4096];
	size_t bufsize;
	size_t obtained = 0;

	CHECK_ERR(lis_safebet(&impl));
	CHECK_ERR(impl->list_devices(
		impl, LIS_DEVICE_LOCATIONS_ANY, &dev_infos
	));

	if (dev_infos[0] == NULL) {
		fprintf(stderr, "No scan device found\n");
		return;
	}

	// let's use the first scan device found, because it looks cool.
	printf("Will use %s %s (%s ; %s)\n",
			dev_infos[0]->vendor, dev_infos[0]->model,
			dev_infos[0]->type,
			dev_infos[0]->dev_id);
	CHECK_ERR(impl->get_device(impl, dev_infos[0]->dev_id, &device));

	CHECK_ERR(device->get_children(device, &sources));

	// Normalizers ensure us that there is at least one source,
	// so let's use the first one because it looks cool too.
	printf("Will use source '%s'\n", sources[0]->name);

	// Setting resolution: This one may or may not work, depending on
	// the scanner
	printf("Setting resolution to 300\n");
	CHECK_ERR(lis_set_option(sources[0], OPT_NAME_RESOLUTION, "300"));
	// Normalizers ensure us that the mode option has the value "Color"
	// by default (still put here for the example)
	printf("Setting mode to Color\n");
	CHECK_ERR(lis_set_option(sources[0], OPT_NAME_MODE, "Color"));
	// Normalizers ensure us that by default, the maximum scan area will
	// be used

	CHECK_ERR(sources[0]->scan_start(sources[0], &scan_session));

	// scan parameters must be obtained *after* the scan session has been
	// started if we want a reliable image width (this is a limitation of
	// some drivers).
	CHECK_ERR(scan_session->get_scan_parameters(
		scan_session, &parameters
	));
	printf(
		"Scan will be: %d px x %d px (%zd bytes)\n",
		parameters.width, parameters.height, parameters.image_size
	);

	while (!scan_session->end_of_feed(scan_session)) {
		while (!scan_session->end_of_page(scan_session)) {
			bufsize = sizeof(img_buffer);
			err = scan_session->scan_read(
				scan_session, img_buffer, &bufsize
			);
			CHECK_ERR(err);

			if (err == LIS_WARMING_UP) {
				// old scanners need warming time.
				// No data has been returned.
				assert(bufsize == 0);
				sleep(1);
				continue;
			}

			obtained += bufsize;
			printf(
				"\r%zd KB / %zd KB",
				(obtained / 1024),
				(parameters.image_size / 1024)
			);
			fflush(stdout);

			// do something with the chunk of the image/page that
			// has just been scanned

			// here for example we write simply to a file
			// TODO
		}
		// do something with the whole image/page that has just been scanned
	}

	// do something with all the images/pages that have just been scanned
	printf("\nAll done !\n");

end:
	if (device != NULL) {
		device->close(device);
	}
	if (impl != NULL) {
		impl->cleanup(impl);
	}

#undef CHECK_ERR
}
//! [ExampleLisScan]


int main(int argc, char **argv)
{
	LIS_UNUSED(argv); // TODO
	LIS_UNUSED(argc); // TODO

	lis_set_log_callbacks(&g_log_callbacks);
	lets_scan();
	return EXIT_SUCCESS;
}
