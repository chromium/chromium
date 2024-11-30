project(
    'rustversion',
    'rust',
    default_options: ['rust_std=2018'],
    license: 'MIT OR Apache-2.0',
    meson_version: '>= 1.3.0',
    version: '1.0.18',
)

rust = import('rust')

build = executable(
    'build_script',
    native: true,
    sources: files('build/build.rs'),
)

rustc_args = custom_target(
    command: [
        find_program('python3'),
        '@SOURCE_ROOT@/tools/meson/buildscript_run.py',
        '--buildscript',
        build,
        '--manifest-dir',
        '@CURRENT_SOURCE_DIR@',
        '--rustc-wrapper',
        '@BUILD_ROOT@/tools/meson/rustc_wrapper.sh',
        '--out-dir',
        '@PRIVATE_DIR@',
        '--rustc-args',
        '@OUTPUT@',
    ],
    env: {'HOST': 'x86_64-unknown-linux-gnu'},
    # Hack: any extension other than .rs causes a failure "ERROR: Rust target
    # rustversion contains a non-rust source file" below, and forces the use of
    # `structured_sources` which would mean listing out every source file in the
    # crate, instead of just the crate root lib.rs.
    output: 'rustc_args.out.rs',
)

lib = rust.proc_macro(
    'rustversion',
    rust_args: ['@' + rustc_args.full_path()],
    sources: [files('src/lib.rs'), rustc_args],
)

dep = declare_dependency(link_with: lib)
meson.override_dependency('rustversion', dep)
