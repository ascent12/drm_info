#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <drm_fourcc.h>

#include "tables.h"

static void print_nvidia_modifier(uint64_t mod) {
	if (!(mod & 0x10)) {
		printf("NVIDIA(unknown)");
		return;
	}

	uint64_t h = mod & 0xF;
	uint64_t k = (mod >> 12) & 0xFF;
	uint64_t g = (mod >> 20) & 0x3;
	uint64_t s = (mod >> 22) & 0x1;
	uint64_t c = (mod >> 23) & 0x7;

	printf("NVIDIA_BLOCK_LINEAR_2D(h=%"PRIu64", k=%"PRIu64", g=%"PRIu64", "
		"s=%"PRIu64", c=%"PRIu64")", h, k, g, s, c);
}

static const char *amd_tile_version_str(uint64_t tile_version) {
	switch (tile_version) {
	case AMD_FMT_MOD_TILE_VER_GFX9:
		return "GFX9";
	case AMD_FMT_MOD_TILE_VER_GFX10:
		return "GFX10";
	case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
		return "GFX10_RBPLUS";
	}
	return "Unknown";
}

static const char *amd_tile_str(uint64_t tile, uint64_t tile_version) {
	switch (tile_version) {
	case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
		/* fallthrough */
	case AMD_FMT_MOD_TILE_VER_GFX10:
		/* fallthrough */
	case AMD_FMT_MOD_TILE_VER_GFX9:
		switch (tile) {
		case AMD_FMT_MOD_TILE_GFX9_64K_S:
			return "GFX9_64K_S";
		case AMD_FMT_MOD_TILE_GFX9_64K_D:
			return "GFX9_64K_D";
		case AMD_FMT_MOD_TILE_GFX9_64K_S_X:
			return "GFX9_64K_S_X";
		case AMD_FMT_MOD_TILE_GFX9_64K_D_X:
			return "GFX9_64K_D_X";
		case AMD_FMT_MOD_TILE_GFX9_64K_R_X:
			return "GFX9_64K_R_X";
		}
	}
	return "Unknown";
}

static const char *amd_dcc_block_size_str(uint64_t size) {
	switch (size) {
	case AMD_FMT_MOD_DCC_BLOCK_64B:
		return "64B";
	case AMD_FMT_MOD_DCC_BLOCK_128B:
		return "128B";
	case AMD_FMT_MOD_DCC_BLOCK_256B:
		return "256B";
	}
	return "Unknown";
}

static bool amd_gfx9_tile_is_x_t(uint64_t tile) {
	switch (tile) {
	case AMD_FMT_MOD_TILE_GFX9_64K_S_X:
	case AMD_FMT_MOD_TILE_GFX9_64K_D_X:
	case AMD_FMT_MOD_TILE_GFX9_64K_R_X:
		return true;
	}
	return false;
}

static void print_amd_modifier(uint64_t mod) {
	uint64_t tile_version = AMD_FMT_MOD_GET(TILE_VERSION, mod);
	uint64_t tile = AMD_FMT_MOD_GET(TILE, mod);
	uint64_t dcc = AMD_FMT_MOD_GET(DCC, mod);
	uint64_t dcc_retile = AMD_FMT_MOD_GET(DCC_RETILE, mod);

	printf("AMD(TILE_VERSION = %s, TILE = %s",
		amd_tile_version_str(tile_version), amd_tile_str(tile, tile_version));

	if (dcc) {
		printf(", DCC");
		if (dcc_retile) {
			printf(", DCC_RETILE");
		}
		if (!dcc_retile && AMD_FMT_MOD_GET(DCC_PIPE_ALIGN, mod)) {
			printf(", DCC_PIPE_ALIGN");
		}
		if (AMD_FMT_MOD_GET(DCC_INDEPENDENT_64B, mod)) {
			printf(", DCC_INDEPENDENT_64B");
		}
		if (AMD_FMT_MOD_GET(DCC_INDEPENDENT_128B, mod)) {
			printf(", DCC_INDEPENDENT_128B");
		}
		uint64_t dcc_max_compressed_block =
			AMD_FMT_MOD_GET(DCC_MAX_COMPRESSED_BLOCK, mod);
		printf(", DCC_MAX_COMPRESSED_BLOCK = %s",
			amd_dcc_block_size_str(dcc_max_compressed_block));
		if (AMD_FMT_MOD_GET(DCC_CONSTANT_ENCODE, mod)) {
			printf(", DCC_CONSTANT_ENCODE");
		}
	}

	if (tile_version >= AMD_FMT_MOD_TILE_VER_GFX9 && amd_gfx9_tile_is_x_t(tile)) {
		printf(", PIPE_XOR_BITS = %"PRIu64, AMD_FMT_MOD_GET(PIPE_XOR_BITS, mod));
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9) {
			printf(", BANK_XOR_BITS = %"PRIu64, AMD_FMT_MOD_GET(BANK_XOR_BITS, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS) {
			printf(", PACKERS = %"PRIu64, AMD_FMT_MOD_GET(PACKERS, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9 && dcc) {
			printf(", RB = %"PRIu64, AMD_FMT_MOD_GET(RB, mod));
		}
		if (tile_version == AMD_FMT_MOD_TILE_VER_GFX9 && dcc &&
				(dcc_retile || AMD_FMT_MOD_GET(DCC_PIPE_ALIGN, mod))) {
			printf(", PIPE = %"PRIu64, AMD_FMT_MOD_GET(PIPE, mod));
		}
	}

	printf(")");
}

static const char *amlogic_layout_str(uint64_t layout) {
	switch (layout) {
	case AMLOGIC_FBC_LAYOUT_BASIC:
		return "BASIC";
	case AMLOGIC_FBC_LAYOUT_SCATTER:
		return "SCATTER";
	}
	return "Unknown";
}

static void print_amlogic_modifier(uint64_t mod) {
	uint64_t layout = mod & 0xFF;
	uint64_t options = (mod >> 8) & 0xFF;

	printf("AMLOGIC_FBC(layout = %s, options = %s)",
		amlogic_layout_str(layout),
		(options & AMLOGIC_FBC_OPTION_MEM_SAVING) ? "MEM_SAVING" : "0");
}

static uint8_t mod_vendor(uint64_t mod) {
	return (uint8_t)(mod >> 56);
}

void print_modifier(uint64_t mod) {
	switch (mod_vendor(mod)) {
	case DRM_FORMAT_MOD_VENDOR_NVIDIA:
		print_nvidia_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_AMD:
		print_amd_modifier(mod);
		break;
	case DRM_FORMAT_MOD_VENDOR_AMLOGIC:
		print_amlogic_modifier(mod);
		break;
	default:
		printf("%s", basic_modifier_str(mod));
	}
	printf(" (0x%"PRIx64")", mod);
}
