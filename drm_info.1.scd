drm_info(1)

# NAME

drm_info - dump information about DRM devices

# SYNOPSIS

*drm_info* [-j] [device]...

# DESCRIPTION

*drm_info* is a small utility to dump information about DRM devices.

_device_ must be a path to a DRM device, e.g. "/dev/dri/card0". When omitted,
all available devices are printed.

# OPTIONS

*-j*
	Print information in JSON format. By default, the output will be
	pretty-printed in a human-readable format.

# AUTHORS

Maintained by Scott Anderson <scott@anderso.nz>. For more information about
drm_info development, see <https://github.com/ascent12/drm_info>.
