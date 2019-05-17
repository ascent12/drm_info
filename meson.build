project('drm_info', 'c',
  version: '1.2.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

conf = configuration_data()
libdrm = dependency('libdrm', version: '>= 2.4.74')
jsonc = dependency('json-c', version: '>=0.13', fallback: ['json-c', 'json_c'])

libdrm_ver = libdrm.version().split('.')
conf.set('LIBDRM_MAJ', libdrm_ver[0])
conf.set('LIBDRM_MIN', libdrm_ver[1])
conf.set('LIBDRM_PAT', libdrm_ver[2])

config_h = configure_file(
  configuration: conf,
  output: 'config.h',
)

executable('drm_info',
  files('main.c', 'json.c', 'pretty.c'),
  dependencies: [libdrm, jsonc],
  install: true,
)
