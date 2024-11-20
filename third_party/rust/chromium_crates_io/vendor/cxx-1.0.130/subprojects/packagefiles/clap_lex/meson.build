project(
    'clap_lex',
    'rust',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '0.7.2',
)

lib = static_library('clap_lex', native: true, sources: files('src/lib.rs'))

dep = declare_dependency(link_with: lib)
meson.override_dependency('clap_lex', dep)
