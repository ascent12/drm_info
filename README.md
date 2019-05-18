# drm_info

[![builds.sr.ht status](https://builds.sr.ht/~ascent/drm_info.svg)](https://builds.sr.ht/~ascent/drm_info?)

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
