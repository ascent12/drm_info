#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// Defines for comaptibility with old libdrm

// drm.h

#ifndef DRM_CAP_CRTC_IN_VBLANK_EVENT
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#endif

#ifndef DRM_CAP_SYNCOBJ
#define DRM_CAP_SYNCOBJ 0x13
#endif

#ifndef DRM_CLIENT_CAP_WRITEBACK_CONNECTORS
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5
#endif

#ifndef DRM_MODE_CONNECTOR_WRITEBACK
#define DRM_MODE_CONNECTOR_WRITEBACK 18
#endif

// drm_fourcc.h

#ifndef DRM_FORMAT_R16
#define DRM_FORMAT_R16 fourcc_code('R', '1', '6', ' ')
#endif

#ifndef DRM_FORMAT_RG1616
#define DRM_FORMAT_RG1616 fourcc_code('R', 'G', '3', '2')
#endif

#ifndef DRM_FORMAT_GR1616
#define DRM_FORMAT_GR1616 fourcc_code('G', 'R', '3', '2')
#endif

#define L_LINE "│   "
#define L_VAL  "├───"
#define L_LAST "└───"
#define L_GAP  "    "

static void print_client_cap(int fd, uint64_t name, const char *str)
{
	printf(L_LINE L_VAL);

	if (drmSetClientCap(fd, name, 1) == 0)
		printf("%s supported\n", str);
	else
		printf("%s not supported\n", str);
}

#define print_client_cap(fd, name) print_client_cap(fd, name, #name)

static void print_cap_bool(int fd, bool last, uint64_t name, const char *str)
{
	printf(last ? L_LINE L_LAST : L_LINE L_VAL);

	uint64_t cap;
	if (drmGetCap(fd, name, &cap) == 0)
		printf("%s = %s\n", str, cap ? "true" : "false");
	else
		printf("%s not supported\n", str);
}

#define print_cap_bool(fd, last, name) print_cap_bool(fd, last, name, #name)

static void print_cap_val(int fd, bool last, uint64_t name, const char *str)
{
	printf(last ? L_LINE L_LAST : L_LINE L_VAL);

	uint64_t cap;
	if (drmGetCap(fd, name, &cap) == 0)
		printf("%s = %"PRIu64"\n", str, cap);
	else
		printf("%s not supported\n", str);
}

#define print_cap_val(fd, last, name) print_cap_val(fd, last, name, #name)

static void driver_info(int fd)
{
	drmVersion *ver = drmGetVersion(fd);
	if (!ver) {
		printf(L_VAL "Driver: Failed to get version\n");
		return;
	}

	printf(L_VAL "Driver: %s %d.%d.%d %s\n", ver->name, ver->version_major,
		ver->version_minor, ver->version_patchlevel, ver->date);

	drmFreeVersion(ver);

	print_client_cap(fd, DRM_CLIENT_CAP_STEREO_3D);
	print_client_cap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES);
	print_client_cap(fd, DRM_CLIENT_CAP_ATOMIC);
	print_client_cap(fd, DRM_CLIENT_CAP_ASPECT_RATIO);
	print_client_cap(fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS);

	print_cap_bool(fd, false, DRM_CAP_DUMB_BUFFER);
	print_cap_bool(fd, false, DRM_CAP_VBLANK_HIGH_CRTC);
	print_cap_val(fd, false, DRM_CAP_DUMB_PREFERRED_DEPTH);
	print_cap_bool(fd, false, DRM_CAP_DUMB_PREFER_SHADOW);

	// Print PRIME support specially, because format is different
	uint64_t cap;
	if (drmGetCap(fd, DRM_CAP_PRIME, &cap) == 0) {
		printf(L_LINE L_VAL "DRM_CAP_PRIME supported\n");
		printf(L_LINE L_LINE L_VAL "DRM_PRIME_CAP_IMPORT = %s\n",
			cap & DRM_PRIME_CAP_IMPORT ? "true" : "false");
		printf(L_LINE L_LINE L_LAST "DRM_PRIME_CAP_EXPORT = %s\n",
			cap & DRM_PRIME_CAP_EXPORT ? "true" : "false");
	} else {
		printf(L_LINE L_VAL "DRM_CAP_PRIME not supported\n");
	}

	print_cap_bool(fd, false, DRM_CAP_TIMESTAMP_MONOTONIC);
	print_cap_bool(fd, false, DRM_CAP_ASYNC_PAGE_FLIP);
	print_cap_val(fd, false, DRM_CAP_CURSOR_WIDTH);
	print_cap_val(fd, false, DRM_CAP_CURSOR_HEIGHT);
	print_cap_bool(fd, false, DRM_CAP_ADDFB2_MODIFIERS);
	print_cap_bool(fd, false, DRM_CAP_PAGE_FLIP_TARGET);
	print_cap_bool(fd, false, DRM_CAP_CRTC_IN_VBLANK_EVENT);
	print_cap_bool(fd, true, DRM_CAP_SYNCOBJ);
}

static const char *obj_str(uint32_t type)
{
	switch (type) {
	case DRM_MODE_OBJECT_CRTC:      return "CRTC";
	case DRM_MODE_OBJECT_CONNECTOR: return "Connector";
	case DRM_MODE_OBJECT_ENCODER:   return "Encoder";
	case DRM_MODE_OBJECT_MODE:      return "Mode";
	case DRM_MODE_OBJECT_PROPERTY:  return "Property";
	case DRM_MODE_OBJECT_FB:        return "Framebuffer";
	case DRM_MODE_OBJECT_BLOB:      return "BLOB";
	case DRM_MODE_OBJECT_PLANE:     return "Plane";
	case DRM_MODE_OBJECT_ANY:       return "Any";
	default:                        return "Unknown";
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

static const char *format_str(uint32_t fmt)
{
	switch (fmt) {
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
	default:                     return "Unknown";
	}
}

static const char *modifier_str(uint64_t modifier)
{
	switch (modifier) {
	case DRM_FORMAT_MOD_INVALID: return "DRM_FORMAT_MOD_INVALID";
	case DRM_FORMAT_MOD_LINEAR: return "DRM_FORMAT_MOD_LINEAR";
	case I915_FORMAT_MOD_X_TILED: return "I915_FORMAT_MOD_X_TILED";
	case I915_FORMAT_MOD_Y_TILED: return "I915_FORMAT_MOD_Y_TILED";
	case I915_FORMAT_MOD_Yf_TILED: return "I915_FORMAT_MOD_Yf_TILED";
	case I915_FORMAT_MOD_Y_TILED_CCS: return "I915_FORMAT_MOD_Y_TILED_CCS";
	case I915_FORMAT_MOD_Yf_TILED_CCS: return "I915_FORMAT_MOD_Yf_TILED_CSS";
	case DRM_FORMAT_MOD_SAMSUNG_64_32_TILE: return "DRM_FORMAT_MOD_SAMSUNG_64_32_TILE";
	case DRM_FORMAT_MOD_VIVANTE_TILED: return "DRM_FORMAT_MOD_VIVANTE_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SUPER_TILED: return "DRM_FORMAT_MOD_VIVANTE_SUPER_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED: return "DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED";
	case DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED: return "DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED";
	case DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED: return "DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB";
	case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB: return "DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB";
	case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED: return "DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED";
	default: return "Unknown";
	}
}

static void print_in_formats(int fd, uint32_t id, const char *prefix)
{
	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, id);

	struct drm_format_modifier_blob *data = blob->data;

	uint32_t *fmts = (uint32_t *)
		((char *)data + data->formats_offset);

	struct drm_format_modifier *mods = (struct drm_format_modifier *)
		((char *)data + data->modifiers_offset);

	for (uint32_t i = 0; i < data->count_modifiers; ++i) {
		bool last = i == data->count_modifiers - 1;

		printf("%s%s%s\n",
				prefix,
				last ? L_LAST : L_VAL,
				modifier_str(mods[i].modifier));
		for (uint64_t j = 0; j < 64; ++j) {
			if (mods[i].formats & (1ull << j))
				printf("%s%s%s%s\n",
						prefix,
						last ? L_GAP : L_LINE,
						(mods[i].formats >> j) ^ 1 ? L_VAL : L_LAST,
						format_str(fmts[j + mods[i].offset]));
		}
	}

	drmModeFreePropertyBlob(blob);
}

// The refresh rate provided by the mode itself is innacurate,
// so we calculate it ourself.
static int32_t refresh_rate(const drmModeModeInfo *mode) {
	int32_t refresh = (mode->clock * 1000000LL / mode->htotal +
		mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		refresh *= 2;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		refresh /= 2;

	if (mode->vscan > 1)
		refresh /= mode->vscan;

	return refresh;
}

static void print_mode(const drmModeModeInfo *mode)
{
	printf("%"PRIu16"x%"PRIu16"@%.02f ", mode->hdisplay, mode->vdisplay,
		refresh_rate(mode) / 1000.0);

	if (mode->type & DRM_MODE_TYPE_BUILTIN)
		printf("builtin ");
	if (mode->type & DRM_MODE_TYPE_CLOCK_C)
		printf("clock_c ");
	if (mode->type & DRM_MODE_TYPE_CRTC_C)
		printf("crtc_c ");
	if (mode->type & DRM_MODE_TYPE_PREFERRED)
		printf("preferred ");
	if (mode->type & DRM_MODE_TYPE_DEFAULT)
		printf("default ");
	if (mode->type & DRM_MODE_TYPE_USERDEF)
		printf("userdef ");
	if (mode->type & DRM_MODE_TYPE_DRIVER)
		printf("driver ");

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		printf("phsync ");
	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		printf("nhsync ");
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		printf("pvsync ");
	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		printf("nvsync ");
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		printf("interlace ");
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		printf("dblscan ");
	if (mode->flags & DRM_MODE_FLAG_CSYNC)
		printf("csync ");
	if (mode->flags & DRM_MODE_FLAG_PCSYNC)
		printf("pcsync ");
	if (mode->flags & DRM_MODE_FLAG_NCSYNC)
		printf("nvsync ");
	if (mode->flags & DRM_MODE_FLAG_HSKEW)
		printf("hskew ");
	if (mode->flags & DRM_MODE_FLAG_BCAST)
		printf("bcast ");
	if (mode->flags & DRM_MODE_FLAG_PIXMUX)
		printf("pixmux ");
	if (mode->flags & DRM_MODE_FLAG_DBLCLK)
		printf("dblclk ");
	if (mode->flags & DRM_MODE_FLAG_CLKDIV2)
		printf("clkdiv2 ");
}

static void print_mode_id(int fd, uint32_t id, const char *prefix)
{
	if (id == 0) {
		return;
	}

	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, id);

	drmModeModeInfo *mode = blob->data;

	printf("%s%s", prefix, L_LAST);
	print_mode(mode);
	printf("\n");

	drmModeFreePropertyBlob(blob);
}

static void print_writeback_pixel_formats(int fd, uint32_t id, const char *prefix)
{
	drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(fd, id);

	uint32_t *fmts = blob->data;
	uint32_t fmts_len = blob->length / sizeof(uint32_t);
	for (uint32_t i = 0; i < fmts_len; ++i) {
		bool last = i == fmts_len - 1;
		printf("%s%s%s\n", prefix, last ? L_LAST : L_VAL, format_str(fmts[i]));
	}

	drmModeFreePropertyBlob(blob);
}

static void properties(int fd, uint32_t id, uint32_t type, const char *prefix)
{
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, id, type);
	if (!props) {
		perror("drmModeObjectGetProperties");
		return;
	}

	printf("%s" L_LAST "Properties\n", prefix);

	for (uint32_t i = 0; i < props->count_props; ++i) {
		bool last = i == props->count_props - 1;

		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		if (!prop) {
			perror("drmModeGetProperty");
			continue;
		}

		uint32_t flags = prop->flags;
		bool atomic = flags & DRM_MODE_PROP_ATOMIC;
		bool immut = flags & DRM_MODE_PROP_IMMUTABLE;

		printf("%s" L_GAP "%s\"%s\"", prefix, last ? L_LAST : L_VAL, prop->name);
		if (atomic && immut)
			printf(" (Atomic|Immutable)");
		else if (atomic)
			printf(" (Atomic)");
		else if (immut)
			printf(" (Immutable)");

		printf(": ");

		char sub_prefix[strlen(prefix) + 2 * strlen(L_VAL) + 1];
		snprintf(sub_prefix, sizeof(sub_prefix), "%s" L_GAP "%s",
			prefix, last ? L_GAP : L_LINE);

		switch (flags & DRM_MODE_PROP_LEGACY_TYPE) {
		case DRM_MODE_PROP_RANGE:;
			const char *low = u64_str(prop->values[0]);
			const char *high = u64_str(prop->values[1]);

			if (low)
				printf("Range [%s, ", low);
			else
				printf("Range [%"PRIu64", ", prop->values[0]);

			if (high)
				printf("%s]", high);
			else
				printf("%"PRIu64"]", prop->values[1]);

			// This is a special case, as the SRC_* properties are
			// in 16.16 fixed point
			if (strncmp(prop->name, "SRC_", 4) == 0)
				printf(" - %"PRIu64"\n", props->prop_values[i] >> 16);
			else
				printf(" - %"PRIu64"\n", props->prop_values[i]);

			break;
		case DRM_MODE_PROP_ENUM:
			printf("Enum {%s", prop->enums[0].name);
			for (int j = 1; j < prop->count_enums; ++j)
				printf(", %s", prop->enums[j].name);

			int j;
			for (j = 0; j < prop->count_enums; ++j)
				if (prop->enums[j].value == props->prop_values[i])
					break;

			if (j == prop->count_enums)
				printf("} - Invalid (%"PRIu64")\n", props->prop_values[i]);
			else
				printf("} - %s\n", prop->enums[j].name);

			break;
		case DRM_MODE_PROP_BLOB:
			printf("Blob\n");
			if (strcmp(prop->name, "IN_FORMATS") == 0)
				print_in_formats(fd, props->prop_values[i], sub_prefix);
			else if (strcmp(prop->name, "MODE_ID") == 0)
				print_mode_id(fd, props->prop_values[i], sub_prefix);
			else if (strcmp(prop->name, "WRITEBACK_PIXEL_FORMATS") == 0)
				print_writeback_pixel_formats(fd, props->prop_values[i], sub_prefix);
			break;
		case DRM_MODE_PROP_BITMASK:
			printf("Bitmask {%s", prop->enums[0].name);
			for (int j = 1; j < prop->count_enums; ++j)
				printf(", %s", prop->enums[j].name);
			printf("} - (");

			bool first = true;
			for (int j = 0; j < prop->count_enums; ++j) {
				uint64_t val = prop->enums[j].value;
				if ((props->prop_values[i] & val) != val)
					continue;

				if (first)
					first = false;
				else
					printf(" | ");
				printf("%s", prop->enums[j].name);
			}

			printf(")\n");
			break;
		default:
			break;
		}

		switch (flags & DRM_MODE_PROP_EXTENDED_TYPE) {
		case DRM_MODE_PROP_OBJECT:
			printf("Object %s - %"PRIu64"\n", obj_str(prop->values[0]), props->prop_values[i]);
			break;
		case DRM_MODE_PROP_SIGNED_RANGE:;
			const char *low = i64_str(prop->values[0]);
			const char *high = i64_str(prop->values[1]);

			if (low)
				printf("SRange [%s, ", low);
			else
				printf("SRange [%"PRId64", ", prop->values[0]);

			if (high)
				printf("%s]", high);
			else
				printf("%"PRId64"]", prop->values[1]);

			printf(" - %"PRId64"\n", (int64_t)props->prop_values[i]);

			break;
		default:
			break;
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
}

static void mode_info(const drmModeModeInfo *modes, size_t n, const char *prefix)
{
	if (n == 0)
		return;

	printf("%s" L_VAL "Modes\n", prefix);
	for (size_t i = 0; i < n; ++i) {
		const drmModeModeInfo *mode = &modes[i];
		bool last = i == n - 1;

		printf("%s" L_LINE "%s", prefix, last ? L_LAST : L_VAL);
		print_mode(mode);
		printf("\n");
	}
}

static const char *conn_name(uint32_t type)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	case DRM_MODE_CONNECTOR_DPI:         return "DPI";
	case DRM_MODE_CONNECTOR_WRITEBACK:   return "Writeback";
	default:                             return "Unknown";
	}
}

static const char *conn_connection(drmModeConnection conn)
{
	switch (conn) {
	case DRM_MODE_CONNECTED:         return "Connected";
	case DRM_MODE_DISCONNECTED:      return "Disconnected";
	case DRM_MODE_UNKNOWNCONNECTION: return "Unknown";
	default:                         return "Unknown";
	}
}

static const char *conn_subpixel(drmModeSubPixel subpixel)
{
	switch (subpixel) {
	case DRM_MODE_SUBPIXEL_UNKNOWN:        return "Unknown";
	case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB: return "Horizontal RGB";
	case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR: return "Horizontal BGR";
	case DRM_MODE_SUBPIXEL_VERTICAL_RGB:   return "Vertical RGB";
	case DRM_MODE_SUBPIXEL_VERTICAL_BGR:   return "Vertical BGR";
	case DRM_MODE_SUBPIXEL_NONE:           return "None";
	default:                               return "Unknown";
	}
}

static void connector_info(int fd, drmModeRes *res)
{
	printf(L_VAL "Connectors\n");
	for (int i = 0; i < res->count_connectors; ++i) {
		bool last = i == res->count_connectors - 1;

		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) {
			perror("drmModeGetConnector");
			continue;
		}

		printf(L_LINE "%sConnector %d\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n", last ? L_GAP : L_LINE, conn->connector_id);
		printf(L_LINE "%s" L_VAL "Type: %s\n", last ? L_GAP : L_LINE, conn_name(conn->connector_type));
		printf(L_LINE "%s" L_VAL "Status: %s\n", last ? L_GAP : L_LINE, conn_connection(conn->connection));
		printf(L_LINE "%s" L_VAL "Physical size: %"PRIu32"x%"PRIu32" mm\n", last ? L_GAP : L_LINE, conn->mmWidth, conn->mmHeight);
		printf(L_LINE "%s" L_VAL "Subpixel: %s\n", last ? L_GAP : L_LINE, conn_subpixel(conn->subpixel));

		bool first = true;
		printf(L_LINE "%s" L_VAL "Encoders: {", last ? L_GAP : L_LINE);
		for (int j = 0; j < conn->count_encoders; ++j) {
			for (int k = 0; k < res->count_encoders; ++k) {
				if (conn->encoders[j] != res->encoders[k])
					continue;

				printf("%s%d", first ? "" : ", ", k);
				first = false;
			}
		}
		printf("}\n");

		mode_info(conn->modes, conn->count_modes, last ? L_LINE L_GAP : L_LINE L_LINE);
		properties(fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR,
			last ? L_LINE L_GAP : L_LINE L_LINE);

		drmModeFreeConnector(conn);
	}
}

static const char *encoder_str(uint32_t type)
{
	switch (type) {
	case DRM_MODE_ENCODER_NONE:    return "None";
	case DRM_MODE_ENCODER_DAC:     return "DAC";
	case DRM_MODE_ENCODER_TMDS:    return "TMDS";
	case DRM_MODE_ENCODER_LVDS:    return "LVDS";
	case DRM_MODE_ENCODER_TVDAC:   return "TV DAC";
	case DRM_MODE_ENCODER_VIRTUAL: return "Virtual";
	case DRM_MODE_ENCODER_DSI:     return "DSI";
	case DRM_MODE_ENCODER_DPMST:   return "DP MST";
	case DRM_MODE_ENCODER_DPI:     return "DPI";
	default:                       return "Unknown";
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

static void encoder_info(int fd, drmModeRes *res)
{
	printf(L_VAL "Encoders\n");
	for (int i = 0; i < res->count_encoders; ++i) {
		bool last = i == res->count_encoders - 1;
		drmModeEncoder *enc = drmModeGetEncoder(fd, res->encoders[i]);
		if (!enc) {
			perror("drmModeGetEncoder");
			continue;
		}

		printf(L_LINE "%sEncoder %d\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n", last ? L_GAP : L_LINE, enc->encoder_id);
		printf(L_LINE "%s" L_VAL "Type: %s\n", last ? L_GAP : L_LINE, encoder_str(enc->encoder_type));

		printf(L_LINE "%s" L_VAL "CRTCS: ", last ? L_GAP : L_LINE);
		print_bitmask(enc->possible_crtcs);
		printf("\n");

		printf(L_LINE "%s" L_LAST "Clones: ", last ? L_GAP : L_LINE);
		print_bitmask(enc->possible_clones);
		printf("\n");

		drmModeFreeEncoder(enc);
	}
}

static void crtc_info(int fd, drmModeRes *res)
{
	printf(L_VAL "CRTCs\n");
	for (int i = 0; i < res->count_crtcs; ++i) {
		bool last = i == res->count_crtcs - 1;

		drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		if (!crtc) {
			perror("drmModeGetCrtc");
			continue;
		}

		printf(L_LINE "%sCRTC %d\n", last ? L_LAST : L_VAL, i);

		printf(L_LINE "%s" L_VAL "Object ID: %"PRIu32"\n", last ? L_GAP : L_LINE, crtc->crtc_id);

		properties(fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
			last ? L_LINE L_GAP : L_LINE L_LINE);

		drmModeFreeCrtc(crtc);
	}
}

static void plane_info(int fd)
{
	printf(L_LAST "Planes\n");

	drmModePlaneRes *res = drmModeGetPlaneResources(fd);
	if (!res) {
		perror("drmModeGetPlaneResources");
		return;
	}

	for (uint32_t i = 0; i < res->count_planes; ++i) {
		bool last = i == res->count_planes - 1;

		drmModePlane *plane = drmModeGetPlane(fd, res->planes[i]);
		if (!plane) {
			perror("drmModeGetPlane");
			continue;
		}

		printf(L_GAP "%sPlane %"PRIu32"\n", last ? L_LAST : L_VAL, i);

		printf(L_GAP "%s" L_VAL "Object ID: %"PRIu32"\n", last ? L_GAP : L_LINE, plane->plane_id);
		printf(L_GAP "%s" L_VAL "CRTCS: ", last ? L_GAP : L_LINE);
		print_bitmask(plane->possible_crtcs);
		printf("\n");

		printf(L_GAP "%s" L_VAL "Formats:\n", last ? L_GAP : L_LINE);
		for (uint32_t j = 0; j < plane->count_formats; ++j) {
			bool fmt_last = j == plane->count_formats - 1;

			printf(L_GAP "%s" L_LINE "%s%s\n", last ? L_GAP : L_LINE,
				fmt_last ? L_LAST : L_VAL,
				format_str(plane->formats[j]));
		}

		properties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE,
			last ? L_GAP L_GAP : L_GAP L_LINE);

		drmModeFreePlane(plane);
	}

	drmModeFreePlaneResources(res);
}

static void drm_info(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return;
	}

	// Print driver info before getting resources, as it'll try to enable some
	// DRM client capabilities
	printf("Device: %s\n", path);
	driver_info(fd);

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) {
		perror("drmModeGetResources");
		return;
	}

	connector_info(fd, res);
	encoder_info(fd, res);
	crtc_info(fd, res);
	plane_info(fd);

	drmModeFreeResources(res);

	close(fd);
}

int main(void)
{
	char path[PATH_MAX];
	for (int i = 0;; ++i) {
		snprintf(path, sizeof path, DRM_DEV_NAME, DRM_DIR_NAME, i);
		if (access(path, R_OK) < 0)
			break;

		drm_info(path);
	}
}
