project('drm_info', 'c',
  version: '1.0.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

executable('drm_info', 'drm_info.c',
  dependencies: dependency('libdrm', version: '>= 2.4.74'),
  install: true,
)
