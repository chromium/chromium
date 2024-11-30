project(
    'cxx',
    default_options: ['rust_std=2021'],
    license: 'MIT OR Apache-2.0',
    license_files: ['LICENSE-APACHE', 'LICENSE-MIT'],
    meson_version: '>= 1.3.0',
    version: '1.0.130',
)

add_languages('rust', native: true)
add_languages('rust', 'cpp', native: false)

cpp_compiler = meson.get_compiler('cpp')
add_project_arguments(
    cpp_compiler.get_supported_arguments('-Wno-dollar-in-identifier-extension'),
    language: 'cpp',
)

add_global_arguments('-Zunstable-options', language: 'rust', native: true)

subdir('tools/meson')
subdir('third-party')

rust = import('rust')

project_root = include_directories('.')

cxx_core = static_library(
    'cxx_core',
    implicit_include_directories: false,
    sources: files('src/cxx.cc'),
)

cxxbridge_macro = rust.proc_macro(
    'cxxbridge_macro',
    dependencies: [
        third_party['proc-macro2'],
        third_party['quote'],
        third_party['rustversion'],
        third_party['syn'],
    ],
    sources: files('macro/src/lib.rs'),
)

cxx_library = static_library(
    'cxx',
    link_with: [cxx_core, cxxbridge_macro],
    rust_args: [
        '--cfg=feature="alloc"',
        '--cfg=feature="default"',
        '--cfg=feature="std"',
    ],
    sources: files('src/lib.rs'),
)

cxxbridge_cmd = executable(
    'cxxbridge',
    dependencies: [
        third_party['clap'],
        third_party['codespan-reporting'],
        third_party['proc-macro2'],
        third_party['quote'],
        third_party['syn'],
    ],
    native: true,
    sources: files('gen/cmd/src/main.rs'),
)

cxxbridge_generator = generator(
    cxxbridge_cmd,
    arguments: ['@INPUT@', '-o', '@OUTPUT0@', '-o', '@OUTPUT1@'],
    output: ['@PLAINNAME@.h', '@PLAINNAME@.cc'],
)

cxx_header = custom_target(
    'cxx_header',
    command: ['bash', '-c', 'mkdir -p rust; cp @INPUT@ rust'],
    input: files('include/cxx.h'),
    output: 'rust',
)

subdir('demo')
subdir('tests')
