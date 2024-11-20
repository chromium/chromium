project(
    'unicode-ident',
    'rust',
    default_options: ['rust_std=2018'],
    license: '(MIT OR Apache-2.0) AND Unicode-DFS-2016',
    meson_version: '>= 1.3.0',
    version: '1.0.13',
)

lib = static_library('unicode_ident', native: true, sources: files('src/lib.rs'))

dep = declare_dependency(link_with: lib)
meson.override_dependency('unicode-ident', dep)
