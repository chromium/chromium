project(
    'codespan-reporting',
    'rust',
    default_options: ['rust_std=2018'],
    license: 'Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '0.11.1',
)

lib = static_library(
    'codespan_reporting',
    dependencies: [
        dependency('termcolor', version: ['>= 1', '< 2']),
        dependency('unicode-width', version: ['>= 0.1', '< 0.2']),
    ],
    native: true,
    sources: files('src/lib.rs'),
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('codespan-reporting', dep)
