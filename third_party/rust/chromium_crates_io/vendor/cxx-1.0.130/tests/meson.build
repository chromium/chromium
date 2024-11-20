test_module_bridge = static_library(
    'test_module',
    implicit_include_directories: false,
    include_directories: project_root,
    sources: cxxbridge_generator.process(
        files('ffi/module.rs'),
        preserve_path_from: meson.project_source_root(),
    ),
)

test_bridge = static_library(
    'test_bridge',
    implicit_include_directories: false,
    include_directories: [
        project_root,
        test_module_bridge.private_dir_include(),
    ],
    sources: cxxbridge_generator.process(
        files('ffi/lib.rs'),
        preserve_path_from: meson.project_source_root(),
    ),
)

cxx_test_suite = static_library(
    'cxx_test_suite',
    link_with: [
        cxx_library,
        static_library(
            'cxx_test_suite_impl',
            implicit_include_directories: false,
            include_directories: [
                project_root,
                test_bridge.private_dir_include(),
                test_module_bridge.private_dir_include(),
            ],
            link_with: [test_bridge, test_module_bridge],
            sources: [files('ffi/tests.cc'), cxx_header],
        ),
    ],
    sources: files('ffi/lib.rs'),
)

rust.test(
    'tests',
    static_library(
        'tests_lib',
        link_with: [cxx_library, cxx_test_suite],
        rust_args: ['-Aunused_imports', '-Aunused_macros'],
        sources: files('test.rs'),
    ),
)
