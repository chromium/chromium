project(
    'syn',
    'rust',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '2.0.85',
)

lib = static_library(
    'syn',
    dependencies: [
        dependency('proc-macro2', version: ['>= 1.0.83', '< 2']),
        dependency('quote', version: ['>= 1.0.35', '< 2']),
        dependency('unicode-ident', version: ['>= 1', '< 2']),
    ],
    native: true,
    rust_args: [
        '--cfg=feature="clone-impls"',
        '--cfg=feature="default"',
        '--cfg=feature="derive"',
        '--cfg=feature="full"',
        '--cfg=feature="parsing"',
        '--cfg=feature="printing"',
        '--cfg=feature="proc-macro"',
    ],
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('syn', dep)
