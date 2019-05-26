#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <json.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "compat.h"
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

static const char *format_str(uint32_t fmt)
{
	switch (fmt) {
	case DRM_FORMAT_INVALID:     return "invalid";
	case DRM_FORMAT_C8:          return "C8";
	case DRM_FORMAT_R8:          return "R8";
	case DRM_FORMAT_R16:         return "R16";
	case DRM_FORMAT_RG88:        return "RG88";
	case DRM_FORMAT_GR88:        return "GR88";
	case DRM_FORMAT_RG1616:      return "RG1616";
	case DRM_FORMAT_GR1616:      return "GR1616";
	case DRM_FORMAT_RGB332:      return "RGB332";
	case DRM_FORMAT_BGR233:      return "BGR233";
	case DRM_FORMAT_XRGB4444:    return "XRGB4444";
	case DRM_FORMAT_XBGR4444:    return "XBGR4444";
	case DRM_FORMAT_RGBX4444:    return "RGBX4444";
	case DRM_FORMAT_BGRX4444:    return "BGRX4444";
	case DRM_FORMAT_ARGB4444:    return "ARGB4444";
	case DRM_FORMAT_ABGR4444:    return "ABGR4444";
	case DRM_FORMAT_RGBA4444:    return "RGBA4444";
	case DRM_FORMAT_BGRA4444:    return "BGRA4444";
	case DRM_FORMAT_XRGB1555:    return "XRGB1555";
	case DRM_FORMAT_XBGR1555:    return "XBGR1555";
	case DRM_FORMAT_RGBX5551:    return "RGBX5551";
	case DRM_FORMAT_BGRX5551:    return "BGRX5551";
	case DRM_FORMAT_ARGB1555:    return "ARGB1555";
	case DRM_FORMAT_ABGR1555:    return "ABGR1555";
	case DRM_FORMAT_RGBA5551:    return "RGBA5551";
	case DRM_FORMAT_BGRA5551:    return "BGRA5551";
	case DRM_FORMAT_RGB565:      return "RGB565";
	case DRM_FORMAT_BGR565:      return "BGR565";
	case DRM_FORMAT_RGB888:      return "RGB888";
	case DRM_FORMAT_BGR888:      return "BGR888";
	case DRM_FORMAT_XRGB8888:    return "XRGB8888";
	case DRM_FORMAT_XBGR8888:    return "XBGR8888";
	case DRM_FORMAT_RGBX8888:    return "RGBX8888";
	case DRM_FORMAT_BGRX8888:    return "BGRX8888";
	case DRM_FORMAT_ARGB8888:    return "ARGB8888";
	case DRM_FORMAT_ABGR8888:    return "ABGR8888";
	case DRM_FORMAT_RGBA8888:    return "RGBA8888";
	case DRM_FORMAT_BGRA8888:    return "BGRA8888";
	case DRM_FORMAT_XRGB2101010: return "XRGB2101010";
	case DRM_FORMAT_XBGR2101010: return "XBGR2101010";
	case DRM_FORMAT_RGBX1010102: return "RGBX1010102";
	case DRM_FORMAT_BGRX1010102: return "BGRX1010102";
	case DRM_FORMAT_ARGB2101010: return "ARGB2101010";
	case DRM_FORMAT_ABGR2101010: return "ABGR2101010";
	case DRM_FORMAT_RGBA1010102: return "RGBA1010102";
	case DRM_FORMAT_BGRA1010102: return "BGRA1010102";
	case DRM_FORMAT_YUYV:        return "YUYV";
	case DRM_FORMAT_YVYU:        return "YVYU";
	case DRM_FORMAT_UYVY:        return "UYVY";
	case DRM_FORMAT_VYUY:        return "VYUY";
	case DRM_FORMAT_AYUV:        return "AYUV";
	case DRM_FORMAT_NV12:        return "NV12";
	case DRM_FORMAT_NV21:        return "NV21";
	case DRM_FORMAT_NV16:        return "NV16";
	case DRM_FORMAT_NV61:        return "NV61";
	case DRM_FORMAT_NV24:        return "NV24";
	case DRM_FORMAT_NV42:        return "NV42";
	case DRM_FORMAT_YUV410:      return "YUV410";
	case DRM_FORMAT_YVU410:      return "YVU410";
	case DRM_FORMAT_YUV411:      return "YUV411";
	case DRM_FORMAT_YVU411:      return "YVU411";
	case DRM_FORMAT_YUV420:      return "YUV420";
	case DRM_FORMAT_YVU420:      return "YVU420";
	case DRM_FORMAT_YUV422:      return "YUV422";
	case DRM_FORMAT_YVU422:      return "YVU422";
	case DRM_FORMAT_YUV444:      return "YUV444";
	case DRM_FORMAT_YVU444:      return "YVU444";
	default:                     return "unknown";
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

static const char *modifier_str(uint64_t modifier)
{
	/*
	 * ARM has a complex format which we can't be bothered to parse.
	 */
	if ((modifier >> 56) == DRM_FORMAT_MOD_VENDOR_ARM) {
		return "DRM_FORMAT_MOD_ARM_AFBC()";
	}

	switch (modifier) {
	case DRM_FORMAT_MOD_INVALID: return "DRM_FORMAT_MOD_INVALID";
	case DRM_FORMAT_MOD_LINEAR: return "DRM_FORMAT_MOD_LINEAR";
	case I915_FORMAT_MOD_X_TILED: return "I915_FORMAT_MOD_X_TILED";
	case I915_FORMAT_MOD_Y_TILED: return "I915_FORMAT_MOD_Y_TILED";
	case I915_FORMAT_MOD_Yf_TILED: return "I915_FORMAT_MOD_Yf_TILED";
	case I915_FORMAT_MOD_Y_TILED_CCS: return "I915_FORMAT_MOD_Y_TILED_CCS";
	case I915_FORMAT_MOD_Yf_TILED_CCS: return "I915_FORMAT_MOD_Yf_TILED_CSS";
	case DRM_FORMAT_MOD_SAMSUNG_64_32_TILE: return "DRM_FORMAT_MOD_SAMSUNG_64_32_TILE";
	// The following formats were added in 2.4.82, but IN_FORMATS wasn't added until 2.4.83
	case DRM_FORMAT_MOD_VIVANTE_TILED: return "DRM_FORMAT_MOD_VIVANTE_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SUPER_TILED: return "DRM_FORMAT_MOD_VIVANTE_SUPER_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED: return "DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED: return "DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED";
	case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED: return "DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED";
	case DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED: return "DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB";
	case DRM_FORMAT_MOD_BROADCOM_SAND32: return "DRM_FORMAT_MOD_BROADCOM_SAND32";
	case DRM_FORMAT_MOD_BROADCOM_SAND64: return "DRM_FORMAT_MOD_BROADCOM_SAND64";
	case DRM_FORMAT_MOD_BROADCOM_SAND128: return "DRM_FORMAT_MOD_BROADCOM_SAND128";
	case DRM_FORMAT_MOD_BROADCOM_SAND256: return "DRM_FORMAT_MOD_BROADCOM_SAND265";
	case DRM_FORMAT_MOD_BROADCOM_UIF: return "DRM_FORMAT_MOD_BROADCOM_UIF";
	case DRM_FORMAT_MOD_ALLWINNER_TILED: return "DRM_FORMAT_MOD_ALLWINNER_TILED";
	default: return "unknown";
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
