#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "properties.h"
#include "util.h"
#include "wia2.h"


static const struct lis_wia2lis_possibles g_possible_formats[] = {
	{
		.wia.clsid = &WiaImgFmt_BMP,
		.lis.string = "bmp",
	},
	{
		.wia.clsid = &WiaImgFmt_CIFF,
		.lis.string = "ciff",
	},
	{
		.wia.clsid = &WiaImgFmt_EXIF,
		.lis.string = "exif",
	},
	{
		.wia.clsid = &WiaImgFmt_FLASHPIX,
		.lis.string = "flashpix",
	},
	{
		.wia.clsid = &WiaImgFmt_GIF,
		.lis.string = "gif",
	},
	{
		.wia.clsid = &WiaImgFmt_ICO,
		.lis.string = "ico",
	},
	/* TODO
	{
		.wia.clsid = &WiaImgFmt_JBIG,
		.lis.string = "jbig",
	},
	*/
	{
		.wia.clsid = &WiaImgFmt_JPEG,
		.lis.string = "jpeg",
	},
	{
		.wia.clsid = &WiaImgFmt_JPEG2K,
		.lis.string = "jpeg2k",
	},
	{
		.wia.clsid = &WiaImgFmt_JPEG2KX,
		.lis.string = "jpeg2kx",
	},
	{
		.wia.clsid = &WiaImgFmt_MEMORYBMP,
		.lis.string = "memorybmp",
	},
	/* TODO
	{
		.wia.clsid = &WiaImgFmt_PDFA,
		.lis.string = "pdfa",
	},
	*/
	{
		.wia.clsid = &WiaImgFmt_PHOTOCD,
		.lis.string = "photocd",
	},
	{
		.wia.clsid = &WiaImgFmt_PICT,
		.lis.string = "pict",
	},
	{
		.wia.clsid = &WiaImgFmt_PNG,
		.lis.string = "png",
	},
	/* TODO
	{
		.wia.clsid = &WiaImgFmt_RAW,
		.lis.string = "raw",
	},
	*/
	{
		.wia.clsid = &WiaImgFmt_RAWRGB,
		.lis.string = "rawrgb",
	},
	{
		.wia.clsid = &WiaImgFmt_TIFF,
		.lis.string = "tiff",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_document_handling_select[] = {
	{
		.wia.integer = FEEDER,
		.lis.string = "feeder",
	},
	{
		.wia.integer = FLATBED,
		.lis.string = "flatbed",
	},
	{
		.wia.integer = DUPLEX,
		.lis.string = "duplex",
	},
	{
		.wia.integer = AUTO_ADVANCE,
		.lis.string = "auto_advance",
	},
	{
		.wia.integer = FRONT_FIRST,
		.lis.string = "front_first",
	},
	{
		.wia.integer = BACK_FIRST,
		.lis.string = "back_first",
	},
	{
		.wia.integer = FRONT_ONLY,
		.lis.string = "front_only",
	},
	{
		.wia.integer = BACK_ONLY,
		.lis.string = "back_only",
	},
	{
		.wia.integer = NEXT_PAGE,
		.lis.string = "next_page",
	},
	{
		.wia.integer = PREFEED,
		.lis.string = "prefeed",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_previews[] = {
	{
		.wia.integer = LIS_WIA_ADVANCED_PREVIEW,
		.lis.string = "advanced",
	},
	{
		.wia.integer = LIS_WIA_BASIC_PREVIEW,
		.lis.string = "basic",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_registrations[] = {
	{
		.wia.integer = LEFT_JUSTIFIED,
		.lis.string = "left_justified",
	},
	{
		.wia.integer = CENTERED,
		.lis.string = "centered",
	},
	{
		.wia.integer = RIGHT_JUSTIFIED,
		.lis.string = "right_justified",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_vertical_registrations[] = {
	{
		.wia.integer = TOP_JUSTIFIED,
		.lis.string = "top_justified",
	},
	{
		.wia.integer = CENTERED,
		.lis.string = "centered",
	},
	{
		.wia.integer = BOTTOM_JUSTIFIED,
		.lis.string = "bottom_justified",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_page_sizes[] = {
	{ .wia.integer = WIA_PAGE_A4, .lis.string = "a4", },

	// see WIA_DPS_PAGE_HEIGHT and WIA_DPS_PAGE_WIDTH
	{ .wia.integer = WIA_PAGE_CUSTOM, .lis.string = "custom", },

	{ .wia.integer = WIA_PAGE_LETTER, .lis.string = "letter", },
	{ .wia.integer = LIS_WIA_PAGE_USLEGAL, .lis.string = "uslegal", },
	{ .wia.integer = LIS_WIA_PAGE_USLETTER, .lis.string = "usletter", },
	{ .wia.integer = LIS_WIA_PAGE_USLEDGER, .lis.string = "usledger", },
	{ .wia.integer = LIS_WIA_PAGE_USSTATEMENT, .lis.string = "usstatement", },
	{ .wia.integer = LIS_WIA_PAGE_BUSINESSCARD, .lis.string = "businesscard", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A0, .lis.string = "iso_a0", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A1, .lis.string = "iso_a1", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A2, .lis.string = "iso_a2", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A3, .lis.string = "iso_a3", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A4, .lis.string = "iso_a4", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A5, .lis.string = "iso_a5", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A6, .lis.string = "iso_a6", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A7, .lis.string = "iso_a7", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A8, .lis.string = "iso_a8", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A9, .lis.string = "iso_a9", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_A10, .lis.string = "iso_a10", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B0, .lis.string = "iso_b0", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B1, .lis.string = "iso_b1", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B2, .lis.string = "iso_b2", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B3, .lis.string = "iso_b3", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B4, .lis.string = "iso_b4", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B5, .lis.string = "iso_b5", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B6, .lis.string = "iso_b6", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B7, .lis.string = "iso_b7", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B8, .lis.string = "iso_b8", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B9, .lis.string = "iso_b9", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_B10, .lis.string = "iso_b10", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C0, .lis.string = "iso_c0", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C1, .lis.string = "iso_c1", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C2, .lis.string = "iso_c2", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C3, .lis.string = "iso_c3", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C4, .lis.string = "iso_c4", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C5, .lis.string = "iso_c5", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C6, .lis.string = "iso_c6", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C7, .lis.string = "iso_c7", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C8, .lis.string = "iso_c8", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C9, .lis.string = "iso_c9", },
	{ .wia.integer = LIS_WIA_PAGE_ISO_C10, .lis.string = "iso_c10", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B0, .lis.string = "jis_b0", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B1, .lis.string = "jis_b1", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B2, .lis.string = "jis_b2", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B3, .lis.string = "jis_b3", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B4, .lis.string = "jis_b4", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B5, .lis.string = "jis_b5", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B6, .lis.string = "jis_b6", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B7, .lis.string = "jis_b7", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B8, .lis.string = "jis_b8", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B9, .lis.string = "jis_b9", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_B10, .lis.string = "jis_b10", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_2A, .lis.string = "jis_2a", },
	{ .wia.integer = LIS_WIA_PAGE_JIS_4A, .lis.string = "jis_4a", },
	{ .wia.integer = LIS_WIA_PAGE_DIN_2B, .lis.string = "din_2b", },
	{ .wia.integer = LIS_WIA_PAGE_DIN_4B, .lis.string = "din_4b", },
	{ .wia.integer = LIS_WIA_PAGE_AUTO, .lis.string = "auto", },
	{ .wia.integer = LIS_WIA_PAGE_CUSTOM_BASE, .lis.string = "custom_base", },
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_orientations[] = {
	{
		.wia.integer = LANSCAPE,
		.lis.string = "landscape",
	},
	{
		.wia.integer = PORTRAIT,
		.lis.string = "portrait",
	},
	{
		.wia.integer = ROT180,
		.lis.string = "rot180",
	},
	{
		.wia.integer = ROT270,
		.lis.string = "rot270",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_segmentations[] = {
	{
		.wia.integer = LIS_WIA_USE_SEGMENTATION_FILTER,
		.lis.string = "true",
	},
	{
		.wia.integer = LIS_WIA_DONT_USE_SEGMENTATION_FILTER,
		.lis.string = "false",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_possibles g_possible_show_preview[] = {
	{
		.wia.integer = WIA_SHOW_PREVIEW_CONTROL,
		.lis.string = "show_preview_control",
	},
	{
		.wia.integer = WIA_DONT_SHOW_PREVIEW_CONTROL,
		.lis.string = "dont_show_preview_control",
	},
	{ .eol = 1 },
};


static const struct lis_wia2lis_property g_wia2lis_properties[] = {

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPA_CONNECT_STATUS, .type = VT_I4, },
		.lis = { .name = "connect_status", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_DEVICE_NOT_CONNECTED,
				.lis.string = "not_connected"
			},
			{
				.wia.integer = WIA_DEVICE_CONNECTED,
				.lis.string = "connected"
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPA_FIRMWARE_VERSION, .type = VT_BSTR, },
		.lis = { .name = "firmware_version", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_ACCESS_RIGHTS, .type = VT_I4, },
		.lis = { .name = "access_rights", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_ITEM_READ,
				.lis.string = "read",
			},
			{
				.wia.integer = WIA_ITEM_WRITE,
				.lis.string = "write",
			},
			{
				.wia.integer = WIA_ITEM_CAN_BE_DELETED,
				.lis.string = "can_be_deleted",
			},
			{
				.wia.integer = WIA_ITEM_RD,
				.lis.string = "read_can_be_deleted",
			},
			{
				.wia.integer = WIA_ITEM_RWD,
				.lis.string = "read_write_can_be_deleted",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_BITS_PER_CHANNEL, .type = VT_I4, },
		.lis = { .name = "bits_per_channel", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_BUFFER_SIZE, .type = VT_I4, },
		.lis = { .name = "buffer_size", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_BYTES_PER_LINE, .type = VT_I4, },
		.lis = { .name = "bytes_per_line", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_CHANNELS_PER_PIXEL, .type = VT_I4, },
		.lis = { .name = "channels_per_pixel", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_COLOR_PROFILE, .type = VT_I4, },
		.lis = { .name = "color_profile", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_COMPRESSION, .type = VT_I4, },
		.lis = { .name = "compression", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_COMPRESSION_NONE,
				.lis.string = "none",
			},
			{
				.wia.integer = LIS_WIA_COMPRESSION_AUTO,
				.lis.string = "auto",
			},
			{
				.wia.integer = WIA_COMPRESSION_BI_RLE4,
				.lis.string = "bi_rle4",
			},
			{
				.wia.integer = WIA_COMPRESSION_BI_RLE8,
				.lis.string = "bi_rle8",
			},
			{
				.wia.integer = WIA_COMPRESSION_G3,
				.lis.string = "g3",
			},
			{
				.wia.integer = WIA_COMPRESSION_G4,
				.lis.string = "g4",
			},
			{
				.wia.integer = WIA_COMPRESSION_JPEG,
				.lis.string = "jpeg",
			},
			{
				.wia.integer = LIS_WIA_COMPRESSION_JBIG,
				.lis.string = "jbig",
			},
			{
				.wia.integer = LIS_WIA_COMPRESSION_JPEG2K,
				.lis.string = "jpeg2k",
			},
			{
				.wia.integer = LIS_WIA_COMPRESSION_PNG,
				.lis.string = "png",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_DATATYPE, .type = VT_I4, },
		.lis = { .name = "datatype", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = LIS_WIA_DATA_AUTO,
				.lis.string = "auto",
			},
			{
				.wia.integer = WIA_DATA_COLOR,
				.lis.string = "color",
			},
			{
				.wia.integer = WIA_DATA_COLOR_DITHER,
				.lis.string = "color_dither",
			},
			{
				.wia.integer = WIA_DATA_COLOR_THRESHOLD,
				.lis.string = "color_threshold",
			},
			{
				.wia.integer = WIA_DATA_DITHER,
				.lis.string = "dither",
			},
			{
				.wia.integer = WIA_DATA_GRAYSCALE,
				.lis.string = "grayscale",
			},
			{
				.wia.integer = WIA_DATA_THRESHOLD,
				.lis.string = "threshold",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_BGR,
				.lis.string = "raw_bgr",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_CMY,
				.lis.string = "raw_cmy",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_CMYK,
				.lis.string = "raw_cmyk",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_RGB,
				.lis.string = "raw_rgb",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_YUV,
				.lis.string = "raw_yuv",
			},
			{
				.wia.integer = LIS_WIA_DATA_RAW_YUVK,
				.lis.string = "raw_yuvk",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_DEPTH, .type = VT_I4, },
		.lis = { .name = "depth", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_FILENAME_EXTENSION, .type = VT_BSTR, },
		.lis = {
			.name = "filename_extension",
			.type = LIS_TYPE_STRING,
		},
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_FORMAT, .type = VT_CLSID, },
		.lis = { .name = "format", .type = LIS_TYPE_STRING, },
		.possibles = g_possible_formats
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_PREFERRED_FORMAT, .type = VT_CLSID, },
		.lis = { .name = "preferred_format", .type = LIS_TYPE_STRING, },
		.possibles = g_possible_formats
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_FULL_ITEM_NAME, .type = VT_BSTR, },
		.lis = { .name = "full_item_name", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_GAMMA_CURVES, .type = VT_I4, },
		.lis = { .name = "gamma_curves", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_ICM_PROFILE_NAME, .type = VT_BSTR, },
		.lis = { .name = "icm_profile_name", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPA_ITEM_CATEGORY, .type = VT_CLSID, },
		.lis = { .name = "item_category", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.clsid = &LIS_WIA_CATEGORY_ROOT,
				.lis.string = "root",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FLATBED,
				.lis.string = "flatbed",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FEEDER,
				.lis.string = "feeder",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FEEDER_FRONT,
				.lis.string = "feeder_front",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FEEDER_BACK,
				.lis.string = "feeder_back",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FILM,
				.lis.string = "film",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FOLDER,
				.lis.string = "folder",
			},
			{
				.wia.clsid = &LIS_WIA_CATEGORY_FINISHED_FILE,
				.lis.string = "finished_file",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_ITEM_FLAGS, .type = VT_I4, },
		.lis = { .name = "item_flags", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WiaItemTypeAnalyze,
				.lis.string = "analyze",
			},
			{
				.wia.integer = WiaItemTypeAudio,
				.lis.string = "audio",
			},
			{
				.wia.integer = WiaItemTypeBurst,
				.lis.string = "burst",
			},
			{
				.wia.integer = WiaItemTypeDeleted,
				.lis.string = "deleted",
			},
			/* TODO
			   {
			   .wia.integer = WiaItemTypeDocument,
			   .lis.string = "document",
			   },
			   */
			{
				.wia.integer = WiaItemTypeDevice,
				.lis.string = "device",
			},
			{
				.wia.integer = WiaItemTypeDisconnected,
				.lis.string = "disconnected",
			},
			{
				.wia.integer = WiaItemTypeFile,
				.lis.string = "file",
			},
			{
				.wia.integer = WiaItemTypeFolder,
				.lis.string = "folder",
			},
			{
				.wia.integer = WiaItemTypeFree,
				.lis.string = "free",
			},
			{
				.wia.integer = WiaItemTypeGenerated,
				.lis.string = "generated",
			},
			{
				.wia.integer = WiaItemTypeHasAttachments,
				.lis.string = "has_attachments",
			},
			{
				.wia.integer = WiaItemTypeHPanorama,
				.lis.string = "hpanorama",
			},
			{
				.wia.integer = WiaItemTypeImage,
				.lis.string = "image",
			},
			/* TODO
			   {
			   .wia.integer = WiaItemTypeProgrammableDataSource,
			   .lis.string = "programmable_data_source",
			   },
			   */
			{
				.wia.integer = WiaItemTypeRoot,
				.lis.string = "root",
			},
			{
				.wia.integer = WiaItemTypeStorage,
				.lis.string = "storage",
			},
			{
				.wia.integer = WiaItemTypeTransfer,
				.lis.string = "transfer",
			},
			{
				.wia.integer = WiaItemTypeVideo,
				.lis.string = "video",
			},
			{
				.wia.integer = WiaItemTypeVPanorama,
				.lis.string = "vpanorama",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_ITEM_NAME, .type = VT_BSTR, },
		.lis = { .name = "item_name", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_ITEM_SIZE, .type = VT_I4, },
		.lis = { .name = "item_size", .type = LIS_TYPE_INTEGER, },
		.possibles = NULL,
	},

	/* TODO
	   {
	   .line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
	   .wia = { .id = WIA_IPA_ITEM_TIME, .type = VT_UI2 | VT_VECTOR, },
	   .lis = { .name = "item_size", .type = LIS_TYPE_INTEGER, },
	   .possibles = NULL,
	   },
	   */

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPA_ITEMS_STORED, .type = VT_I4, },
		.lis = { .name = "items_stored", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	/* XXX(Jflesch): same ID than WIA_IPA_BUFFER_SIZE
	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_MIN_BUFFER_SIZE, .type = VT_I4, },
		.lis = { .name = "min_buffer_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},
	*/

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_NUMBER_OF_LINES, .type = VT_I4, },
		.lis = { .name = "number_of_lines", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_PIXELS_PER_LINE, .type = VT_I4, },
		.lis = { .name = "pixels_per_line", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_PLANAR, .type = VT_I4, },
		.lis = { .name = "planar", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	/* TODO
	   {
	   .line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
	   .wia = { .id = WIA_IPA_PROP_STREAM_COMPAT_ID, .type = VT_CLSID, },
	   .lis = {
	   .name = "prop_stream_compat_id",
	   .type = LIS_TYPE_STRING
	   },
	   .possibles = NULL, // TODO
	   },
	   */

	/* TODO
	   {
	   .line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
	   .wia = {
	   .id = WIA_IPA_RAW_BITS_PER_CHANNEL,
	   .type = VT_UI1 | VT_VECTOR,
	   },
	   .lis = {
	   .name = "raw_bits_per_channel", .type = LIS_TYPE_INTEGER
	   },
	   .possibles = NULL,
	   },
	   */

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_REGION_TYPE, .type = VT_I4, },
		.lis = { .name = "region_type", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_SUPPRESS_PROPERTY_PAGE, .type = VT_I4, },
		.lis = {
			.name = "suppress_property_page",
			.type = LIS_TYPE_STRING,
		},
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_PROPPAGE_CAMERA_ITEM_GENERAL,
				.lis.string = "camera_item_general",
			},
			{
				.wia.integer = WIA_PROPPAGE_SCANNER_ITEM_GENERAL,
				.lis.string = "scanner_item_general",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPA_TYMED, .type = VT_I4, },
		.lis = { .name = "tymed", .type = LIS_TYPE_STRING, },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = TYMED_CALLBACK,
				.lis.string = "callback",
			},
			{
				.wia.integer = TYMED_MULTIPAGE_CALLBACK,
				.lis.string = "multipage_acallback",
			},
			{
				.wia.integer = TYMED_FILE,
				.lis.string = "file",
			},
			{
				.wia.integer = TYMED_MULTIPAGE_FILE,
				.lis.string = "multipage_file",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPA_UPLOAD_ITEM_SIZE, .type = VT_I4, },
		.lis = { .name = "upload_item_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_DEV_ID, .type = VT_BSTR, },
		.lis = { .name = "dev_id", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_VEND_DESC, .type = VT_BSTR },
		.lis = { .name = "vend_desc", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_DEV_DESC, .type = VT_BSTR },
		.lis = { .name = "dev_desc", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_DEV_TYPE, .type = VT_I4 },
		.lis = { .name = "dev_type", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = StiDeviceTypeDefault,
				.lis.string = "default",
			},
			{
				.wia.integer = StiDeviceTypeScanner,
				.lis.string = "scanner",
			},
			{
				.wia.integer = StiDeviceTypeDigitalCamera,
				.lis.string = "digital_camera",
			},
			{
				.wia.integer = StiDeviceTypeStreamingVideo,
				.lis.string = "streaming_video",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_PORT_NAME, .type = VT_BSTR },
		.lis = { .name = "port_name", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_DEV_NAME, .type = VT_BSTR, },
		.lis = { .name = "dev_name", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_SERVER_NAME, .type = VT_BSTR, },
		.lis = { .name = "server_name", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_REMOTE_DEV_ID, .type = VT_BSTR, },
		.lis = { .name = "remote_dev_id", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_UI_CLSID, .type = VT_BSTR, }, // TODO ?
		.lis = { .name = "ui_clsid", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_HW_CONFIG, .type = VT_I4 },
		.lis = { .name = "hw_config", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = STI_HW_CONFIG_UNKNOWN,
				.lis.string = "generic",
			},
			{
				.wia.integer = STI_HW_CONFIG_SCSI,
				.lis.string = "scsi",
			},
			{
				.wia.integer = STI_HW_CONFIG_USB,
				.lis.string = "usb",
			},
			{
				.wia.integer = STI_HW_CONFIG_SERIAL,
				.lis.string = "serial",
			},
			{
				.wia.integer = STI_HW_CONFIG_PARALLEL,
				.lis.string = "parallel",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_BAUDRATE, .type = VT_BSTR, }, // TODO ?
		.lis = { .name = "baudrate", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_STI_GEN_CAPABILITIES, .type = VT_I4 },
		.lis = { .name = "sti_gen_capabilities", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_WIA_VERSION, .type = VT_BSTR },
		.lis = { .name = "wia_version", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DIP_DRIVER_VERSION, .type = VT_BSTR },
		.lis = { .name = "driver_version", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DIP_PNP_ID, .type = VT_BSTR },
		.lis = { .name = "pnp_id", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DIP_STI_DRIVER_VERSION, .type = VT_BSTR },
		.lis = { .name = "sti_driver_version", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DPS_DEVICE_ID, .type = VT_BSTR },
		.lis = { .name = "device_id", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_DOCUMENT_HANDLING_CAPABILITIES,
			.type = VT_I4,
		},
		.lis = {
			.name = "dps_document_handling_capabilities",
			.type = LIS_TYPE_STRING,
		},
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = LIS_AUTO_SOURCE,
				.lis.string = "auto_source",
			},
			{
				.wia.integer = LIS_ADVANCED_DUP,
				.lis.string = "advanced_dup",
			},
			{
				.wia.integer = LIS_DETECT_FILM_TPA,
				.lis.string = "detect_film_tpa",
			},
			{
				.wia.integer = LIS_FILM_TPA,
				.lis.string = "film_tpa",
			},
			{
				.wia.integer = LIS_STOR,
				.lis.string = "stor",
			},
			{
				.wia.integer = DETECT_FEED,
				.lis.string = "detect_feed",
			},
			{
				.wia.integer = DETECT_FLAT,
				.lis.string = "detect_flat",
			},
			{
				.wia.integer = DETECT_SCAN,
				.lis.string = "detect_scan",
			},
			{
				.wia.integer = DUP,
				.lis.string = "dup",
			},
			{
				.wia.integer = FEED,
				.lis.string = "feed",
			},
			{
				.wia.integer = FLAT,
				.lis.string = "flat",
			},
			{
				.wia.integer = DETECT_DUP,
				.lis.string = "detect_dup",
			},
			{
				.wia.integer = DETECT_DUP_AVAIL,
				.lis.string = "detect_dup_avail",
			},
			{
				.wia.integer = DETECT_FEED_AVAIL,
				.lis.string = "detect_feed_avail",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_DOCUMENT_HANDLING_SELECT,
			.type = VT_I4,
		},
		.lis = {
			.name = "dps_document_handling_select",
			.type = LIS_TYPE_STRING,
		},
		.possibles = g_possible_document_handling_select,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = {
			.id = LIS_WIA_IPS_DOCUMENT_HANDLING_SELECT,
			.type = VT_I4,
		},
		.lis = {
			.name = "document_handling_select",
			.type = LIS_TYPE_STRING,
		},
		.possibles = g_possible_document_handling_select,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_DOCUMENT_HANDLING_STATUS,
			.type = VT_I4,
		},
		.lis = {
			.name = "document_handling_status",
			.type = LIS_TYPE_STRING,
		},
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = FEED_READY,
				.lis.string = "feed_ready",
			},
			{
				.wia.integer = FLAT_READY,
				.lis.string = "flat_ready",
			},
			{
				.wia.integer = DUP_READY,
				.lis.string = "dup_ready",
			},
			{
				.wia.integer = FLAT_COVER_UP,
				.lis.string = "flat_cover_up",
			},
			{
				.wia.integer = PATH_COVER_UP,
				.lis.string = "path_cover_up",
			},
			{
				.wia.integer = PAPER_JAM,
				.lis.string = "paper_jam",
			},
			{
				.wia.integer = LIS_FILM_TPA_READY,
				.lis.string = "film_tpa_ready",
			},
			{
				.wia.integer = LIS_STORAGE_READY,
				.lis.string = "storage_ready",
			},
			{
				.wia.integer = LIS_STORAGE_FULL,
				.lis.string = "storage_full",
			},
			{
				.wia.integer = LIS_MULTIPLE_FEED,
				.lis.string = "multiple_feed",
			},
			{
				.wia.integer = LIS_DEVICE_ATTENTION,
				.lis.string = "device_attention",
			},
			{
				.wia.integer = LIS_LAMP_ERR,
				.lis.string = "lamp_err",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_ENDORSER_CHARACTERS, .type = VT_BSTR },
		.lis = { .name = "dps_endorser_characters", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_ENDORSER_STRING, .type = VT_BSTR },
		.lis = { .name = "dps_endorser_string", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DPS_GLOBAL_IDENTITY, .type = VT_BSTR },
		.lis = { .name = "global_identity", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_HORIZONTAL_BED_REGISTRATION,
			.type = VT_I4,
		},
		.lis = {
			.name = "dps_horizontal_bed_registration",
			.type = LIS_TYPE_STRING,
		},
		.possibles = g_possible_registrations
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_HORIZONTAL_BED_SIZE, .type = VT_I4, },
		.lis = { .name = "dps_horizontal_bed_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_HORIZONTAL_SHEET_FEED_SIZE, .type = VT_I4, },
		.lis = { .name = "dps_horizontal_sheet_feed_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_MAX_SCAN_TIME, .type = VT_I4, },
		.lis = { .name = "dps_max_scan_time", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_MIN_HORIZONTAL_SHEET_FEED_SIZE,
			.type = VT_I4,
		},
		.lis = {
			.name = "dps_min_horizontal_sheet_feed_size",
			.type = LIS_TYPE_INTEGER,
		},
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_MIN_VERTICAL_SHEET_FEED_SIZE,
			.type = VT_I4,
		},
		.lis = {
			.name = "dps_min_vertical_sheet_feed_size",
			.type = LIS_TYPE_INTEGER,
		},
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_OPTICAL_XRES, .type = VT_I4, },
		.lis = { .name = "dps_optical_xres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_OPTICAL_YRES, .type = VT_I4, },
		.lis = { .name = "dps_optical_yres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PAD_COLOR, .type = VT_I4, },
		.lis = { .name = "dps_pad_color", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PAGE_HEIGHT, .type = VT_I4, },
		.lis = { .name = "dps_page_height", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PAGE_WIDTH, .type = VT_I4, },
		.lis = { .name = "dps_page_width", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PAGE_SIZE, .type = VT_I4, },
		.lis = { .name = "dps_page_size", .type = LIS_TYPE_STRING },
		.possibles = g_possible_page_sizes,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PLATEN_COLOR, .type = VT_UI1 | VT_VECTOR, },
		.lis = { .name = "dps_platen_color", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_PREVIEW, .type = VT_I4, },
		.lis = { .name = "dps_preview", .type = LIS_TYPE_STRING },
		.possibles = g_possible_previews,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_SCAN_AHEAD_PAGES, .type = VT_I4 },
		.lis = { .name = "dps_scan_ahead_pages", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DPS_SCAN_AVAILABLE_ITEM, .type = VT_I4 },
		.lis = { .name = "dps_scan_available_item", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DPS_SERVICE_ID, .type = VT_BSTR },
		.lis = { .name = "service_id", .type = LIS_TYPE_STRING, },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = {
			.id = WIA_DPS_SHEET_FEEDER_REGISTRATION,
			.type = VT_I4
		},
		.lis = {
			.name = "dps_sheet_feeder_registration",
			.type = LIS_TYPE_STRING,
		},
		.possibles = g_possible_registrations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_SHOW_PREVIEW_CONTROL, .type = VT_I4 },
		.lis = { .name = "dps_show_preview_control", .type = LIS_TYPE_STRING },
		.possibles = g_possible_show_preview,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = LIS_WIA_DPS_USER_NAME, .type = VT_BSTR },
		.lis = { .name = "user_name", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_VERTICAL_BED_REGISTRATION, .type = VT_I4 },
		.lis = { .name = "dps_vertical_bed_registration", .type = LIS_TYPE_STRING },
		.possibles = g_possible_vertical_registrations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_VERTICAL_BED_SIZE, .type = VT_I4 },
		.lis = { .name = "dps_vertical_bed_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_DEVICE,
		.wia = { .id = WIA_DPS_VERTICAL_SHEET_FEED_SIZE, .type = VT_I4 },
		.lis = { .name = "dps_vertical_sheet_feed_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_AUTO_DESKEW, .type = VT_I4 },
		.lis = { .name = "auto_deskew", .type = LIS_TYPE_INTEGER },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = LIS_WIA_AUTO_DESKEW_ON,
				.lis.string = "deskew_on",
			},
			{
				.wia.integer = LIS_WIA_AUTO_DESKEW_OFF,
				.lis.string = "deskew_off",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_BRIGHTNESS, .type = VT_I4 },
		.lis = { .name = "brightness", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_CONTRAST, .type = VT_I4 },
		.lis = { .name = "contrast", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_CUR_INTENT, .type = VT_I4 },
		.lis = { .name = "current_intent", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_INTENT_NONE,
				.lis.string = "none",
			},
			{
				.wia.integer = WIA_INTENT_IMAGE_TYPE_COLOR,
				.lis.string = "image_type_color",
			},
			{
				.wia.integer = WIA_INTENT_IMAGE_TYPE_GRAYSCALE,
				.lis.string = "image_type_grayscale",
			},
			{
				.wia.integer = WIA_INTENT_IMAGE_TYPE_TEXT,
				.lis.string = "image_type_text",
			},
			{
				.wia.integer = WIA_INTENT_IMAGE_TYPE_MASK,
				.lis.string = "image_type_mask",
			},
			{
				.wia.integer = WIA_INTENT_MINIMIZE_SIZE,
				.lis.string = "minimize_size",
			},
			{
				.wia.integer = WIA_INTENT_MAXIMIZE_QUALITY,
				.lis.string = "maximize_quality",
			},
			{
				.wia.integer = WIA_INTENT_SIZE_MASK,
				.lis.string = "size_mask",
			},
			{
				.wia.integer = WIA_INTENT_BEST_PREVIEW,
				.lis.string = "best_preview",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_DESKEW_X, .type = VT_I4, },
		.lis = { .name = "deskew_x", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_DESKEW_Y, .type = VT_I4, },
		.lis = { .name = "deskew_y", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_FILM_NODE_NAME, .type = VT_BSTR },
		.lis = { .name = "film_node_name", .type = LIS_TYPE_STRING },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_FILM_SCAN_MODE, .type = VT_I4 },
		.lis = { .name = "film_scan_mode", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = LIS_WIA_FILM_COLOR_SLIDE,
				.lis.string = "color_slide",
			},
			{
				.wia.integer = LIS_WIA_FILM_COLOR_NEGATIVE,
				.lis.string = "color_negative",
			},
			{
				.wia.integer = LIS_WIA_FILM_BW_NEGATIVE,
				.lis.string = "bw_negative",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_INVERT, .type = VT_I4 },
		.lis = { .name = "invert", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_LAMP, .type = VT_I4 },
		.lis = { .name = "lamp", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = LIS_WIA_LAMP_ON,
				.lis.string = "on"
			},
			{
				.wia.integer = LIS_WIA_LAMP_OFF,
				.lis.string = "off",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_LAMP_AUTO_OFF, .type = VT_I4 },
		.lis = { .name = "lamp_auto_off", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_MAX_HORIZONTAL_SIZE, .type = VT_I4 },
		.lis = { .name = "max_horizontal_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_MAX_VERTICAL_SIZE, .type = VT_I4 },
		.lis = { .name = "max_vertical_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_MIN_HORIZONTAL_SIZE, .type = VT_I4 },
		.lis = { .name = "min_horizontal_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_MIN_VERTICAL_SIZE, .type = VT_I4 },
		.lis = { .name = "min_vertical_size", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_MIRROR, .type = VT_I4 },
		.lis = { .name = "mirror", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_OPTICAL_XRES, .type = VT_I4, },
		.lis = { .name = "optical_xres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_OPTICAL_YRES, .type = VT_I4, },
		.lis = { .name = "optical_yres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_ORIENTATION, .type = VT_I4, },
		.lis = { .name = "orientation", .type = LIS_TYPE_STRING },
		.possibles = g_possible_orientations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PAGE_SIZE, .type = VT_I4, },
		.lis = { .name = "page_size", .type = LIS_TYPE_STRING },
		.possibles = g_possible_page_sizes,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PAGE_WIDTH, .type = VT_I4, },
		.lis = { .name = "page_width", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PAGE_HEIGHT, .type = VT_I4, },
		.lis = { .name = "page_height", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PAGES, .type = VT_I4, },
		.lis = { .name = "pages", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_PHOTOMETRIC_INTERP, .type = VT_I4, },
		.lis = { .name = "photometric_interp", .type = LIS_TYPE_STRING },
		.possibles = (struct lis_wia2lis_possibles[]) {
			{
				.wia.integer = WIA_PHOTO_WHITE_0,
				.lis.string = "white_0",
			},
			{
				.wia.integer = WIA_PHOTO_WHITE_1,
				.lis.string = "white_1",
			},
			{ .eol = 1 },
		},
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PREVIEW, .type = VT_I4, },
		.lis = { .name = "preview", .type = LIS_TYPE_STRING },
		.possibles = g_possible_previews,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_PREVIEW_TYPE, .type = VT_I4, },
		.lis = { .name = "preview_type", .type = LIS_TYPE_STRING },
		.possibles = g_possible_previews,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_ROTATION, .type = VT_I4, },
		.lis = { .name = "rotation", .type = LIS_TYPE_STRING },
		.possibles = g_possible_orientations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_SEGMENTATION, .type = VT_I4, },
		.lis = { .name = "segmentation", .type = LIS_TYPE_STRING },
		.possibles = g_possible_segmentations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = {
			.id = LIS_WIA_IPS_SHEET_FEEDER_REGISTRATION,
			.type = VT_I4
		},
		.lis = {
			.name = "sheet_feeder_registration",
			.type = LIS_TYPE_STRING,
		},
		.possibles = g_possible_registrations,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_SHOW_PREVIEW_CONTROL, .type = VT_I4 },
		.lis = { .name = "show_preview_control", .type = LIS_TYPE_STRING },
		.possibles = g_possible_show_preview
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_SUPPORTS_CHILD_ITEM_CREATION, .type = VT_I4 },
		.lis = { .name = "supports_child_item_creation", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_THRESHOLD, .type = VT_I4 },
		.lis = { .name = "threshold", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_TRANSFER_CAPABILITIES, .type = VT_I4 },
		.lis = { .name = "transfer_capabilities", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_WARM_UP_TIME, .type = VT_I4 },
		.lis = { .name = "warm_up_time", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_XEXTENT, .type = VT_I4 },
		.lis = { .name = "xextent", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_XPOS, .type = VT_I4 },
		.lis = { .name = "xpos", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_XRES, .type = VT_I4 },
		.lis = { .name = "xres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_XSCALING, .type = VT_I4 },
		.lis = { .name = "xscaling", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_YEXTENT, .type = VT_I4 },
		.lis = { .name = "yextent", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_YPOS, .type = VT_I4 },
		.lis = { .name = "ypos", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = WIA_IPS_YRES, .type = VT_I4 },
		.lis = { .name = "yres", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},

	{
		.line = __LINE__,
		.item_type = LIS_PROPERTY_ITEM,
		.wia = { .id = LIS_WIA_IPS_YSCALING, .type = VT_I4 },
		.lis = { .name = "yscaling", .type = LIS_TYPE_INTEGER },
		.possibles = NULL,
	},
};


const struct lis_wia2lis_property *lis_wia2lis_get_property(
		bool root, PROPID propid
	)
{
	enum wia2lis_item_type expected_type = (
		root ? LIS_PROPERTY_DEVICE : LIS_PROPERTY_ITEM
	);
	unsigned int i;
	const struct lis_wia2lis_property *prop;

	// try first to find an exact match
	for (i = 0 ; i < LIS_COUNT_OF(g_wia2lis_properties) ; i++) {
		prop = &g_wia2lis_properties[i];
		if (prop->item_type == expected_type && prop->wia.id == propid) {
			return prop;
		}
	}

	// in case this is some crappy device with only a root item
	// implementing both root and children properties
	for (i = 0 ; i < LIS_COUNT_OF(g_wia2lis_properties) ; i++) {
		prop = &g_wia2lis_properties[i];
		if (prop->wia.id == propid) {
			lis_log_warning(
				"Found property %lu, but on an unexpected "
				" node type (%s instead of %s)",
				propid,
				(root ? "child" : "root"),
				(root ? "root" : "child")
			);
			return prop;
		}
	}

	lis_log_warning(
		"Unknown property %lu (item_type=%s)",
		propid, (root ? "root" : "child")
	);
	return NULL;
}


enum lis_error lis_wia2lis_get_possibles(
		const struct lis_wia2lis_property *in_wia2lis,
		struct lis_value_list *out_list
	)
{
	int i;
	lis_log_debug("Getting possible values for option '%s'", in_wia2lis->lis.name);
	assert(in_wia2lis->lis.type == LIS_TYPE_STRING);

	for (i = 0 ; !in_wia2lis->possibles[i].eol ; i++) { }

	out_list->values = calloc(i, sizeof(union lis_value));
	if (out_list->values == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	out_list->nb_values = i;

	for (i = 0 ; !in_wia2lis->possibles[i].eol ; i++) {
		out_list->values[i].string = in_wia2lis->possibles[i].lis.string;
	}

	return LIS_OK;
}


enum lis_error lis_wia2lis_get_range(
		const struct lis_wia2lis_property *in_wia2lis,
		PROPVARIANT in_propvariant,
		struct lis_value_range *out_range
	)
{
	unsigned int i;

	lis_log_debug("Getting range for option '%s'", in_wia2lis->lis.name);
	switch(in_wia2lis->wia.type) {
		case VT_I4:
			if (in_propvariant.cal.cElems == WIA_RANGE_NUM_ELEMS) {
				out_range->min.integer = in_propvariant.cal.pElems[WIA_RANGE_MIN];
				out_range->max.integer = in_propvariant.cal.pElems[WIA_RANGE_MAX];
				out_range->interval.integer = in_propvariant.cal.pElems[WIA_RANGE_STEP];
				// XXX(Jflesch): WIA_RANGE_NOM ?
				return LIS_OK;
			} else {
				lis_log_error(
					"Unexpected number of elements in"
					" constraint range VT_I4: %lu",
					in_propvariant.cal.cElems
				);
				for (i = 0 ; i < in_propvariant.cal.cElems ; i++) {
					lis_log_debug(
						"- value: %lu",
						in_propvariant.cal.pElems[i]
					);
				}
				return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
			}
			break;
		default:
			break;
	}

	lis_log_error(
		"Unsupported constraint range type: %d", in_wia2lis->wia.type
	);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


enum lis_error lis_wia2lis_get_list(
		const struct lis_wia2lis_property *in_wia2lis,
		PROPVARIANT in_propvariant,
		struct lis_value_list *out_list
	)
{
	unsigned int i;

	lis_log_debug("Getting list for option '%s'", in_wia2lis->lis.name);

	switch(in_wia2lis->wia.type) {
		case VT_I4:
			out_list->values = calloc(in_propvariant.cal.cElems, sizeof(union lis_value));
			if (out_list->values == NULL) {
				lis_log_error("Out of memory");
				return LIS_ERR_NO_MEM;
			}
			out_list->nb_values = in_propvariant.cal.cElems;
			for (i = 0 ; i < in_propvariant.cal.cElems ; i++) {
				out_list->values[i].integer = in_propvariant.cal.pElems[i];
			}
			return LIS_OK;
		case VT_BSTR:
			out_list->values = calloc(in_propvariant.cabstr.cElems, sizeof(union lis_value));
			if (out_list->values == NULL) {
				lis_log_error("Out of memory");
				return LIS_ERR_NO_MEM;
			}
			out_list->nb_values = in_propvariant.cabstr.cElems;
			for (i = 0 ; i < in_propvariant.cabstr.cElems ; i++) {
				out_list->values[i].string = lis_bstr2cstr(
					in_propvariant.cabstr.pElems[i]
				);
				// TODO(Jflesch): out of memory
			}
			return LIS_OK;
		default:
			break;
	}

	lis_log_error(
		"Unsupported constraint list type: %d", in_wia2lis->wia.type
	);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


/* for tests only */

/* for tests */
const struct lis_wia2lis_property *lis_get_all_properties(
		size_t *nb_properties
	)
{
	*nb_properties = LIS_COUNT_OF(g_wia2lis_properties);
	return g_wia2lis_properties;
}
