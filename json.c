#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json-c/json_object.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "config.h"
#include "drm_info.h"

// Defines for comaptibility with old libdrm

// drm.h

#ifndef DRM_CAP_CRTC_IN_VBLANK_EVENT
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#endif

#ifndef DRM_CAP_SYNCOBJ
#define DRM_CAP_SYNCOBJ 0x13
#endif

#ifndef DRM_CLIENT_CAP_ASPECT_RATIO
#define DRM_CLIENT_CAP_ASPECT_RATIO 4
#endif

#ifndef DRM_CLIENT_CAP_WRITEBACK_CONNECTORS
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5
#endif

#ifndef DRM_MODE_CONNECTOR_WRITEBACK
#define DRM_MODE_CONNECTOR_WRITEBACK 18
#endif

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
	}
	json_object_object_add(obj, "device_data", device_data_obj);

	drmFreeDevice(&dev);

	return obj;
}

#if HAVE_LIBDRM_2_4_83
static struct json_object *in_formats_info(int fd, uint32_t blob_id)
{
	struct json_object *arr = json_object_new_array();

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);

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
#else
static struct json_object *in_formats_info(int fd, uint32_t blob_id)
{
	(void)fd;
	(void)blob_id;

	return NULL;
}
#endif

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
	if (blob_id == 0) {
		return NULL;
	}

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);

	drmModeModeInfo *mode = blob->data;

	struct json_object *obj = mode_info(mode);

	drmModeFreePropertyBlob(blob);

	return obj;
}

static struct json_object *writeback_pixel_formats_info(int fd, uint32_t blob_id)
{
	struct json_object *arr = json_object_new_array();

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);

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
	if (blob_id == 0) {
		return NULL;
	}

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, blob_id);

	struct json_object *obj = json_object_new_string_len(blob->data, blob->length);

	drmModeFreePropertyBlob(blob);

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

		struct json_object *value_obj;
		switch (type) {
		// TODO: DRM_MODE_PROP_BLOB
		case DRM_MODE_PROP_SIGNED_RANGE:
			value_obj = json_object_new_int64((int64_t)value);
			break;
		default:
			value_obj = new_json_object_uint64(value);
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
				data_obj = path_info(fd, props->prop_values[i]);
			}
			break;
		case DRM_MODE_PROP_RANGE:
			// This is a special case, as the SRC_* properties are
			// in 16.16 fixed point
			if (strncmp(prop->name, "SRC_", 4) == 0) {
				data_obj = new_json_object_uint64(value >> 16);
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
		char path[PATH_MAX];
		for (int i = 0;; ++i) {
			snprintf(path, sizeof path, DRM_DEV_NAME, DRM_DIR_NAME, i);
			if (access(path, R_OK) < 0)
				break;

			struct json_object *dev = node_info(path);
			if (!dev)
				continue;

			json_object_object_add(obj, path, dev);
		}
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
