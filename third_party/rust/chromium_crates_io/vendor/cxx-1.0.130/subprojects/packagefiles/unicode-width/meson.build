project(
    'unicode-width',
    'rust',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '0.1.14',
)

lib = static_library(
    'unicode_width',
    native: true,
    rust_args: ['--cfg=feature="cjk"', '--cfg=feature="default"'],
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('unicode-width', dep)
