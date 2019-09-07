#ifndef __LIBINSANE_WORKAROUNDS_H
#define __LIBINSANE_WORKAROUNDS_H

#include "capi.h"


#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Fix options names
 *
 * ## Option 'scan-resolution' --> 'resolution'
 *
 * - API: Sane
 * - Culprit: Lexmark
 * - Seen on: Lexmark MFP
 *
 * The option 'resolution' is mistakenly named 'scan-resolution'.
 * This workaround replaces it by an option 'resolution'.
 *
 * ## Option 'doc-source' --> 'source'
 *
 * - API: Sane
 * - Culprit: Samsung
 * - Seen on: [Samsung CLX-3300](https://openpaper.work/scanner_db/report/31/)
 *
 * The option 'source' is mistakenly named 'doc-source'.
 * This workaround replaces it by an option option 'source'.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_workaround_opt_names(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Replace unusual option values by usual ones
 *
 * ## Option 'mode': Unusual mode values
 *
 * - API: Sane
 * - Seen on:
 *   - [Brother MFC-7360N](https://openpaper.work/scanner_db/report/20/)
 *   - [Samsung CLX-3300](https://openpaper.work/scanner_db/report/31/)
 *
 * Override the option 'mode' so it changes the following possible values:
 *
 * - Brother
 *   - '24bit Color' --> 'Color'
 *   - 'Black & White' --> 'Lineart'
 *   - 'True Gray' --> 'Gray'
 * - Samsung
 *   - 'Black and White - Line Art' --> 'Lineart'
 *   - 'Grayscale - 256 Levels' --> 'Gray'
 *   - 'Color - 16 Million Colors' --> 'Color'
 *
 * ## Strip option translations
 *
 * - API: Sane
 * - Culprits: Sane project, OKI
 * - Seen on: [OKI MC363](https://openpaper.work/scanner_db/report/56/)
 *
 * This workaround wraps a bunch of options, and try to revert the translations back to English.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_workaround_opt_values(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Prevent operations on options that are not allowed by capabilities
 *
 * ## Do not let application access value of inactive options
 *
 * - API: Sane
 * - Seen on: Sane test backend
 *
 * Some drivers allows access to inactive options (even just for reading).
 * Some may even crash if the user application tries to set a value on an inactive option.
 *
 * ## Do not let application set value on read-only options
 *
 * - API: Sane
 * - Seen on: Can't remember
 *
 * Behavior is undefined when trying to set read-only values.
 * This workaround makes it defined: it always returns an error.
 *
 * ##  Do not let application set value on option that can have only one value
 *
 * - API: Sane
 * - Seen on: [Epson DS-310](https://openpaper.work/scanner_db/report/120/)
 * - Seen on: Epson XP-425
 *
 * When trying to set a value on a property that accept only one value
 * (ex: source=ADF), Sane driver may return SANE_STATUS_INVAL instead of success.
 * This workaround makes sure the value provided matches the only one possible
 * and doesn't even set it.
 *
 * ## Ignore the fact that option is inactive if 'source' is inactive
 *
 * - API: Sane
 * - Seen on: [Canon LiDE 220](https://openpaper.work/scannerdb/report/295/)
 *
 * Some scanners just flag all options as inactive
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_workaround_check_capabilities(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Thread-safety
 *
 * - API: Sane, WIA, TWAIN
 *
 * Most scanner APIs are not thread-safe. If you're lucky, they may work
 * from different threads anyway. If you're not, they will crash your program.
 *
 * This workaround works around this issue by creating a dedicated thread for
 * the job and making all the request go through this thread.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_workaround_dedicated_thread(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Ensure Flatbeds return only one page.
 *
 * - API: Sane, WIA
 * - Culprit: EPSON XP-425 (Sane)
 *
 * Flatbed can only contain one single page. However when requesting
 * another page/image, some drivers keep saying "yeah sure no problem"
 * instead of "no more pages", and keep returning the same page/image
 * again and again.
 *
 * Requires normalizer 'normalizer_source_types'.
 *
 * \param[in] to_wrap Base implementation to wrap.
 * \param[out] out_impl Implementation of the out_impl including the workaround.
 */
extern enum lis_error lis_api_workaround_one_page_flatbed(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


/*!
 * \brief Minimize calls to underlying API
 *
 * - API: Sane (maybe others)
 * - Culprit: HP drivers + sane backend 'net' (+ difference of versions
 *   between servers and clients) (maybe
 *   others)
 *
 * Some drivers or combinations of drivers seem to be very touchy. This
 * workaround aim to reduce to a strict minimum all the calls to
 * list_options(), option->fn.set(), option->fn.get().
 *
 * Assumes that the 'set_flags' when calling option->fn.set() is reliable.
 *
 * Also keep track of the items. Return the same items as long as
 * they haven't been closed. This reduce risk of programming error
 * (even more when using the GObject layer).
 */
extern enum lis_error lis_api_workaround_cache(
	struct lis_api *to_wrap, struct lis_api **out_impl
);


#ifdef __cplusplus
}
#endif

#endif
