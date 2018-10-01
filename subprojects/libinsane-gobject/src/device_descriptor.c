#include <libinsane/log.h>
#include <libinsane/util.h>

#include <libinsane-gobject/device_descriptor.h>
#include <libinsane-gobject/device_descriptor_private.h>


struct _LibinsaneDeviceDescriptorPrivate
{
	char *dev_id;
	char *vendor;
	char *model;
	char *type;
	char *to_string;
};

G_DEFINE_TYPE_WITH_PRIVATE(LibinsaneDeviceDescriptor, libinsane_device_descriptor, G_TYPE_OBJECT)


static void libinsane_device_descriptor_finalize(GObject *object)
{
	LibinsaneDeviceDescriptor *self = LIBINSANE_DEVICE_DESCRIPTOR(object);
	LibinsaneDeviceDescriptorPrivate *private = \
		libinsane_device_descriptor_get_instance_private(self);

	lis_log_debug("[gobject] Finalizing");
	g_free(private->dev_id);
	g_free(private->vendor);
	g_free(private->model);
	g_free(private->type);
	g_free(private->to_string);
}


static void libinsane_device_descriptor_class_init(LibinsaneDeviceDescriptorClass *cls)
{
	GObjectClass *go_cls;
	go_cls = G_OBJECT_CLASS(cls);
	go_cls->finalize = libinsane_device_descriptor_finalize;
}


static void libinsane_device_descriptor_init(LibinsaneDeviceDescriptor *self)
{
	LIS_UNUSED(self);
	lis_log_debug("[gobject] Initializing");
}

LibinsaneDeviceDescriptor *libinsane_device_descriptor_new_from_libinsane(
		const struct lis_device_descriptor *lis_desc
	)
{
	LibinsaneDeviceDescriptor *desc;
	LibinsaneDeviceDescriptorPrivate *private;

	lis_log_debug("[gobject] enter");
	desc = g_object_new(LIBINSANE_DEVICE_DESCRIPTOR_TYPE, NULL);
	private = libinsane_device_descriptor_get_instance_private(desc);
	private->dev_id = g_strdup(lis_desc->dev_id);
	private->vendor = g_strdup(lis_desc->vendor);
	private->model = g_strdup(lis_desc->model);
	private->type = g_strdup(lis_desc->type);
	private->to_string = g_strdup_printf(
		"%s %s (%s ; %s)",
		private->vendor, private->model, private->type, private->dev_id
	);
	lis_log_debug("[gobject] leave");
	return desc;
}


const char *libinsane_device_descriptor_get_dev_id(LibinsaneDeviceDescriptor *self)
{
	LibinsaneDeviceDescriptorPrivate *private;
	private = libinsane_device_descriptor_get_instance_private(self);
	return private->dev_id;
}


const char *libinsane_device_descriptor_get_dev_vendor(LibinsaneDeviceDescriptor *self)
{
	LibinsaneDeviceDescriptorPrivate *private;
	private = libinsane_device_descriptor_get_instance_private(self);
	return private->vendor;
}


const char *libinsane_device_descriptor_get_dev_model(LibinsaneDeviceDescriptor *self)
{
	LibinsaneDeviceDescriptorPrivate *private;
	private = libinsane_device_descriptor_get_instance_private(self);
	return private->model;
}


const char *libinsane_device_descriptor_get_dev_type(LibinsaneDeviceDescriptor *self)
{
	LibinsaneDeviceDescriptorPrivate *private;
	private = libinsane_device_descriptor_get_instance_private(self);
	return private->type;
}

/**
 * libinsane_device_descriptor_to_string:
 * Convenience method: allow to see quickly which device is designated by this object.
 * Do not use in production code.
 */
const char *libinsane_device_descriptor_to_string(LibinsaneDeviceDescriptor *self)
{
	LibinsaneDeviceDescriptorPrivate *private;
	private = libinsane_device_descriptor_get_instance_private(self);
	return private->to_string;
}
