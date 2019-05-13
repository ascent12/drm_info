#include <stdio.h>

#include <json-c/json.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "config.h"
#include "drm_info.h"

#define L_LINE "│   "
#define L_VAL  "├───"
#define L_LAST "└───"
#define L_GAP  "    "

static uint64_t get_object_uint64(struct json_object *obj)
{
	return (uint64_t)json_object_get_int64(obj);
}

static const char *get_object_object_string(struct json_object *obj,
		const char *key)
{
	struct json_object *str_obj = json_object_object_get(obj, key);
	if (!str_obj) {
		return NULL;
	}
	return json_object_get_string(str_obj);
}

static uint64_t get_object_object_uint64(struct json_object *obj,
		const char *key)
{
	struct json_object *uint64_obj = json_object_object_get(obj, key);
	if (!uint64_obj) {
		return 0;
	}
	return get_object_uint64(uint64_obj);
}

void print_driver(struct json_object *obj)
{
	const char *name = get_object_object_string(obj, "name");
	const char *desc = get_object_object_string(obj, "desc");
	struct json_object *version_obj = json_object_object_get(obj, "version");
	int version_major = get_object_object_uint64(version_obj, "major");
	int version_minor = get_object_object_uint64(version_obj, "minor");
	int version_patch = get_object_object_uint64(version_obj, "patch");
	const char *version_date = get_object_object_string(version_obj, "date");

	printf(L_VAL "Driver: %s (%s) version %d.%d.%d (%s)\n", name, desc,
		version_major, version_minor, version_patch, version_date);

	struct json_object_iter iter;
	struct json_object *client_caps_obj =
		json_object_object_get(obj, "client_caps");
	json_object_object_foreachC(client_caps_obj, iter) {
		json_bool supported = json_object_get_boolean(iter.val);
		printf(L_LINE L_VAL "DRM_CLIENT_CAP_%s %s\n", iter.key,
			supported ? "supported" : "not supported");
	}

	struct json_object *caps_obj = json_object_object_get(obj, "caps");
	json_object_object_foreachC(caps_obj, iter) {
		printf(!iter.entry->next ? L_LINE L_LAST : L_LINE L_VAL);
		if (iter.val == NULL) {
			printf("DRM_CAP_%s not supported\n", iter.key);
		} else {
			printf("DRM_CAP_%s = %"PRIu64"\n", iter.key,
				get_object_uint64(iter.val));
		}
	}
}

static const char *bustype_str(int type)
{
	switch (type) {
	case DRM_BUS_PCI:      return "PCI";
	case DRM_BUS_USB:      return "USB";
	case DRM_BUS_PLATFORM: return "platform";
	case DRM_BUS_HOST1X:   return "host1x";
	default:               return "unknown";
	}
}

void print_device(struct json_object *obj)
{
	int bus_type = get_object_object_uint64(obj, "bus_type");
	struct json_object *data_obj = json_object_object_get(obj, "device_data");

	printf(L_VAL "Device: %s", bustype_str(bus_type));
	switch (bus_type) {
	case DRM_BUS_PCI:;
		uint16_t vendor = get_object_object_uint64(data_obj, "vendor");
		uint16_t device = get_object_object_uint64(data_obj, "device");
		printf(" %04x:%04x", vendor, device);
		break;
	}
	printf("\n");
}

void print_node(const char *path, struct json_object *obj)
{
	printf("Node: %s\n", path);
	print_driver(json_object_object_get(obj, "driver"));
	print_device(json_object_object_get(obj, "device"));
}

void print_drm(struct json_object *obj)
{
	json_object_object_foreach(obj, path, node_obj) {
		print_node(path, node_obj);
	}
}
