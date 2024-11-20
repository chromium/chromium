project(
    'anstyle',
    'rust',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '1.0.9',
)

lib = static_library(
    'anstyle',
    native: true,
    rust_args: ['--cfg=feature="default"', '--cfg=feature="std"'],
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('anstyle', dep)
