# drm_info

[![builds.sr.ht status](https://builds.sr.ht/~ascent/drm_info/commits.svg)](https://builds.sr.ht/~ascent/drm_info/commits?)

Small utility to dump info about DRM devices.

Requires libdrm and json-c.

## Building

Build with
```
meson build
cd build
ninja
sudo ninja install
```

If you don't have the minimum json-c version (0.13.0), meson will automatically
download and compile it for you. If you don't want this, run the first meson
command with
```
meson build --wrap-mode nofallback
```

## Usage

```
drm_info [-j] [--] [path]...
```
- `-j` - Output info in JSON. Otherwise the output is pretty-printed.
- `path` - Zero or more paths to a DRM device to print info about, e.g.
`/dev/dri/card0`. If no paths are given, all devices found in
`/dev/dri/card*` are printed.

## DRM database

[drmdb](https://drmdb.emersion.fr) is a database of Direct Rendering Manager
dumps. This database is used to keep track of GPUs and DRM driver features
support.

Please help us gather more data! You can do so by uploading DRM information
from your GPU.

```
drm_info -j | curl -X POST -d @- https://drmdb.emersion.fr/submit
```

This will upload information about your GPUs, your GPU drivers and your
screens.
