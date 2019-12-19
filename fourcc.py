#!/usr/bin/env python3

import sys
import re
from itertools import chain, combinations

def powerset(s):
	return chain.from_iterable(combinations(s, r) for r in range(len(s) + 1))

def case_print(f, c, s):
	f.write('\tcase {}:\n'.format(c))
	f.write('\t\treturn "{}";\n'.format(s))

def afbc_print(f, l):
	fmt = 'DRM_FORMAT_MOD_ARM_AFBC({})'
	if l:
		# Strip prefix to stop string being stupidly long
		short = [s[len('AFBC_FORMAT_MOD_'):] for s in l]
		case_print(f, fmt.format('|'.join(l)), fmt.format('|'.join(short)))
	else:
		case_print(f, fmt.format(0), fmt.format(0))

info = {
	'fmt': r'^#define (\w+)\s*(?:\\$\s*)?fourcc_code',
	'basic_pre': r'\bI915_FORMAT_MOD_\w+\b',
	'basic_post': r'\b(DRM_FORMAT_MOD_(?:INVALID|LINEAR|SAMSUNG|QCOM|VIVANTE|NVIDIA|BROADCOM|ALLWINNER)\w*)\s',
	'afbc_block': r'\bAFBC_FORMAT_MOD_BLOCK_SIZE(?:_\d+x\d+)+\b',
	'afbc_bitmask': r'\bAFBC_FORMAT_MOD_[A-Z]+\b',
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

const char *modifier_str(uint64_t modifier)
{
	switch (modifier) {
''')

	for ident in info['basic_pre'] + info['basic_post']:
		case_print(f, ident, ident)

	# ARM framebuffer compression
	# Not all of the combintations we generate will be valid modifers exposed
	# by the driver

	for bits in powerset(info['afbc_bitmask']):
		bits = list(bits)
		bits.sort()
		afbc_print(f, bits)
		for block in info['afbc_block']:
			afbc_print(f, bits + [block])

	f.write('''\
	default:
		return "Unknown";
	}
}
''')
