#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <json.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef HAVE_LIBPCI
#include <pci/pci.h>
#endif

#include "drm_info.h"
#include "tables.h"

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

static const char *node_type_str(int type)
{
	switch (type) {
	case DRM_NODE_PRIMARY:
		return "primary";
	case DRM_NODE_CONTROL:
		return "control";
	case DRM_NODE_RENDER:
		return "render";
	default:
		return "unknown";
	}
}

static void print_available_nodes(int available_nodes)
{
	bool first = true;
	for (int i = 0; i < DRM_NODE_MAX; i++) {
		if (!(available_nodes & (1 << i)))
			continue;
		printf("%s%s", first ? "" : ", ", node_type_str(i));
		first = false;
	}
}

static void print_device(struct json_object *obj)
{
	if (!obj)
		return;

	int available_nodes = get_object_object_uint64(obj, "available_nodes");
	int bus_type = get_object_object_uint64(obj, "bus_type");
	struct json_object *data_obj = json_object_object_get(obj, "device_data");

	printf(L_VAL "Device: %s", bustype_str(bus_type));
	switch (bus_type) {
	case DRM_BUS_PCI:;
		uint16_t pci_vendor = get_object_object_uint64(data_obj, "vendor");
		uint16_t pci_device = get_object_object_uint64(data_obj, "device");
		printf(" %04x:%04x", pci_vendor, pci_device);
#ifdef HAVE_LIBPCI
		struct pci_access *pci = pci_alloc();
		pci_init(pci);
		char name[512];
		if (pci_lookup_name(pci, name, sizeof(name),
				PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
				pci_vendor, pci_device)) {
			printf(" %s", name);
		}
		pci_cleanup(pci);
#endif
		break;
	case DRM_BUS_USB:;
		uint16_t usb_vendor = get_object_object_uint64(data_obj, "vendor");
		uint16_t usb_product = get_object_object_uint64(data_obj, "product");
		printf(" %04x:%04x", usb_vendor, usb_product);
		break;
	case DRM_BUS_PLATFORM:;
		struct json_object *compatible_arr =
			json_object_object_get(data_obj, "compatible");
		for (size_t i = 0; i < json_object_array_length(compatible_arr); i++) {
			printf(" %s", json_object_get_string(
				json_object_array_get_idx(compatible_arr, i)));
		}
		break;
	}
	printf("\n");

	printf(L_LINE L_LAST "Available nodes: ");
	print_available_nodes(available_nodes);
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

/* Replace well-known constants with strings */
static const char *u64_str(uint64_t val)
{
	switch (val) {
	case INT8_MAX:   return "INT8_MAX";
	case UINT8_MAX:  return "UINT8_MAX";
	case INT16_MAX:  return "INT16_MAX";
	case UINT16_MAX: return "UINT16_MAX";
	case INT32_MAX:  return "INT32_MAX";
	case UINT32_MAX: return "UINT32_MAX";
	case INT64_MAX:  return "INT64_MAX";
	case UINT64_MAX: return "UINT64_MAX";
	default:         return NULL;
	}
}

/* Replace well-known constants with strings */
static const char *i64_str(int64_t val)
{
	switch (val) {
	case INT8_MIN:   return "INT8_MIN";
	case INT16_MIN:  return "INT16_MIN";
	case INT32_MIN:  return "INT32_MIN";
	case INT64_MIN:  return "INT64_MIN";
	case INT8_MAX:   return "INT8_MAX";
	case UINT8_MAX:  return "UINT8_MAX";
	case INT16_MAX:  return "INT16_MAX";
	case UINT16_MAX: return "UINT16_MAX";
	case INT32_MAX:  return "INT32_MAX";
	case UINT32_MAX: return "UINT32_MAX";
	case INT64_MAX:  return "INT64_MAX";
	default:         return NULL;
	}
}

static const char *obj_str(uint32_t type)
{
	switch (type) {
	case DRM_MODE_OBJECT_CRTC:      return "CRTC";
	case DRM_MODE_OBJECT_CONNECTOR: return "connector";
	case DRM_MODE_OBJECT_ENCODER:   return "encoder";
	case DRM_MODE_OBJECT_MODE:      return "mode";
	case DRM_MODE_OBJECT_PROPERTY:  return "property";
	case DRM_MODE_OBJECT_FB:        return "framebuffer";
	case DRM_MODE_OBJECT_BLOB:      return "blob";
	case DRM_MODE_OBJECT_PLANE:     return "plane";
	case DRM_MODE_OBJECT_ANY:       return "any";
	default:                        return "unknown";
	}
}

static void print_in_formats(struct json_object *arr, const char *prefix)
{
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		bool last = i == json_object_array_length(arr) - 1;
		struct json_object *mod_obj = json_object_array_get_idx(arr, i);
		uint64_t mod = get_object_object_uint64(mod_obj, "modifier");
		struct json_object *formats_arr =
			json_object_object_get(mod_obj, "formats");

		printf("%s%s%s (0x%"PRIx64")\n", prefix, last ? L_LAST : L_VAL,
			modifier_str(mod), mod);
		for (size_t j = 0; j < json_object_array_length(formats_arr); ++j) {
			bool fmt_last = j == json_object_array_length(formats_arr) - 1;
			uint32_t fmt = get_object_uint64(
				json_object_array_get_idx(formats_arr, j));
			printf("%s%s%s%s (0x%08"PRIx32")\n", prefix, last ? L_GAP : L_LINE,
				fmt_last ? L_LAST : L_VAL, format_str(fmt), fmt);
		}
	}
}

static void print_mode_id(struct json_object *obj, const char *prefix)
{
	printf("%s" L_LAST, prefix);
	print_mode(obj);
	printf("\n");
}

static void print_writeback_pixel_formats(struct json_object *arr,
		const char *prefix)
{
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		bool last = i == json_object_array_length(arr) - 1;
		uint32_t fmt = get_object_uint64(json_object_array_get_idx(arr, i));
		printf("%s%s%s (0x%08"PRIx32")\n", prefix, last ? L_LAST : L_VAL,
			format_str(fmt), fmt);
	}
}

static void print_path(struct json_object *obj, const char *prefix)
{
	printf("%s" L_LAST "%s\n", prefix, json_object_get_string(obj));
}

static void print_fb(struct json_object *obj, const char *prefix)
{
	uint32_t id = get_object_object_uint64(obj, "id");
	uint32_t width = get_object_object_uint64(obj, "width");
	uint32_t height = get_object_object_uint64(obj, "height");

	struct json_object *pitch_obj = json_object_object_get(obj, "pitch");
	struct json_object *bpp_obj = json_object_object_get(obj, "bpp");
	struct json_object *depth_obj = json_object_object_get(obj, "depth");
	struct json_object *format_obj = json_object_object_get(obj, "format");
	struct json_object *modifier_obj = json_object_object_get(obj, "modifier");
	struct json_object *planes_arr = json_object_object_get(obj, "planes");
	bool has_legacy = pitch_obj && bpp_obj && depth_obj;

	printf("%s" L_VAL "Object ID: %"PRIu32"\n", prefix, id);
	printf("%s%sSize: %"PRIu32"x%"PRIu32"\n", prefix,
		(has_legacy || format_obj) ? L_VAL : L_LAST,
		width, height);

	if (has_legacy) {
		printf("%s" L_VAL "Pitch: %"PRIu32"\n", prefix,
			(uint32_t)get_object_uint64(pitch_obj));
		printf("%s" L_VAL "Bits per pixel: %"PRIu32"\n", prefix,
			(uint32_t)get_object_uint64(bpp_obj));
		printf("%s%sDepth: %"PRIu32"\n", prefix, format_obj ? L_VAL : L_LAST,
			(uint32_t)get_object_uint64(depth_obj));
	}
	if (format_obj) {
		bool last = !modifier_obj && !planes_arr;
		uint32_t fmt = (uint32_t)get_object_uint64(format_obj);
		printf("%s%sFormat: %s (0x%08"PRIx32")\n", prefix,
			last ? L_LAST : L_VAL, format_str(fmt), fmt);
	}
	if (modifier_obj) {
		uint64_t mod = get_object_uint64(modifier_obj);
		printf("%s%sModifier: %s (0x%"PRIx64")\n", prefix,
			planes_arr ? L_VAL : L_LAST, modifier_str(mod), mod);
	}
	if (planes_arr) {
		printf("%s" L_LAST "Planes:\n", prefix);
		for (size_t i = 0; i < json_object_array_length(planes_arr); ++i) {
			bool last = i == json_object_array_length(planes_arr) - 1;
			struct json_object *plane_obj =
				json_object_array_get_idx(planes_arr, i);
			uint64_t offset = get_object_object_uint64(plane_obj, "offset");
			uint64_t pitch = get_object_object_uint64(plane_obj, "pitch");
			printf("%s" L_GAP "%sPlane %zu: "
				"offset = %"PRIu64", pitch = %"PRIu64"\n",
				prefix, last ? L_LAST : L_LINE, i, offset, pitch);
		}
	}
}

static void print_properties(struct json_object *obj, const char *prefix)
{
	printf("%s" L_LAST "Properties\n", prefix);

	struct json_object_iter iter;
	json_object_object_foreachC(obj, iter) {
		bool last = !iter.entry->next;
		const char *prop_name = iter.key;
		struct json_object *prop_obj = iter.val;

		char sub_prefix[strlen(prefix) + 2 * strlen(L_VAL) + 1];
		snprintf(sub_prefix, sizeof(sub_prefix), "%s" L_GAP "%s",
			prefix, last ? L_GAP : L_LINE);

		uint32_t flags = get_object_object_uint64(prop_obj, "flags");
		uint32_t type = flags & (DRM_MODE_PROP_LEGACY_TYPE |
			DRM_MODE_PROP_EXTENDED_TYPE);
		bool atomic = flags & DRM_MODE_PROP_ATOMIC;
		bool immutable = flags & DRM_MODE_PROP_IMMUTABLE;

		printf("%s" L_GAP "%s\"%s\"", prefix, last ? L_LAST : L_VAL, prop_name);
		if (atomic && immutable)
			printf(" (atomic, immutable)");
		else if (atomic)
			printf(" (atomic)");
		else if (immutable)
			printf(" (immutable)");

		printf(": ");

		uint64_t raw_val = get_object_object_uint64(prop_obj, "raw_value");
		struct json_object *spec_obj = json_object_object_get(prop_obj, "spec");
		struct json_object *val_obj = json_object_object_get(prop_obj, "value");
		struct json_object *data_obj = json_object_object_get(prop_obj, "data");
		bool first;
		switch (type) {
		case DRM_MODE_PROP_RANGE:;
			uint64_t min = get_object_object_uint64(spec_obj, "min");
			uint64_t max = get_object_object_uint64(spec_obj, "max");
			const char *min_str = u64_str(min);
			const char *max_str = u64_str(max);

			if (min_str)
				printf("range [%s, ", min_str);
			else
				printf("range [%"PRIu64", ", min);

			if (max_str)
				printf("%s]", max_str);
			else
				printf("%"PRIu64"]", max);

			if (data_obj != NULL)
				printf(" = %"PRIu64"\n", get_object_uint64(data_obj));
			else
				printf(" = %"PRIu64"\n", get_object_uint64(val_obj));
			break;
		case DRM_MODE_PROP_ENUM:
			printf("enum {");
			const char *val_name = NULL;
			first = true;
			for (size_t j = 0; j < json_object_array_length(spec_obj); ++j) {
				struct json_object *item_obj =
					json_object_array_get_idx(spec_obj, j);
				const char *item_name =
					get_object_object_string(item_obj, "name");
				uint64_t item_value =
					get_object_object_uint64(item_obj, "value");

				if (raw_val == item_value) {
					val_name = item_name;
				}

				printf("%s%s", first ? "" : ", ", item_name);
				first = false;
			}
			printf("} = ");

			if (val_name) {
				printf("%s\n", val_name);
			} else {
				printf("invalid (%"PRIu64")\n", raw_val);
			}
			break;
		case DRM_MODE_PROP_BLOB:;
			printf("blob = %" PRIu64 "\n", raw_val);
			if (!data_obj)
				break;
			if (strcmp(prop_name, "IN_FORMATS") == 0)
				print_in_formats(data_obj, sub_prefix);
			else if (strcmp(prop_name, "MODE_ID") == 0)
				print_mode_id(data_obj, sub_prefix);
			else if (strcmp(prop_name, "WRITEBACK_PIXEL_FORMATS") == 0)
				print_writeback_pixel_formats(data_obj, sub_prefix);
			else if (strcmp(prop_name, "PATH") == 0)
				print_path(data_obj, sub_prefix);
			break;
		case DRM_MODE_PROP_BITMASK:
			printf("bitmask {");
			first = true;
			for (size_t j = 0; j < json_object_array_length(spec_obj); ++j) {
				struct json_object *item_obj =
					json_object_array_get_idx(spec_obj, j);
				const char *item_name =
					get_object_object_string(item_obj, "name");

				printf("%s%s", first ? "" : ", ", item_name);
				first = false;
			}
			printf("} = (");

			first = true;
			for (size_t j = 0; j < json_object_array_length(spec_obj); ++j) {
				struct json_object *item_obj =
					json_object_array_get_idx(spec_obj, j);
				const char *item_name =
					get_object_object_string(item_obj, "name");
				uint64_t item_value =
					get_object_object_uint64(item_obj, "value");
				if ((item_value & raw_val) != item_value) {
					continue;
				}

				printf("%s%s", first ? "" : " | ", item_name);
				first = false;
			}

			printf(")\n");
			break;
		case DRM_MODE_PROP_OBJECT:;
			uint32_t obj_type = get_object_uint64(spec_obj);
			printf("object %s = %"PRIu64"\n", obj_str(obj_type), raw_val);
			if (!data_obj)
				break;
			if (strcmp(prop_name, "FB_ID") == 0)
				print_fb(data_obj, sub_prefix);
			break;
		case DRM_MODE_PROP_SIGNED_RANGE:;
			int64_t smin =
				json_object_get_int64(json_object_object_get(spec_obj, "min"));
			int64_t smax =
				json_object_get_int64(json_object_object_get(spec_obj, "max"));
			const char *smin_str = i64_str(smin);
			const char *smax_str = i64_str(smax);

			if (smin_str)
				printf("srange [%s, ", smin_str);
			else
				printf("srange [%"PRIi64", ", smin);

			if (smax_str)
				printf("%s]", smax_str);
			else
				printf("%"PRIi64"]", smax);

			if (data_obj != NULL)
				printf(" = %"PRIi64"\n", json_object_get_int64(data_obj));
			else
				printf(" = %"PRIi64"\n", json_object_get_int64(val_obj));
			break;
		default:
			printf("unknown type (%"PRIu32") = %"PRIu64"\n", type, raw_val);
		}
	}
}

static void print_modes(struct json_object *arr, const char *prefix)
{
	if (json_object_array_length(arr) == 0) {
		return;
	}

	printf("%s" L_VAL "Modes\n", prefix);
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
	case DRM_MODE_CONNECTOR_DisplayPort: return "DisplayPort";
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

static ssize_t find_encoder_index(struct json_object *encoders_arr,
		uint32_t enc_id)
{
	for (size_t i = 0; i < json_object_array_length(encoders_arr); ++i) {
		struct json_object *obj = json_object_array_get_idx(encoders_arr, i);
		uint32_t id = get_object_object_uint64(obj, "id");
		if (enc_id == id) {
			return i;
		}
	}
	return -1;
}

static void print_connectors(struct json_object *arr,
		struct json_object *encoders_arr)
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
		struct json_object *props_obj = json_object_object_get(obj, "properties");

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
			printf("%s%zi", first ? "" : ", ",
				find_encoder_index(encoders_arr, enc_id));
			first = false;
		}
		printf("}\n");

		print_modes(modes_arr, last ? L_LINE L_GAP : L_LINE L_LINE);
		print_properties(props_obj, last ? L_LINE L_GAP : L_LINE L_LINE);
	}
}

static const char *encoder_str(uint32_t type)
{
	switch (type) {
	case DRM_MODE_ENCODER_NONE:    return "none";
	case DRM_MODE_ENCODER_DAC:     return "DAC";
	case DRM_MODE_ENCODER_TMDS:    return "TMDS";
	case DRM_MODE_ENCODER_LVDS:    return "LVDS";
	case DRM_MODE_ENCODER_TVDAC:   return "TV DAC";
	case DRM_MODE_ENCODER_VIRTUAL: return "virtual";
	case DRM_MODE_ENCODER_DSI:     return "DSI";
	case DRM_MODE_ENCODER_DPMST:   return "DP MST";
	case DRM_MODE_ENCODER_DPI:     return "DPI";
	default:                       return "unknown";
	}
}

static void print_bitmask(uint32_t mask)
{
	bool first = true;
	printf("{");
	for (uint32_t i = 0; i < 31; ++i) {
		if (!(mask & (1 << i)))
			continue;

		printf("%s%"PRIu32, first ? "" : ", ", i);
		first = false;
	}
	printf("}");
}

static void print_encoders(struct json_object *arr)
{
	printf(L_VAL "Encoders\n");
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		struct json_object *obj = json_object_array_get_idx(arr, i);
		bool last = i == json_object_array_length(arr) - 1;

		uint32_t id = get_object_object_uint64(obj, "id");
		uint32_t type = get_object_object_uint64(obj, "type");
		uint32_t crtcs = get_object_object_uint64(obj, "possible_crtcs");
		uint32_t clones = get_object_object_uint64(obj, "possible_clones");

		printf(L_LINE "%sEncoder %zu\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n",
			last ? L_GAP : L_LINE, id);
		printf(L_LINE "%s" L_VAL "Type: %s\n", last ? L_GAP : L_LINE,
			encoder_str(type));

		printf(L_LINE "%s" L_VAL "CRTCS: ", last ? L_GAP : L_LINE);
		print_bitmask(crtcs);
		printf("\n");

		printf(L_LINE "%s" L_LAST "Clones: ", last ? L_GAP : L_LINE);
		print_bitmask(clones);
		printf("\n");
	}
}

static void print_crtcs(struct json_object *arr)
{
	printf(L_VAL "CRTCs\n");
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		struct json_object *obj = json_object_array_get_idx(arr, i);
		bool last = i == json_object_array_length(arr) - 1;

		uint32_t id = get_object_object_uint64(obj, "id");
		struct json_object *props_obj = json_object_object_get(obj, "properties");

		printf(L_LINE "%sCRTC %zu\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n",
			last ? L_GAP : L_LINE, id);

		struct json_object *mode_obj = json_object_object_get(obj, "mode");
		if (mode_obj) {
			printf(L_LINE "%s" L_VAL "Mode: ", last ? L_GAP : L_LINE);
			print_mode(mode_obj);
			printf("\n");
		}

		struct json_object *gamma_size_obj =
			json_object_object_get(obj, "gamma_size");
		printf(L_LINE "%s" L_VAL "Gamma size: %d\n",
			last ? L_GAP : L_LINE, json_object_get_int(gamma_size_obj));

		print_properties(props_obj, last ? L_LINE L_GAP : L_LINE L_LINE);
	}
}

static void print_planes(struct json_object *arr)
{
	printf(L_LAST "Planes\n");
	for (size_t i = 0; i < json_object_array_length(arr); ++i) {
		struct json_object *obj = json_object_array_get_idx(arr, i);
		bool last = i == json_object_array_length(arr) - 1;

		uint32_t id = get_object_object_uint64(obj, "id");
		uint32_t crtcs = get_object_object_uint64(obj, "possible_crtcs");
		struct json_object *formats_arr = json_object_object_get(obj, "formats");
		struct json_object *props_obj = json_object_object_get(obj, "properties");

		printf(L_GAP "%sPlane %zu\n", last ? L_LAST : L_VAL, i);

		printf(L_GAP "%s" L_VAL "Object ID: %"PRIu32"\n",
			last ? L_GAP : L_LINE, id);
		printf(L_GAP "%s" L_VAL "CRTCs: ", last ? L_GAP : L_LINE);
		print_bitmask(crtcs);
		printf("\n");

		printf(L_GAP "%s" L_VAL "Formats:\n", last ? L_GAP : L_LINE);
		for (size_t j = 0; j < json_object_array_length(formats_arr); ++j) {
			uint32_t fmt = get_object_uint64(
				json_object_array_get_idx(formats_arr, j));
			bool fmt_last = j == json_object_array_length(formats_arr) - 1;

			printf(L_GAP "%s" L_LINE "%s%s (0x%08"PRIx32")\n", last ? L_GAP : L_LINE,
				fmt_last ? L_LAST : L_VAL,
				format_str(fmt), fmt);
		}

		print_properties(props_obj, last ? L_GAP L_GAP : L_GAP L_LINE);
	}
}

static void print_node(const char *path, struct json_object *obj)
{
	printf("Node: %s\n", path);
	print_driver(json_object_object_get(obj, "driver"));
	print_device(json_object_object_get(obj, "device"));

	printf(L_VAL "Framebuffer size\n");
	struct json_object *fb_size_obj = json_object_object_get(obj, "fb_size");
	printf(L_LINE L_VAL "Width: [%"PRIu64", %"PRIu64"]\n",
		get_object_object_uint64(fb_size_obj, "min_width"),
		get_object_object_uint64(fb_size_obj, "max_width"));
	printf(L_LINE L_LAST "Height: [%"PRIu64", %"PRIu64"]\n",
		get_object_object_uint64(fb_size_obj, "min_height"),
		get_object_object_uint64(fb_size_obj, "max_height"));

	struct json_object *encs_arr = json_object_object_get(obj, "encoders");
	print_connectors(json_object_object_get(obj, "connectors"), encs_arr);
	print_encoders(encs_arr);
	print_crtcs(json_object_object_get(obj, "crtcs"));
	print_planes(json_object_object_get(obj, "planes"));
}

void print_drm(struct json_object *obj)
{
	json_object_object_foreach(obj, path, node_obj) {
		print_node(path, node_obj);
	}
}
