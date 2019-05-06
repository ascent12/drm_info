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
#include <json-c/json_util.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "config.h"

#define JSON_SCHEMA_VERSION 0

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
	{ "DRM_CLIENT_CAP_STEREO_3D", DRM_CLIENT_CAP_STEREO_3D },
	{ "DRM_CLIENT_CAP_UNIVERSAL_PLANES", DRM_CLIENT_CAP_UNIVERSAL_PLANES },
	{ "DRM_CLIENT_CAP_ATOMIC", DRM_CLIENT_CAP_ATOMIC },
	{ "DRM_CLIENT_CAP_ASPECT_RATIO", DRM_CLIENT_CAP_ASPECT_RATIO },
	{ "DRM_CLIENT_CAP_WRITEBACK_CONNECTORS", DRM_CLIENT_CAP_WRITEBACK_CONNECTORS },
};

static const struct {
	const char *name;
	uint64_t cap;
} caps[] = {
	{ "DRM_CAP_DUMB_BUFFER", DRM_CAP_DUMB_BUFFER },
	{ "DRM_CAP_VBLANK_HIGH_CRTC", DRM_CAP_VBLANK_HIGH_CRTC },
	{ "DRM_CAP_DUMB_PREFERRED_DEPTH", DRM_CAP_DUMB_PREFERRED_DEPTH },
	{ "DRM_CAP_DUMB_PREFER_SHADOW", DRM_CAP_DUMB_PREFER_SHADOW },
	{ "DRM_CAP_PRIME", DRM_CAP_PRIME },
	{ "DRM_CAP_TIMESTAMP_MONOTONIC", DRM_CAP_TIMESTAMP_MONOTONIC },
	{ "DRM_CAP_ASYNC_PAGE_FLIP", DRM_CAP_ASYNC_PAGE_FLIP },
	{ "DRM_CAP_CURSOR_WIDTH", DRM_CAP_CURSOR_WIDTH },
	{ "DRM_CAP_CURSOR_HEIGHT", DRM_CAP_CURSOR_HEIGHT },
	{ "DRM_CAP_ADDFB2_MODIFIERS", DRM_CAP_ADDFB2_MODIFIERS },
	{ "DRM_CAP_PAGE_FLIP_TARGET", DRM_CAP_PAGE_FLIP_TARGET },
	{ "DRM_CAP_CRTC_IN_VBLANK_EVENT", DRM_CAP_CRTC_IN_VBLANK_EVENT },
	{ "DRM_CAP_SYNCOBJ", DRM_CAP_SYNCOBJ },
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
	json_object *obj = json_object_new_int64((int64_t)u);
	json_object_set_serializer(obj, json_object_uint_to_json_string, NULL, NULL);
	return obj;
}

static struct json_object *driver_info(int fd)
{
	struct json_object *obj = json_object_new_object();

	drmVersion *ver = drmGetVersion(fd);
	if (!ver) {
		perror("drmGetVersion");
		return NULL;
	}

	json_object_object_add(obj, "name", json_object_new_string(ver->name));

	struct json_object *ver_obj = json_object_new_object();
	json_object_object_add(ver_obj, "major",
		json_object_new_int(ver->version_major));
	json_object_object_add(ver_obj, "minor",
		json_object_new_int(ver->version_minor));
	json_object_object_add(ver_obj, "patchlevel",
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
		json_object *cap_obj = NULL;
		uint64_t cap;
		if (drmGetCap(fd, caps[i].cap, &cap) == 0) {
			cap_obj = json_object_new_int64(cap);
		}
		json_object_object_add(caps_obj, caps[i].name, cap_obj);
	}
	json_object_object_add(obj, "caps", caps_obj);

	return obj;
}

static struct json_object *properties_info(int fd, uint32_t id, uint32_t type)
{
	struct json_object *obj = json_object_new_object();

	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, id, type);
	if (!props) {
		perror("drmModeObjectGetProperties");
		return NULL;
	}

	for (uint32_t i = 0; i < props->count_props; ++i) {
		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		if (!prop) {
			perror("drmModeGetProperty");
			continue;
		}

		uint32_t flags = prop->flags;

		struct json_object *prop_obj = json_object_new_object();
		json_object_object_add(prop_obj, "id",
			new_json_object_uint64(prop->prop_id));
		json_object_object_add(prop_obj, "flags", new_json_object_uint64(flags));

		struct json_object *values_arr = json_object_new_array();
		for (int j = 0; j < prop->count_values; ++j) {
			json_object_array_add(values_arr,
				new_json_object_uint64(prop->values[j]));
		}
		json_object_object_add(prop_obj, "values", values_arr);

		// TODO
		/*struct json_object *enums_arr = json_object_new_array();
		for (int j = 0; j < prop->count_enums; ++j) {
			json_object_array_add(enums_arr,
				json_object_new_string(prop->enums[j]));
		}
		json_object_object_add(prop_obj, "enums", enums_arr);*/

		// TODO
		/*struct json_object *blobs_arr = json_object_new_array();
		for (int j = 0; j < prop->count_blobs; ++j) {
			json_object_array_add(blobs_arr,
				json_object_new_string(prop->blobs[j]));
		}
		json_object_object_add(prop_obj, "blobs", blobs_arr);*/

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
			(void)mode; // TODO
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
	struct json_object *arr = json_object_new_array();

	drmModePlaneRes *res = drmModeGetPlaneResources(fd);
	if (!res) {
		perror("drmModeGetPlaneResources");
		return NULL;
	}

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

static struct json_object *device_info(const char *path)
{
	struct json_object *obj = json_object_new_object();

	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return NULL;
	}

	// Get driver info before getting resources, as it'll try to enable some
	// DRM client capabilities
	struct json_object *driver = driver_info(fd);
	json_object_object_add(obj, "driver", driver);

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) {
		perror("drmModeGetResources");
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

static struct json_object *drm_info(void)
{
	struct json_object *obj = json_object_new_object();

	json_object_object_add(obj, "version",
		json_object_new_int(JSON_SCHEMA_VERSION));

	char path[PATH_MAX];
	for (int i = 0;; ++i) {
		snprintf(path, sizeof path, DRM_DEV_NAME, DRM_DIR_NAME, i);
		if (access(path, R_OK) < 0)
			break;

		struct json_object *dev = device_info(path);
		json_object_object_add(obj, path, dev);
	}

	return obj;
}

int main(void)
{
	struct json_object *obj = drm_info();
	json_object_to_fd(STDOUT_FILENO, obj,
		JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
	json_object_put(obj);
	return 0;
}
