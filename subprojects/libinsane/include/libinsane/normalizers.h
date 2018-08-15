#ifndef __LIBINSANE_NORMALIZERS_H
#define __LIBINSANE_NORMALIZERS_H

#include "capi.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \todo TWAIN
 */

/*!
 * \brief Ensure that sources are represented as node
 *
 * WIA2: For each scanner, provide a device tree (see :ref:`WIA2` description).
 *
 * Sane: There is no tree (see :ref:`Sane` description). Children nodes (sources) must be
 * simulated.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_source_nodes(
		struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure there is at least one source
 *
 * - Culprits: Microsoft, Sane project
 *
 * Sane: [Some scanner do not provide the option "source"](https://openpaper.work/en-us/scanner_db/report/57).
 *
 * WIA2: [Some scanners do not always provide a source](https://openpaper.work/en/scanner_db/report/28). Scan is done directly on them.
 *
 * If there is no source at all, this normalization will create a fake one.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_min_one_source(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure all options are available on sources
 *
 * - Culprits: Microsoft, Sane project
 *
 * Goal: Makes sure the application can find easily options by making
 * the scanner options available on all its sources.
 *
 * Sane: Only the scanner itself has options. Options must all be mapped
 * too on all its sources.
 *
 * WIA: Some options are on the scanner itself, some options are on the
 * sources. Scanner options must be mapped on all its sources.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_all_opts_on_all_sources(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure scan area option names are all the same
 *
 * Sane: [Sane scan area options](http://sane.alioth.debian.org/sane2/0.08/doc014.html#s4.5.4)
 * are used as reference.
 *
 * WIA2: Fake options are added to simulate Sane options. They act like Sane
 * options, and any change to these options is applied back to the WIA2 options.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_scan_area_opts(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure resolution constraint is always in the same format
 *
 * - Culprits: Microsoft, Sane project
 *
 * Sane and WIA: Resolution constraint can be expressed as a range or as a list
 * of possible values. This normalization makes sure they are always expressed as
 * a list. If the range has an interval < 25dpi, the interval used to generate the
 * list will be 25dpi.
 *
 * Sane: Resolution can be expressed as integers or as SANE_Fixed values
 * (16 bits integer / 16 bits non-integer) (converted as float for the
 * C API)
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_resolution(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure source types are clearly identified
 *
 * - Culprit: Sane project
 *
 * Sane: Sources have "name", but the exact names are up to the drivers.
 *
 * WIA2: source types are already clearly defined.
 *
 * See \ref lis_item.type.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_source_types(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure the output format is RAW
 *
 * - Culprit: Microsoft.
 *
 * Always getting the image as RAW24 is much more handy if you want to
 * display the scan on-the-fly.
 *
 * Sane: Image is always returned as RAW (unless some scanner-specific
 * options are set to non-default values).
 *
 * WIA2: Drivers may return the image in a variety of file formats: RAW, BMP,
 * JPEG, PNG, etc. Not all drivers support returning the image as RAW24.
 * LibInsane supports only BMP and will output the image as RAW24.
 * WIA2 drivers
 * [must support the BMP format](https://msdn.microsoft.com/en-us/ie/ff546016(v=vs.94)).
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_raw(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure the output format is RAW24.
 *
 * - Culprit: Sane
 *
 * Sane can return the image as various raw formats:
 * RAW1 (B&W), RAW8 (Grayscale), RAW24 (RGB), etc.
 *
 * This normalization ensures the output image is always in RAW24 (RGB).
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_raw24(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Set safest default values
 *
 * ## Ensure default mode is Color
 *
 * Not all scanner have mode=Color by default
 *
 * ## Ensure the scan area is set to the maximum by default.
 *
 * By default, some drivers don't have the scan area set to the maximum.
 * This workaround just make sure the default area is the maximum area.
 * It may be handy if you don't want to scan a specific area.
 *
 * Requires: \ref lis_api_normalizer_scan_area_opts
 *
 * ## Ensure the scan mode by default is 24bits colors.
 *
 * By default, some drivers don't have the mode set to color.
 * This workaround just make sure the default mode is 24bits color.
 *
 * Requires: \ref lis_api_workaround_opt_values
 *
 * ## Fujistu: Extra options 'page-height' and 'page-width'
 *
 * - API: Sane
 * - Culprit: Fujitsu
 * - Seen on: [Fujitsu ScanSnap S1500](https://github.com/openpaperwork/paperwork/issues/230#issuecomment-22792362)
 *   and [Fujistu ScanSnap iX500](https://openpaper.work/en/scanner_db/report/122/)
 *
 * Fujistu provides 2 extra settings, 'page-height' and 'page-width'. 'page-height' use case is unknown,
 * but 'page-width' is used for automatic centering of the page.
 * The default values are bad and this feature is specific to Fujistu ==> set them to the max
 * by default to disable automatic centering.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_safe_defaults(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Makes sure the source names all look the same accross OSes
 *
 * - All source names will be lower-case
 * - No WIA prefix
 * - All ADF will be called "feeder"
 * - All flatbeds will be called "flatbed"
 *
 * Exanples when using WIA:
 * - '0000\\Root\\Flatbed' --> 'flatbed'
 * - '0000\\Root\\Feeder' --> 'feeder'
 *
 * Examples when using Sane:
 * - 'Automatic document Feeder (left aligned)' --> 'feeder (left aligned)'
 * - 'ADF Duplex' --> 'feeder duplex'
 * - 'Flatbed' --> 'flatbed'
 * - 'Document Table' --> 'flatbed' (Epson perfection v19)
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_source_names(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Clean device descriptors (name, model, etc)
 *
 * ## Device model name may contain '_' instead of spaces
 *
 * - API: Sane
 * - Culprit: HP
 * - Seen on: [all HP devices](https://openpaper.work/scanner_db/vendor/7/)
 *
 * ## Device model name may contain manufacturer name
 *
 * - API: Sane, WIA
 * - Culprits: too many. Especially HP.
 *
 * If the model name contains also the manufacturer name, this workaround strips it.
 *
 * Random example:
 *
 * - Manufacturer: Brother
 * - Model: Brother MFC-7360N
 *
 * Will become:
 *
 * - Manufacturer: Brother
 * - Model: MFC-7360N
 *
 * Special case: HP. Manufacturer is "hewlett-packard", but
 * [model names contain the prefix "hp"](https://openpaper.work/scanner_db/vendor/7/).
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_normalizer_clean_dev_descs(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


#ifdef __cplusplus
}
#endif

#endif
