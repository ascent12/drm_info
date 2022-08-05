project('drm_info', 'c',
  version: '2.3.0',
  license: 'MIT',
  meson_version: '>=0.49.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

jsonc = dependency('json-c', version: '>=0.13', fallback: ['json-c', 'json_c'])
libpci = dependency('libpci', required: get_option('libpci'))
libdrm = dependency('libdrm',
  fallback: ['libdrm', 'ext_libdrm'],
  default_options: [
    'libkms=false',
    'intel=false',
    'radeon=false',
    'amdgpu=false',
    'nouveau=false',
    'vmwgfx=false',
    'omap=false',
    'exynos=false',
    'freedreno=false',
    'tegra=false',
    'vc4=false',
    'etnaviv=false',
    'cairo-tests=false',
    'man-pages=false',
    'valgrind=false',
  ],
)

inc = []
# libdrm pretty consistently pulls in the linux userspace API headers.
# We want a new libdrm to get all of the #defines in those headers, but
# we don't actually need to link against a new version of libdrm itself.
#
# We need to make sure we don't use any new libdrm functions, but those
# are added very infrequently, so this is unlikely to be an issue.
if libdrm.version().version_compare('<2.4.104')
  if libdrm.type_name() == 'internal'
    error('libdrm subproject out of date. Run `meson subprojects update`.')
  endif

  msg = [
    'System libdrm version does not have latest Linux DRM headers.',
    'Attempting to use headers from meson subproject if present.',
    'If this fails, update your system libdrm or run `meson subprojects download`.',
  ]
  foreach s : msg
    warning(s)
  endforeach

  fourcc_h = files('subprojects/libdrm/include/drm/drm_fourcc.h')
  inc += include_directories('subprojects/libdrm/include/drm')
  libdrm = libdrm.partial_dependency(link_args: true)
elif libdrm.type_name() == 'internal'
  fourcc_h = files('subprojects/libdrm/include/drm/drm_fourcc.h')
else
  fourcc_h = files(libdrm.get_pkgconfig_variable('includedir') / 'libdrm/drm_fourcc.h')
endif

if libpci.found()
  add_project_arguments('-DHAVE_LIBPCI', language: 'c')
endif

if libdrm.type_name() == 'internal' or cc.has_function('drmModeGetFB2', dependencies: [libdrm])
  add_project_arguments('-DHAVE_GETFB2', language: 'c')
endif

python3 = import('python').find_installation()

tables_c = custom_target('tables_c',
  output : 'tables.c',
  command : [python3, files('fourcc.py'), fourcc_h, '@OUTPUT@'])

executable('drm_info',
  [files('main.c', 'modifiers.c', 'json.c', 'pretty.c'), tables_c],
  include_directories: inc,
  dependencies: [libdrm, libpci, jsonc],
  install: true,
)

scdoc = dependency('scdoc', native: true, required: get_option('man-pages'))
if scdoc.found()
  man_pages = ['drm_info.1.scd']
  sh = find_program('sh', native: true)

  foreach src : man_pages
    topic = src.split('.')[0]
    section = src.split('.')[1]
    output = topic + '.' + section

    custom_target(
      output,
      input: files(src),
      output: output,
      command: [
        sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.get_pkgconfig_variable('scdoc'), output)
      ],
      install: true,
      install_dir: get_option('mandir') / 'man' + section
    )
  endforeach
endif
