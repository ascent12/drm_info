#ifndef COMPAT_H
#define COMPAT_H

#include <drm_fourcc.h>
#include "config.h"

/*
 * Defines for compatibility with old libdrm.
 * The oldest version we target is whatever version Debian stable has.
 */

#define VERSION(maj, min, pat) (((maj) << 16) | ((min) << 8) | (pat))
#define LIBDRM_VERSION VERSION(LIBDRM_MAJ, LIBDRM_MIN, LIBDRM_PAT)

#if LIBDRM_VERSION < VERSION(2, 4, 75)
#define DRM_BUS_PCI 0
#define DRM_BUS_USB 1
#define DRM_BUS_PLATFORM 2
#define DRM_BUS_HOST1X 3
#endif /* LIBDRM_VERSION < VERSION(2, 4, 75) */

#if LIBDRM_VERSION < VERSION(2, 4, 78)
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#endif /* LIBDRM_VERSION < VERSION(2, 4, 78) */

#if LIBDRM_VERSION < VERSION(2, 4, 82)
#define DRM_CAP_SYNCOBJ 0x13
#define DRM_FORMAT_R16 fourcc_code('R', '1', '6', ' ')
#define DRM_FORMAT_RG1616 fourcc_code('R', 'G', '3', '2')
#define DRM_FORMAT_GR1616 fourcc_code('G', 'R', '3', '2')
#define DRM_FORMAT_MOD_VENDOR_VIVANTE 0x06
#define DRM_FORMAT_MOD_VENDOR_BROADCOM 0x07
#define DRM_FORMAT_MOD_VENDOR_NONE    0
#define DRM_FORMAT_MOD_LINEAR	fourcc_mod_code(NONE, 0)
#define DRM_FORMAT_MOD_VIVANTE_TILED		fourcc_mod_code(VIVANTE, 1)
#define DRM_FORMAT_MOD_VIVANTE_SUPER_TILED	fourcc_mod_code(VIVANTE, 2)
#define DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED	fourcc_mod_code(VIVANTE, 3)
#define DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED fourcc_mod_code(VIVANTE, 4)
#define DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED fourcc_mod_code(BROADCOM, 1)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 82) */

#if LIBDRM_VERSION < VERSION(2, 4, 83)
#define DRM_FORMAT_RESERVED	      ((1ULL << 56) - 1)
#define DRM_FORMAT_MOD_INVALID	fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)
#define I915_FORMAT_MOD_Y_TILED_CCS	fourcc_mod_code(INTEL, 4)
#define I915_FORMAT_MOD_Yf_TILED_CCS	fourcc_mod_code(INTEL, 5)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 83) */

#if LIBDRM_VERSION < VERSION(2, 4, 86)
#define DRM_MODE_PICTURE_ASPECT_NONE		0
#define DRM_MODE_PICTURE_ASPECT_4_3		1
#define DRM_MODE_PICTURE_ASPECT_16_9		2
#define DRM_MODE_FLAG_PIC_AR_MASK		(0x0F<<19)
#define  DRM_MODE_FLAG_PIC_AR_NONE (DRM_MODE_PICTURE_ASPECT_NONE<<19)
#define  DRM_MODE_FLAG_PIC_AR_4_3 (DRM_MODE_PICTURE_ASPECT_4_3<<19)
#define  DRM_MODE_FLAG_PIC_AR_16_9 (DRM_MODE_PICTURE_ASPECT_16_9<<19)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 86) */

#if LIBDRM_VERSION < VERSION(2, 4, 91)
#define DRM_FORMAT_MOD_VENDOR_NVIDIA  0x03
#define DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED fourcc_mod_code(NVIDIA, 1)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(v) \
	fourcc_mod_code(NVIDIA, 0x10 | ((v) & 0xf))
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB \
	fourcc_mod_code(NVIDIA, 0x10)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB \
	fourcc_mod_code(NVIDIA, 0x11)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB \
	fourcc_mod_code(NVIDIA, 0x12)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB \
	fourcc_mod_code(NVIDIA, 0x13)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB \
	fourcc_mod_code(NVIDIA, 0x14)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB \
	fourcc_mod_code(NVIDIA, 0x15)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 91) */

#if LIBDRM_VERSION < VERSION(2, 4, 95)
#define DRM_CLIENT_CAP_ASPECT_RATIO 4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5
#define DRM_MODE_PICTURE_ASPECT_64_27		3
#define DRM_MODE_PICTURE_ASPECT_256_135		4
#define  DRM_MODE_FLAG_PIC_AR_64_27 (DRM_MODE_PICTURE_ASPECT_64_27<<19)
#define  DRM_MODE_FLAG_PIC_AR_256_135 (DRM_MODE_PICTURE_ASPECT_256_135<<19)
#define DRM_FORMAT_INVALID 0
#define DRM_MODE_CONNECTOR_WRITEBACK 18
#define DRM_FORMAT_MOD_VENDOR_ARM     0x08
#define __fourcc_mod_broadcom_param_shift 8
#define __fourcc_mod_broadcom_param_bits 48
#define fourcc_mod_broadcom_code(val, params) \
	fourcc_mod_code(BROADCOM, ((((__u64)params) << __fourcc_mod_broadcom_param_shift) | val))
#define fourcc_mod_broadcom_param(m) \
	((int)(((m) >> __fourcc_mod_broadcom_param_shift) &	\
	       ((1ULL << __fourcc_mod_broadcom_param_bits) - 1)))
#define fourcc_mod_broadcom_mod(m) \
	((m) & ~(((1ULL << __fourcc_mod_broadcom_param_bits) - 1) <<	\
		 __fourcc_mod_broadcom_param_shift))
#define DRM_FORMAT_MOD_BROADCOM_SAND32_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(2, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND64_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(3, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(4, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND256_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(5, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND32 \
	DRM_FORMAT_MOD_BROADCOM_SAND32_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND64 \
	DRM_FORMAT_MOD_BROADCOM_SAND64_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND128 \
	DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND256 \
	DRM_FORMAT_MOD_BROADCOM_SAND256_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_UIF fourcc_mod_code(BROADCOM, 6)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 95) */

#if LIBDRM_VERSION < VERSION(2, 4, 98)
#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)
#endif /* LIBDRM_VERSION < VERSION(2, 4, 98) */

#if LIBDRM_VERSION < VERSION(2, 4, 99)
#define DRM_CAP_SYNCOBJ_TIMELINE	0x14
#endif /* LIBDRM_VERSION < VERSION(2, 4, 99) */

#endif
