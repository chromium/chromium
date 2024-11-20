project(
    'quote',
    'rust',
    default_options: ['rust_std=2018'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '1.0.37',
)

lib = static_library(
    'quote',
    dependencies: [dependency('proc-macro2', version: ['>= 1.0.80', '< 2'])],
    native: true,
    rust_args: ['--cfg=feature="default"', '--cfg=feature="proc-macro"'],
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('quote', dep)
