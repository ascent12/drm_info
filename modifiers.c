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

static uint8_t mod_vendor(uint64_t mod) {
	return (uint8_t)(mod >> 56);
}

void print_modifier(uint64_t mod) {
	switch (mod_vendor(mod)) {
	case DRM_FORMAT_MOD_VENDOR_NVIDIA:
		print_nvidia_modifier(mod);
		break;
	default:
		printf("%s", basic_modifier_str(mod));
	}
	printf(" (0x%"PRIx64")", mod);
}
