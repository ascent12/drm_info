#!/usr/bin/env python3

import sys
import re

def case_print(f, c, s):
	f.write('\tcase {}:\n'.format(c))
	f.write('\t\treturn "{}";\n'.format(s))

info = {
	'fmt': r'^#define (\w+)\s*(?:\\$\s*)?fourcc_code',
	'basic_pre': r'^#define (I915_FORMAT_MOD_\w+)\b',
	'basic_post': r'^#define (DRM_FORMAT_MOD_(?:INVALID|LINEAR|SAMSUNG|QCOM|VIVANTE|NVIDIA|BROADCOM|ALLWINNER)\w*)\s',
}

with open(sys.argv[1], 'r') as f:
	data = f.read()
	for k, v in info.items():
		info[k] = re.findall(v, data, flags=re.M)

with open(sys.argv[2], 'w') as f:
	f.write('''\
#include <stdint.h>
#include <drm_fourcc.h>

#include "tables.h"

const char *format_str(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_INVALID:
		return "INVALID";
''')

	for ident in info['fmt']:
		case_print(f, ident, ident[len('DRM_FORMAT_'):])

	f.write('''\
	default:
		return "Unknown";
	}
}

const char *basic_modifier_str(uint64_t modifier)
{
	switch (modifier) {
''')

	for ident in info['basic_pre'] + info['basic_post']:
		case_print(f, ident, ident)

	f.write('''\
	default:
		return "Unknown";
	}
}
''')
