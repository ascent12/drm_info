#include <stdbool.h>
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

static void print_driver(struct json_object *obj)
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

static void print_device(struct json_object *obj)
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

// The refresh rate provided by the mode itself is innacurate,
// so we calculate it ourself.
static int32_t refresh_rate(struct json_object *obj) {
	int clock = get_object_object_uint64(obj, "clock");
	int htotal = get_object_object_uint64(obj, "htotal");
	int vtotal = get_object_object_uint64(obj, "vtotal");
	int vscan = get_object_object_uint64(obj, "vscan");
	int flags = get_object_object_uint64(obj, "flags");

	int32_t refresh = (clock * 1000000LL / htotal +
		vtotal / 2) / vtotal;

	if (flags & DRM_MODE_FLAG_INTERLACE)
		refresh *= 2;

	if (flags & DRM_MODE_FLAG_DBLSCAN)
		refresh /= 2;

	if (vscan > 1)
		refresh /= vscan;

	return refresh;
}

static void print_mode(struct json_object *obj)
{
	int hdisplay = get_object_object_uint64(obj, "hdisplay");
	int vdisplay = get_object_object_uint64(obj, "vdisplay");
	int type = get_object_object_uint64(obj, "type");
	int flags = get_object_object_uint64(obj, "flags");

	printf("%"PRIu16"x%"PRIu16"@%.02f ", hdisplay, vdisplay,
		refresh_rate(obj) / 1000.0);

	if (type & DRM_MODE_TYPE_PREFERRED)
		printf("preferred ");
	if (type & DRM_MODE_TYPE_USERDEF)
		printf("userdef ");
	if (type & DRM_MODE_TYPE_DRIVER)
		printf("driver ");

	if (flags & DRM_MODE_FLAG_PHSYNC)
		printf("phsync ");
	if (flags & DRM_MODE_FLAG_NHSYNC)
		printf("nhsync ");
	if (flags & DRM_MODE_FLAG_PVSYNC)
		printf("pvsync ");
	if (flags & DRM_MODE_FLAG_NVSYNC)
		printf("nvsync ");
	if (flags & DRM_MODE_FLAG_INTERLACE)
		printf("interlace ");
	if (flags & DRM_MODE_FLAG_DBLSCAN)
		printf("dblscan ");
	if (flags & DRM_MODE_FLAG_CSYNC)
		printf("csync ");
	if (flags & DRM_MODE_FLAG_PCSYNC)
		printf("pcsync ");
	if (flags & DRM_MODE_FLAG_NCSYNC)
		printf("nvsync ");
	if (flags & DRM_MODE_FLAG_HSKEW)
		printf("hskew ");
	if (flags & DRM_MODE_FLAG_DBLCLK)
		printf("dblclk ");
	if (flags & DRM_MODE_FLAG_CLKDIV2)
		printf("clkdiv2 ");

	switch (flags & DRM_MODE_FLAG_PIC_AR_MASK) {
	case DRM_MODE_FLAG_PIC_AR_NONE:
		break;
	case DRM_MODE_FLAG_PIC_AR_4_3:
		printf("4:3 ");
		break;
	case DRM_MODE_FLAG_PIC_AR_16_9:
		printf("16:9 ");
		break;
	case DRM_MODE_FLAG_PIC_AR_64_27:
		printf("64:27 ");
		break;
	case DRM_MODE_FLAG_PIC_AR_256_135:
		printf("256:135 ");
		break;
	}

	switch (flags & DRM_MODE_FLAG_3D_MASK) {
	case DRM_MODE_FLAG_3D_NONE:
		break;
	case DRM_MODE_FLAG_3D_FRAME_PACKING:
		printf("3d-frame-packing ");
		break;
	case DRM_MODE_FLAG_3D_FIELD_ALTERNATIVE:
		printf("3d-field-alternative ");
		break;
	case DRM_MODE_FLAG_3D_LINE_ALTERNATIVE:
		printf("3d-line-alternative ");
		break;
	case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_FULL:
		printf("3d-side-by-side-full ");
		break;
	case DRM_MODE_FLAG_3D_L_DEPTH:
		printf("3d-l-depth ");
		break;
	case DRM_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH:
		printf("3d-l-depth-gfx-gfx-depth ");
		break;
	case DRM_MODE_FLAG_3D_TOP_AND_BOTTOM:
		printf("3d-top-and-bottom ");
		break;
	case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF:
		printf("3d-side-by-side-half ");
		break;
	}
}

static void print_properties(struct json_object *obj, const char *prefix)
{
	// TODO
}

static void print_modes(struct json_object *arr, const char *prefix)
{
	if (json_object_array_length(arr) == 0) {
		return;
	}

	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		bool last = i == json_object_array_length(arr) - 1;
		printf("%s" L_LINE "%s", prefix, last ? L_LAST : L_VAL);
		print_mode(json_object_array_get_idx(arr, i));
		printf("\n");
	}
}

static const char *conn_name(uint32_t type)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_Unknown:     return "unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "S-VIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	case DRM_MODE_CONNECTOR_DPI:         return "DPI";
	case DRM_MODE_CONNECTOR_WRITEBACK:   return "writeback";
	default:                             return "unknown";
	}
}

static const char *conn_status(drmModeConnection conn)
{
	switch (conn) {
	case DRM_MODE_CONNECTED:         return "connected";
	case DRM_MODE_DISCONNECTED:      return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION: return "unknown";
	default:                         return "unknown";
	}
}

static const char *conn_subpixel(drmModeSubPixel subpixel)
{
	switch (subpixel) {
	case DRM_MODE_SUBPIXEL_UNKNOWN:        return "unknown";
	case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB: return "horizontal RGB";
	case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR: return "horizontal BGR";
	case DRM_MODE_SUBPIXEL_VERTICAL_RGB:   return "vertical RGB";
	case DRM_MODE_SUBPIXEL_VERTICAL_BGR:   return "vertical BGR";
	case DRM_MODE_SUBPIXEL_NONE:           return "none";
	default:                               return "unknown";
	}
}

static void print_connectors(struct json_object *arr)
{
	printf(L_VAL "Connectors\n");
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		struct json_object *obj = json_object_array_get_idx(arr, i);
		bool last = i == json_object_array_length(arr) - 1;

		uint32_t id = get_object_object_uint64(obj, "id");
		uint32_t type = get_object_object_uint64(obj, "type");
		drmModeConnection status = get_object_object_uint64(obj, "status");
		uint32_t phy_width = get_object_object_uint64(obj, "phy_width");
		uint32_t phy_height = get_object_object_uint64(obj, "phy_height");
		drmModeSubPixel subpixel = get_object_object_uint64(obj, "subpixel");
		struct json_object *encs_arr = json_object_object_get(obj, "encoders");
		struct json_object *modes_arr = json_object_object_get(obj, "modes");
		struct json_object *props_arr = json_object_object_get(obj, "props");

		printf(L_LINE "%sConnector %zu\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n",
			last ? L_GAP : L_LINE, id);
		printf(L_LINE "%s" L_VAL "Type: %s\n", last ? L_GAP : L_LINE,
			conn_name(type));
		printf(L_LINE "%s" L_VAL "Status: %s\n", last ? L_GAP : L_LINE,
			conn_status(status));
		if (status != DRM_MODE_DISCONNECTED) {
			printf(L_LINE "%s" L_VAL "Physical size: %"PRIu32"x%"PRIu32" mm\n",
				last ? L_GAP : L_LINE, phy_width, phy_height);
			printf(L_LINE "%s" L_VAL "Subpixel: %s\n", last ? L_GAP : L_LINE,
				conn_subpixel(subpixel));
		}

		bool first = true;
		printf(L_LINE "%s" L_VAL "Encoders: {", last ? L_GAP : L_LINE);
		for (size_t j = 0; j < json_object_array_length(encs_arr); ++j) {
			uint32_t enc_id = get_object_uint64(
				json_object_array_get_idx(encs_arr, j));
			printf("%s%"PRIu32, first ? "" : ", ", enc_id);
			first = false;
		}
		printf("}\n");

		print_modes(modes_arr, last ? L_LINE L_GAP : L_LINE L_LINE);
		print_properties(props_arr, last ? L_LINE L_GAP : L_LINE L_LINE);
	}
}

static void print_node(const char *path, struct json_object *obj)
{
	printf("Node: %s\n", path);
	print_driver(json_object_object_get(obj, "driver"));
	print_device(json_object_object_get(obj, "device"));
	print_connectors(json_object_object_get(obj, "connectors"));
}

void print_drm(struct json_object *obj)
{
	json_object_object_foreach(obj, path, node_obj) {
		print_node(path, node_obj);
	}
}
