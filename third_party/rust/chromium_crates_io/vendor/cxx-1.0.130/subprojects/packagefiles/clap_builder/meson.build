project(
    'clap_builder',
    'rust',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '4.5.20',
)

lib = static_library(
    'clap_builder',
    dependencies: [
        dependency('anstyle', version: ['>= 1.0.8', '< 2']),
        dependency('clap_lex', version: ['>= 0.7.0', '< 0.8']),
    ],
    native: true,
    rust_args: [
        '--cfg=feature="error-context"',
        '--cfg=feature="help"',
        '--cfg=feature="std"',
        '--cfg=feature="usage"',
    ],
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('clap_builder', dep)
