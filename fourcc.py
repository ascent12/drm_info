#!/usr/bin/env python3

import sys
import re

fourcc_include = sys.argv[1]

fmt_list = []
mod_list = []

fmt = re.compile(r'^#define (\w+)\s*(?:\\$\s*)?fourcc_code', flags=re.M)
mod = re.compile(r'^#define (\w+)\s*(?:\\$\s*)?fourcc_mod_code', flags=re.M)
mod_broadcom = re.compile(r'^#define (DRM_FORMAT_MOD_BROADCOM\w+(?=\s))', flags=re.M)

f = open(fourcc_include).read()
fmt_list = fmt.findall(f)
mod_list = set(mod.findall(f) + mod_broadcom.findall(f))

f_name = sys.argv[2]

with open(f_name, 'w') as f:
	f.write('''\
#include <stdint.h>
#include <drm_fourcc.h>

static inline const char *format_str(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_INVALID:
		return "INVALID";
''')
	for ident in fmt_list:
		f.write('\tcase {}:\n\t\treturn "{}";\n'.format(ident, ident[len('DRM_FORMAT_'):]))
	f.write('''\
	default:
		return "unknown";
	}
}

static inline const char *modifier_str(uint64_t modifier)
{
	/*
	 * ARM has a complex format which we can't be bothered to parse.
	 */
	if ((modifier >> 56) == DRM_FORMAT_MOD_VENDOR_ARM) {
		return "DRM_FORMAT_MOD_ARM_AFBC()";
	}

	switch (modifier) {
''')
	for ident in mod_list:
		f.write('\tcase {}:\n\t\treturn "{}";\n'.format(ident, ident))
	f.write('''\
	default:
		return "unknown";
	}
}
''')
