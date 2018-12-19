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

# These specific versions added more modifiers, so we check for these instead
# of using #ifdef on every single one
foreach version: ['2.4.83', '2.4.91', '2.4.95']
  conf.set10('HAVE_LIBDRM_' + version.underscorify(),
    libdrm.version().version_compare('>=' + version)
  )
endforeach

config_h = configure_file(
  configuration: conf,
  output: 'config.h',
)

executable('drm_info', 'drm_info.c', config_h,
  dependencies: libdrm,
  install: true,
)
