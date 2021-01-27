#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <json_object.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm_info.h"

static const struct {
	const char *name;
	uint64_t cap;
} client_caps[] = {
	{ "STEREO_3D", DRM_CLIENT_CAP_STEREO_3D },
	{ "UNIVERSAL_PLANES", DRM_CLIENT_CAP_UNIVERSAL_PLANES },
	{ "ATOMIC", DRM_CLIENT_CAP_ATOMIC },
	{ "ASPECT_RATIO", DRM_CLIENT_CAP_ASPECT_RATIO },
	{ "WRITEBACK_CONNECTORS", DRM_CLIENT_CAP_WRITEBACK_CONNECTORS },
};

static const struct {
	const char *name;
	uint64_t cap;
} caps[] = {
	{ "DUMB_BUFFER", DRM_CAP_DUMB_BUFFER },
	{ "VBLANK_HIGH_CRTC", DRM_CAP_VBLANK_HIGH_CRTC },
	{ "DUMB_PREFERRED_DEPTH", DRM_CAP_DUMB_PREFERRED_DEPTH },
	{ "DUMB_PREFER_SHADOW", DRM_CAP_DUMB_PREFER_SHADOW },
	{ "PRIME", DRM_CAP_PRIME },
	{ "TIMESTAMP_MONOTONIC", DRM_CAP_TIMESTAMP_MONOTONIC },
	{ "ASYNC_PAGE_FLIP", DRM_CAP_ASYNC_PAGE_FLIP },
	{ "CURSOR_WIDTH", DRM_CAP_CURSOR_WIDTH },
	{ "CURSOR_HEIGHT", DRM_CAP_CURSOR_HEIGHT },
	{ "ADDFB2_MODIFIERS", DRM_CAP_ADDFB2_MODIFIERS },
	{ "PAGE_FLIP_TARGET", DRM_CAP_PAGE_FLIP_TARGET },
	{ "CRTC_IN_VBLANK_EVENT", DRM_CAP_CRTC_IN_VBLANK_EVENT },
	{ "SYNCOBJ", DRM_CAP_SYNCOBJ },
	{ "SYNCOBJ_TIMELINE", DRM_CAP_SYNCOBJ_TIMELINE },
};

static int json_object_uint_to_json_string(struct json_object *obj,
		struct printbuf *pb, int level, int flags)
{
	(void)level;
	(void)flags;

	uint64_t u = (uint64_t)json_object_get_int64(obj);

	char buf[21]; // 20 digits + NULL byte
	int len = snprintf(buf, sizeof(buf), "%" PRIu64, u);

	return printbuf_memappend(pb, buf, len);
}

static struct json_object *new_json_object_uint64(uint64_t u)
{
	struct json_object *obj = json_object_new_int64((int64_t)u);
	json_object_set_serializer(obj, json_object_uint_to_json_string, NULL, NULL);
	return obj;
}

static struct json_object *kernel_info(void)
{
	struct utsname utsname;
	if (uname(&utsname) != 0) {
		perror("uname");
		return NULL;
	}

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "sysname",
		json_object_new_string(utsname.sysname));
	json_object_object_add(obj, "release",
		json_object_new_string(utsname.release));
	json_object_object_add(obj, "version",
		json_object_new_string(utsname.version));
	return obj;
}

static struct json_object *driver_info(int fd)
{
	drmVersion *ver = drmGetVersion(fd);
	if (!ver) {
		perror("drmGetVersion");
		return NULL;
	}

	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "name", json_object_new_string(ver->name));
	json_object_object_add(obj, "desc", json_object_new_string(ver->desc));

	struct json_object *ver_obj = json_object_new_object();
	json_object_object_add(ver_obj, "major",
		json_object_new_int(ver->version_major));
	json_object_object_add(ver_obj, "minor",
		json_object_new_int(ver->version_minor));
	json_object_object_add(ver_obj, "patch",
		json_object_new_int(ver->version_patchlevel));
	json_object_object_add(ver_obj, "date",
		json_object_new_string(ver->date));
	json_object_object_add(obj, "version", ver_obj);

	drmFreeVersion(ver);

	json_object_object_add(obj, "kernel", kernel_info());

	struct json_object *client_caps_obj = json_object_new_object();
	for (size_t i = 0; i < sizeof(client_caps) / sizeof(client_caps[0]); ++i) {
		bool supported = drmSetClientCap(fd, client_caps[i].cap, 1) == 0;
		json_object_object_add(client_caps_obj, client_caps[i].name,
			json_object_new_boolean(supported));
	}
	json_object_object_add(obj, "client_caps", client_caps_obj);

	struct json_object *caps_obj = json_object_new_object();
	for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i) {
		struct json_object *cap_obj = NULL;
		uint64_t cap;
		if (drmGetCap(fd, caps[i].cap, &cap) == 0) {
			cap_obj = json_object_new_int64(cap);
		}
		json_object_object_add(caps_obj, caps[i].name, cap_obj);
	}
	json_object_object_add(obj, "caps", caps_obj);

	return obj;
}

static struct json_object *device_info(int fd)
{
	drmDevice *dev;
	if (drmGetDevice(fd, &dev) != 0) {
		perror("drmGetDevice");
		return NULL;
	}

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "available_nodes",
		new_json_object_uint64(dev->available_nodes));
	json_object_object_add(obj, "bus_type",
		new_json_object_uint64(dev->bustype));

	struct json_object *device_data_obj = NULL;
	switch (dev->bustype) {
	case DRM_BUS_PCI:;
		drmPciDeviceInfo *pci = dev->deviceinfo.pci;
		device_data_obj = json_object_new_object();
		json_object_object_add(device_data_obj, "vendor",
			new_json_object_uint64(pci->vendor_id));
		json_object_object_add(device_data_obj, "device",
			new_json_object_uint64(pci->device_id));
		json_object_object_add(device_data_obj, "subsystem_vendor",
			new_json_object_uint64(pci->subvendor_id));
		json_object_object_add(device_data_obj, "subsystem_device",
			new_json_object_uint64(pci->subdevice_id));
		break;
	case DRM_BUS_USB:;
		drmUsbDeviceInfo *usb = dev->deviceinfo.usb;
		device_data_obj = json_object_new_object();
		json_object_object_add(device_data_obj, "vendor",
			new_json_object_uint64(usb->vendor));
		json_object_object_add(device_data_obj, "product",
			new_json_object_uint64(usb->product));
		break;
	case DRM_BUS_PLATFORM:;
		drmPlatformDeviceInfo *platform = dev->deviceinfo.platform;
		device_data_obj = json_object_new_object();
		struct json_object *compatible_arr = json_object_new_array();
		for (size_t i = 0; platform->compatible[i]; ++i)
			json_object_array_add(compatible_arr,
				json_object_new_string(platform->compatible[i]));
		json_object_object_add(device_data_obj, "compatible", compatible_arr);
		break;
	}
	json_object_object_add(obj, "device_data", device_data_obj);

	drmFreeDevice(&dev);

	return obj;
}

static struct json_object *in_formats_info(int fd, uint32_t blob_id)
{
	struct json_object *arr = json_object_new_array();

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);
	if (!blob) {
		perror("drmModeGetPropertyBlob");
		return NULL;
	}

	struct drm_format_modifier_blob *data = blob->data;

	uint32_t *fmts = (uint32_t *)
		((char *)data + data->formats_offset);

	struct drm_format_modifier *mods = (struct drm_format_modifier *)
		((char *)data + data->modifiers_offset);

	for (uint32_t i = 0; i < data->count_modifiers; ++i) {
		struct json_object *mod_obj = json_object_new_object();
		json_object_object_add(mod_obj, "modifier",
			new_json_object_uint64(mods[i].modifier));

		struct json_object *fmts_arr = json_object_new_array();
		for (uint64_t j = 0; j < 64; ++j) {
			if (mods[i].formats & (1ull << j)) {
				uint32_t fmt = fmts[j + mods[i].offset];
				json_object_array_add(fmts_arr, new_json_object_uint64(fmt));
			}
		}
		json_object_object_add(mod_obj, "formats", fmts_arr);

		json_object_array_add(arr, mod_obj);
	}

	drmModeFreePropertyBlob(blob);

	return arr;
}

static struct json_object *mode_info(const drmModeModeInfo *mode)
{
	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "clock", new_json_object_uint64(mode->clock));

	json_object_object_add(obj, "hdisplay", new_json_object_uint64(mode->hdisplay));
	json_object_object_add(obj, "hsync_start", new_json_object_uint64(mode->hsync_start));
	json_object_object_add(obj, "hsync_end", new_json_object_uint64(mode->hsync_end));
	json_object_object_add(obj, "htotal", new_json_object_uint64(mode->htotal));
	json_object_object_add(obj, "hskew", new_json_object_uint64(mode->hskew));

	json_object_object_add(obj, "vdisplay", new_json_object_uint64(mode->vdisplay));
	json_object_object_add(obj, "vsync_start", new_json_object_uint64(mode->vsync_start));
	json_object_object_add(obj, "vsync_end", new_json_object_uint64(mode->vsync_end));
	json_object_object_add(obj, "vtotal", new_json_object_uint64(mode->vtotal));
	json_object_object_add(obj, "vscan", new_json_object_uint64(mode->vscan));

	json_object_object_add(obj, "vrefresh", new_json_object_uint64(mode->vrefresh));

	json_object_object_add(obj, "flags", new_json_object_uint64(mode->flags));
	json_object_object_add(obj, "type", new_json_object_uint64(mode->type));
	json_object_object_add(obj, "name", json_object_new_string(mode->name));

	return obj;
}

static struct json_object *mode_id_info(int fd, uint32_t blob_id)
{
	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);
	if (!blob) {
		perror("drmModeGetPropertyBlob");
		return NULL;
	}

	drmModeModeInfo *mode = blob->data;

	struct json_object *obj = mode_info(mode);

	drmModeFreePropertyBlob(blob);

	return obj;
}

static struct json_object *writeback_pixel_formats_info(int fd, uint32_t blob_id)
{
	struct json_object *arr = json_object_new_array();

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);
	if (!blob) {
		perror("drmModeGetPropertyBlob");
		return NULL;
	}

	uint32_t *fmts = blob->data;
	uint32_t fmts_len = blob->length / sizeof(uint32_t);
	for (uint32_t i = 0; i < fmts_len; ++i) {
		json_object_array_add(arr, new_json_object_uint64(fmts[i]));
	}

	drmModeFreePropertyBlob(blob);

	return arr;
}

static struct json_object *path_info(int fd, uint32_t blob_id)
{
	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);
	if (!blob) {
		perror("drmModeGetPropertyBlob");
		return NULL;
	}

	struct json_object *obj = json_object_new_string_len(blob->data, blob->length);

	drmModeFreePropertyBlob(blob);

	return obj;
}

static struct json_object *fb_info(int fd, uint32_t id)
{
#ifdef HAVE_GETFB2
	drmModeFB2 *fb2 = drmModeGetFB2(fd, id);
	if (!fb2 && errno != EINVAL) {
		perror("drmModeGetFB2");
		return NULL;
	}
	if (fb2) {
		struct json_object *obj = json_object_new_object();
		json_object_object_add(obj, "id", new_json_object_uint64(fb2->fb_id));
		json_object_object_add(obj, "width", new_json_object_uint64(fb2->width));
		json_object_object_add(obj, "height", new_json_object_uint64(fb2->height));

		json_object_object_add(obj, "format", new_json_object_uint64(fb2->pixel_format));
		if (fb2->flags & DRM_MODE_FB_MODIFIERS) {
			json_object_object_add(obj, "modifier", new_json_object_uint64(fb2->modifier));
		}

		struct json_object *planes_arr = json_object_new_array();
		json_object_object_add(obj, "planes", planes_arr);

		for (size_t i = 0; i < sizeof(fb2->pitches) / sizeof(fb2->pitches[0]); i++) {
			if (!fb2->pitches[i])
				continue;

			struct json_object *plane_obj = json_object_new_object();
			json_object_array_add(planes_arr, plane_obj);

			json_object_object_add(plane_obj, "offset",
				new_json_object_uint64(fb2->offsets[i]));
			json_object_object_add(plane_obj, "pitch",
				new_json_object_uint64(fb2->pitches[i]));
		}

		drmModeFreeFB2(fb2);

		return obj;
	}
#endif

	// Fallback to drmModeGetFB is drmModeGetFB2 isn't available
	drmModeFB *fb = drmModeGetFB(fd, id);
	if (!fb) {
		perror("drmModeGetFB");
		return NULL;
	}

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "id", new_json_object_uint64(fb->fb_id));
	json_object_object_add(obj, "width", new_json_object_uint64(fb->width));
	json_object_object_add(obj, "height", new_json_object_uint64(fb->height));

	// Legacy properties
	json_object_object_add(obj, "pitch", new_json_object_uint64(fb->pitch));
	json_object_object_add(obj, "bpp", new_json_object_uint64(fb->bpp));
	json_object_object_add(obj, "depth", new_json_object_uint64(fb->depth));

	drmModeFreeFB(fb);

	return obj;
}


static struct json_object *properties_info(int fd, uint32_t id, uint32_t type)
{
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, id, type);
	if (!props) {
		perror("drmModeObjectGetProperties");
		return NULL;
	}

	struct json_object *obj = json_object_new_object();

	for (uint32_t i = 0; i < props->count_props; ++i) {
		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		if (!prop) {
			perror("drmModeGetProperty");
			continue;
		}

		uint32_t flags = prop->flags;
		uint32_t type = flags &
			(DRM_MODE_PROP_LEGACY_TYPE | DRM_MODE_PROP_EXTENDED_TYPE);
		bool atomic = flags & DRM_MODE_PROP_ATOMIC;
		bool immutable = flags & DRM_MODE_PROP_IMMUTABLE;
		uint64_t value = props->prop_values[i];

		struct json_object *prop_obj = json_object_new_object();
		json_object_object_add(prop_obj, "id",
			new_json_object_uint64(prop->prop_id));
		json_object_object_add(prop_obj, "flags",
			new_json_object_uint64(flags));
		json_object_object_add(prop_obj, "type", new_json_object_uint64(type));
		json_object_object_add(prop_obj, "atomic",
			json_object_new_boolean(atomic));
		json_object_object_add(prop_obj, "immutable",
			json_object_new_boolean(immutable));

		json_object_object_add(prop_obj, "raw_value",
			new_json_object_uint64(value));

		struct json_object *spec_obj = NULL;
		switch (type) {
		case DRM_MODE_PROP_RANGE:
			spec_obj = json_object_new_object();
			json_object_object_add(spec_obj, "min",
				new_json_object_uint64(prop->values[0]));
			json_object_object_add(spec_obj, "max",
				new_json_object_uint64(prop->values[1]));
			break;
		case DRM_MODE_PROP_ENUM:
		case DRM_MODE_PROP_BITMASK:
			spec_obj = json_object_new_array();
			for (int j = 0; j < prop->count_enums; ++j) {
				struct json_object *item_obj = json_object_new_object();
				json_object_object_add(item_obj, "name",
					json_object_new_string(prop->enums[j].name));
				json_object_object_add(item_obj, "value",
					new_json_object_uint64(prop->enums[j].value));
				json_object_array_add(spec_obj, item_obj);
			}
			break;
		case DRM_MODE_PROP_OBJECT:
			spec_obj = new_json_object_uint64(prop->values[0]);
			break;
		case DRM_MODE_PROP_SIGNED_RANGE:
			spec_obj = json_object_new_object();
			json_object_object_add(spec_obj, "min",
				json_object_new_int64((int64_t)prop->values[0]));
			json_object_object_add(spec_obj, "max",
				json_object_new_int64((int64_t)prop->values[1]));
			break;
		}
		json_object_object_add(prop_obj, "spec", spec_obj);

		struct json_object *value_obj = NULL;
		switch (type) {
		case DRM_MODE_PROP_RANGE:
		case DRM_MODE_PROP_ENUM:
		case DRM_MODE_PROP_BITMASK:
		case DRM_MODE_PROP_OBJECT:
			value_obj = new_json_object_uint64(value);
			break;
		case DRM_MODE_PROP_BLOB:
			// TODO: base64-encode blob contents
			value_obj = NULL;
			break;
		case DRM_MODE_PROP_SIGNED_RANGE:
			value_obj = json_object_new_int64((int64_t)value);
			break;
		}
		json_object_object_add(prop_obj, "value", value_obj);

		struct json_object *data_obj = NULL;
		switch (type) {
		case DRM_MODE_PROP_BLOB:
			if (!value) {
				break;
			}
			if (strcmp(prop->name, "IN_FORMATS") == 0) {
				data_obj = in_formats_info(fd, value);
			} else if (strcmp(prop->name, "MODE_ID") == 0) {
				data_obj = mode_id_info(fd, value);
			} else if (strcmp(prop->name, "WRITEBACK_PIXEL_FORMATS") == 0) {
				data_obj = writeback_pixel_formats_info(fd, value);
			} else if (strcmp(prop->name, "PATH") == 0) {
				data_obj = path_info(fd, value);
			}
			break;
		case DRM_MODE_PROP_RANGE:
			// This is a special case, as the SRC_* properties are
			// in 16.16 fixed point
			if (strncmp(prop->name, "SRC_", 4) == 0) {
				data_obj = new_json_object_uint64(value >> 16);
			}
			break;
		case DRM_MODE_PROP_OBJECT:
			if (!value) {
				break;
			}
			if (strcmp(prop->name, "FB_ID") == 0) {
				data_obj = fb_info(fd, value);
			}
			break;
		}
		json_object_object_add(prop_obj, "data", data_obj);

		json_object_object_add(obj, prop->name, prop_obj);

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);

	return obj;
}

static struct json_object *connectors_info(int fd, drmModeRes *res)
{
	struct json_object *arr = json_object_new_array();

	for (int i = 0; i < res->count_connectors; ++i) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) {
			perror("drmModeGetConnector");
			continue;
		}

		struct json_object *conn_obj = json_object_new_object();

		json_object_object_add(conn_obj, "id",
			new_json_object_uint64(conn->connector_id));
		json_object_object_add(conn_obj, "type",
			new_json_object_uint64(conn->connector_type));
		json_object_object_add(conn_obj, "status",
			new_json_object_uint64(conn->connection));
		json_object_object_add(conn_obj, "phy_width",
			new_json_object_uint64(conn->mmWidth));
		json_object_object_add(conn_obj, "phy_height",
			new_json_object_uint64(conn->mmHeight));
		json_object_object_add(conn_obj, "subpixel",
			new_json_object_uint64(conn->subpixel));
		json_object_object_add(conn_obj, "encoder_id",
			new_json_object_uint64(conn->encoder_id));

		struct json_object *encoders_arr = json_object_new_array();
		for (int j = 0; j < conn->count_encoders; ++j) {
			json_object_array_add(encoders_arr,
				new_json_object_uint64(conn->encoders[j]));
		}
		json_object_object_add(conn_obj, "encoders", encoders_arr);

		struct json_object *modes_arr = json_object_new_array();
		for (int j = 0; j < conn->count_modes; ++j) {
			const drmModeModeInfo *mode = &conn->modes[j];
			json_object_array_add(modes_arr, mode_info(mode));
		}
		json_object_object_add(conn_obj, "modes", modes_arr);

		struct json_object *props_obj = properties_info(fd,
			conn->connector_id, DRM_MODE_OBJECT_CONNECTOR);
		json_object_object_add(conn_obj, "properties", props_obj);

		drmModeFreeConnector(conn);

		json_object_array_add(arr, conn_obj);
	}

	return arr;
}

static struct json_object *encoders_info(int fd, drmModeRes *res)
{
	struct json_object *arr = json_object_new_array();

	for (int i = 0; i < res->count_encoders; ++i) {
		drmModeEncoder *enc = drmModeGetEncoder(fd, res->encoders[i]);
		if (!enc) {
			perror("drmModeGetEncoder");
			continue;
		}

		struct json_object *enc_obj = json_object_new_object();

		json_object_object_add(enc_obj, "id",
			new_json_object_uint64(enc->encoder_id));
		json_object_object_add(enc_obj, "type",
			new_json_object_uint64(enc->encoder_type));
		json_object_object_add(enc_obj, "crtc_id",
			new_json_object_uint64(enc->crtc_id));
		json_object_object_add(enc_obj, "possible_crtcs",
			new_json_object_uint64(enc->possible_crtcs));
		json_object_object_add(enc_obj, "possible_clones",
			new_json_object_uint64(enc->possible_clones));

		drmModeFreeEncoder(enc);

		json_object_array_add(arr, enc_obj);
	}

	return arr;
}

static struct json_object *crtcs_info(int fd, drmModeRes *res)
{
	struct json_object *arr = json_object_new_array();

	for (int i = 0; i < res->count_crtcs; ++i) {
		drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		if (!crtc) {
			perror("drmModeGetCrtc");
			continue;
		}

		struct json_object *crtc_obj = json_object_new_object();

		json_object_object_add(crtc_obj, "id",
			new_json_object_uint64(crtc->crtc_id));
		json_object_object_add(crtc_obj, "fb_id",
			new_json_object_uint64(crtc->buffer_id));
		json_object_object_add(crtc_obj, "x",
			new_json_object_uint64(crtc->x));
		json_object_object_add(crtc_obj, "y",
			new_json_object_uint64(crtc->y));
		if (crtc->mode_valid) {
			json_object_object_add(crtc_obj, "mode", mode_info(&crtc->mode));
		} else {
			json_object_object_add(crtc_obj, "mode", NULL);
		}
		json_object_object_add(crtc_obj, "gamma_size",
			json_object_new_int(crtc->gamma_size));

		struct json_object *props_obj = properties_info(fd,
			crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
		json_object_object_add(crtc_obj, "properties", props_obj);

		drmModeFreeCrtc(crtc);

		json_object_array_add(arr, crtc_obj);
	}

	return arr;
}

static struct json_object *planes_info(int fd)
{
	drmModePlaneRes *res = drmModeGetPlaneResources(fd);
	if (!res) {
		perror("drmModeGetPlaneResources");
		return NULL;
	}

	struct json_object *arr = json_object_new_array();

	for (uint32_t i = 0; i < res->count_planes; ++i) {
		drmModePlane *plane = drmModeGetPlane(fd, res->planes[i]);
		if (!plane) {
			perror("drmModeGetPlane");
			continue;
		}

		struct json_object *plane_obj = json_object_new_object();

		json_object_object_add(plane_obj, "id",
			new_json_object_uint64(plane->plane_id));
		json_object_object_add(plane_obj, "possible_crtcs",
			new_json_object_uint64(plane->possible_crtcs));
		json_object_object_add(plane_obj, "crtc_id",
			new_json_object_uint64(plane->crtc_id));
		json_object_object_add(plane_obj, "fb_id",
			new_json_object_uint64(plane->fb_id));
		json_object_object_add(plane_obj, "crtc_x",
			new_json_object_uint64(plane->crtc_x));
		json_object_object_add(plane_obj, "crtc_y",
			new_json_object_uint64(plane->crtc_y));
		json_object_object_add(plane_obj, "x",
			new_json_object_uint64(plane->x));
		json_object_object_add(plane_obj, "y",
			new_json_object_uint64(plane->y));
		json_object_object_add(plane_obj, "gamma_size",
			new_json_object_uint64(plane->gamma_size));

		json_object_object_add(plane_obj, "fb",
			plane->fb_id ? fb_info(fd, plane->fb_id) : NULL);

		struct json_object *formats_arr = json_object_new_array();
		for (uint32_t j = 0; j < plane->count_formats; ++j) {
			json_object_array_add(formats_arr,
				new_json_object_uint64(plane->formats[j]));
		}
		json_object_object_add(plane_obj, "formats", formats_arr);

		struct json_object *props_obj = properties_info(fd,
			plane->plane_id, DRM_MODE_OBJECT_PLANE);
		json_object_object_add(plane_obj, "properties", props_obj);

		drmModeFreePlane(plane);

		json_object_array_add(arr, plane_obj);
	}

	drmModeFreePlaneResources(res);

	return arr;
}

static struct json_object *node_info(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return NULL;
	}

	struct json_object *obj = json_object_new_object();

	// Get driver info before getting resources, as it'll try to enable some
	// DRM client capabilities
	json_object_object_add(obj, "driver", driver_info(fd));

	json_object_object_add(obj, "device", device_info(fd));

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) {
		perror("drmModeGetResources");
		close(fd);
		json_object_put(obj);
		return NULL;
	}

	struct json_object *fb_size_obj = json_object_new_object();
	json_object_object_add(fb_size_obj, "min_width",
		new_json_object_uint64(res->min_width));
	json_object_object_add(fb_size_obj, "max_width",
		new_json_object_uint64(res->max_width));
	json_object_object_add(fb_size_obj, "min_height",
		new_json_object_uint64(res->min_height));
	json_object_object_add(fb_size_obj, "max_height",
		new_json_object_uint64(res->max_height));
	json_object_object_add(obj, "fb_size", fb_size_obj);

	json_object_object_add(obj, "connectors", connectors_info(fd, res));
	json_object_object_add(obj, "encoders", encoders_info(fd, res));
	json_object_object_add(obj, "crtcs", crtcs_info(fd, res));
	json_object_object_add(obj, "planes", planes_info(fd));

	drmModeFreeResources(res);

	close(fd);

	return obj;
}

/* paths is a NULL terminated argv array */
struct json_object *drm_info(char *paths[])
{
	struct json_object *obj = json_object_new_object();

	/* Print everything by default */
	if (!paths[0]) {
		drmDevice *devices[64];
		int n = drmGetDevices(devices, sizeof(devices) / sizeof(devices[0]));
		if (n < 0) {
			perror("drmGetDevices");
			json_object_put(obj);
			return NULL;
		}

		for (int i = 0; i < n; ++i) {
			drmDevice *dev = devices[i];
			if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY)))
				continue;

			const char *path = dev->nodes[DRM_NODE_PRIMARY];
			struct json_object *dev_obj = node_info(path);
			if (!dev_obj) {
				fprintf(stderr, "Failed to retrieve information from %s\n", path);
				continue;
			}

			json_object_object_add(obj, path, dev_obj);
		}

		drmFreeDevices(devices, n);
	} else {
		for (char **path = paths; *path; ++path) {
			struct json_object *dev = node_info(*path);
			if (!dev)
				continue;

			json_object_object_add(obj, *path, dev);
		}
	}

	return obj;
}
