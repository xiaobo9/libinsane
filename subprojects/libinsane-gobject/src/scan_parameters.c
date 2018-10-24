#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include <libinsane-gobject/scan_parameters.h>


struct _LibinsaneScanParametersPrivate
{
	struct lis_scan_parameters parameters;
};

G_DEFINE_TYPE_WITH_PRIVATE(LibinsaneScanParameters, libinsane_scan_parameters, G_TYPE_OBJECT)


static void libinsane_scan_parameters_finalize(GObject *object)
{
	LIS_UNUSED(object);
	lis_log_debug("[gobject] Finalizing");
}


static void libinsane_scan_parameters_class_init(LibinsaneScanParametersClass *cls)
{
	GObjectClass *go_cls;
	go_cls = G_OBJECT_CLASS(cls);
	go_cls->finalize = libinsane_scan_parameters_finalize;
}


static void libinsane_scan_parameters_init(LibinsaneScanParameters *self)
{
	LIS_UNUSED(self);
	lis_log_debug("[gobject] Initializing");
}


LibinsaneScanParameters *libinsane_scan_parameters_new_from_libinsane(struct lis_scan_parameters *lis_params)
{
	LibinsaneScanParameters *params;
	LibinsaneScanParametersPrivate *private;

	lis_log_debug("[gobject] enter");
	params = g_object_new(LIBINSANE_SCAN_PARAMETERS_TYPE, NULL);
	private = libinsane_scan_parameters_get_instance_private(params);
	memcpy(&private->parameters, lis_params, sizeof(private->parameters));
	lis_log_debug("[gobject] leave");

	return params;
}


LibinsaneImgFormat libinsane_scan_parameters_get_format(LibinsaneScanParameters *self)
{
	LibinsaneScanParametersPrivate *private = libinsane_scan_parameters_get_instance_private(self);

	switch(private->parameters.format) {
		case LIS_IMG_FORMAT_RAW_RGB_24:
			return LIBINSANE_IMG_FORMAT_RAW_RGB_24;
		case LIS_IMG_FORMAT_GRAYSCALE_8:
			return LIBINSANE_IMG_FORMAT_GRAYSCALE_8;
		case LIS_IMG_FORMAT_BW_1:
			return LIBINSANE_IMG_FORMAT_BW_1;
		case LIS_IMG_FORMAT_BMP:
			return LIBINSANE_IMG_FORMAT_BMP;
		case LIS_IMG_FORMAT_GIF:
			return LIBINSANE_IMG_FORMAT_GIF;
		case LIS_IMG_FORMAT_JPEG:
			return LIBINSANE_IMG_FORMAT_JPEG;
		case LIS_IMG_FORMAT_PNG:
			return LIBINSANE_IMG_FORMAT_PNG;
		case LIS_IMG_FORMAT_CIFF:
			return LIBINSANE_IMG_FORMAT_CIFF;
		case LIS_IMG_FORMAT_EXIF:
			return LIBINSANE_IMG_FORMAT_EXIF;
		case LIS_IMG_FORMAT_FLASHPIX:
			return LIBINSANE_IMG_FORMAT_FLASHPIX;
		case LIS_IMG_FORMAT_ICO:
			return LIBINSANE_IMG_FORMAT_ICO;
		case LIS_IMG_FORMAT_JPEG2K:
			return LIBINSANE_IMG_FORMAT_JPEG2K;
		case LIS_IMG_FORMAT_JPEG2KX:
			return LIBINSANE_IMG_FORMAT_JPEG2KX;
		case LIS_IMG_FORMAT_MEMORYBMP:
			return LIBINSANE_IMG_FORMAT_MEMORYBMP;
		case LIS_IMG_FORMAT_PHOTOCD:
			return LIBINSANE_IMG_FORMAT_PHOTOCD;
		case LIS_IMG_FORMAT_PICT:
			return LIBINSANE_IMG_FORMAT_PICT;
		case LIS_IMG_FORMAT_TIFF:
			return LIBINSANE_IMG_FORMAT_TIFF;
	}

	lis_log_warning("Unknown image format: %d", private->parameters.format);
	return LIBINSANE_IMG_FORMAT_RAW_RGB_24;
}


int libinsane_scan_parameters_get_width(LibinsaneScanParameters *self)
{
	LibinsaneScanParametersPrivate *private;
	private = libinsane_scan_parameters_get_instance_private(self);
	return private->parameters.width;
}


int libinsane_scan_parameters_get_height(LibinsaneScanParameters *self)
{
	LibinsaneScanParametersPrivate *private;
	private = libinsane_scan_parameters_get_instance_private(self);
	return private->parameters.height;
}


gsize libinsane_scan_parameters_get_image_size(LibinsaneScanParameters *self)
{
	LibinsaneScanParametersPrivate *private;
	private = libinsane_scan_parameters_get_instance_private(self);
	return private->parameters.image_size;
}
