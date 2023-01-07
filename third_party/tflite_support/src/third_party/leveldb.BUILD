"""Builds LevelDB library."""

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

filegroup(
    name = "db_sources_group",
    srcs = [
        "db/builder.cc",
        "db/builder.h",
        "db/c.cc",
        "db/db_impl.cc",
        "db/db_impl.h",
        "db/db_iter.cc",
        "db/db_iter.h",
        "db/dbformat.cc",
        "db/dbformat.h",
        "db/filename.cc",
        "db/filename.h",
        "db/log_reader.cc",
        "db/log_writer.cc",
        "db/memtable.cc",
        "db/memtable.h",
        "db/repair.cc",
        "db/table_cache.cc",
        "db/table_cache.h",
        "db/version_edit.cc",
        "db/version_edit.h",
        "db/version_set.cc",
        "db/version_set.h",
        "db/write_batch.cc",
        "db/write_batch_internal.h",
    ],
)

filegroup(
    name = "db_headers_group",
    srcs = [
        "db/log_reader.h",
        "db/log_writer.h",
        "leveldb/c.h",
        "leveldb/db.h",
        "leveldb/export.h",
        "leveldb/write_batch.h",
    ],
)

cc_library(
    name = "db",
    srcs = [":db_sources_group"],
    hdrs = [":db_headers_group"],
    copts = ["-Wno-thread-safety"],
    visibility = ["//visibility:public"],
    deps = [
        ":internal_headers",
        ":port",
        ":table",
        #":util",
    ],
)

# Pick a particular port:
cc_library(
    name = "port",
    hdrs = [
        "port/port.h",
        "port/thread_annotations.h",
    ],
    visibility = ["//visibility:private"],
    deps = [":port_stdcxx"],
)

cc_library(
    name = "port_stdcxx",
    hdrs = [
        "leveldb/export.h",
        "port/port_config.h.in",
        "port/port_stdcxx.h",
        "port/thread_annotations.h",
    ],
    defines = [
        "LEVELDB_PLATFORM_POSIX=1",
        "HAVE_SNAPPY=1",
    ],
    # Snappy doesn't build with --config=msvc
    tags = ["non-msvc"],
    visibility = ["//visibility:private"],
    deps = [
        "@snappy",
    ],
)

filegroup(
    name = "internal_headers_group",
    srcs = [
        "db/builder.h",
        "db/db_impl.h",
        "db/db_iter.h",
        "db/dbformat.h",
        "db/filename.h",
        "db/log_format.h",
        "db/memtable.h",
        "db/skiplist.h",
        "db/snapshot.h",
        "db/table_cache.h",
        "db/version_edit.h",
        "db/version_set.h",
        "db/write_batch_internal.h",
        "table/block.h",
        "table/block_builder.h",
        "table/filter_block.h",
        "table/format.h",
        "table/iterator_wrapper.h",
        "table/merger.h",
        "table/two_level_iterator.h",
        "util/arena.h",
        "util/coding.h",
        "util/crc32c.h",
        "util/hash.h",
        "util/logging.h",
        "util/mutexlock.h",
        "util/no_destructor.h",
        "util/random.h",
        # These need to be here as they are used by the other headers.
        "db/log_reader.h",
        "db/log_writer.h",
        "leveldb/cache.h",
        "leveldb/comparator.h",
        "leveldb/db.h",
        "leveldb/env.h",
        "leveldb/export.h",
        "leveldb/filter_policy.h",
        "leveldb/iterator.h",
        "leveldb/options.h",
        "leveldb/slice.h",
        "leveldb/status.h",
        "leveldb/table.h",
        "leveldb/table_builder.h",
        "leveldb/write_batch.h",
    ] + select({
        "@platforms//os:windows": [
            "util/env_windows_test_helper.h",
            "util/windows_logger.h",
        ],
        "//conditions:default": [
            "util/env_posix_test_helper.h",
            "util/posix_logger.h",
        ],
    }),
)

cc_library(
    name = "internal_headers",
    hdrs = [":internal_headers_group"],
    visibility = ["//visibility:private"],
    deps = [":port"],
)

filegroup(
    name = "table_sources_group",
    srcs = [
        "table/block.cc",
        "table/block.h",
        "table/block_builder.cc",
        "table/block_builder.h",
        "table/filter_block.cc",
        "table/filter_block.h",
        "table/format.cc",
        "table/format.h",
        "table/iterator.cc",
        "table/merger.cc",
        "table/merger.h",
        "table/table.cc",
        "table/table_builder.cc",
        "table/two_level_iterator.cc",
        "table/two_level_iterator.h",
    ],
)

filegroup(
    name = "table_headers_group",
    srcs = [
        "leveldb/export.h",
        "leveldb/iterator.h",
        "leveldb/options.h",
        "leveldb/table.h",
        "leveldb/table_builder.h",
    ],
)

cc_library(
    name = "table",
    srcs = [":table_sources_group"],
    hdrs = [":table_headers_group"],
    visibility = ["//visibility:public"],
    deps = [
        ":internal_headers",
        ":port",
        ":util",
    ],
)

filegroup(
    name = "util_sources_group",
    srcs = [
        "util/arena.cc",
        "util/arena.h",
        "util/bloom.cc",
        "util/cache.cc",
        "util/coding.cc",
        "util/coding.h",
        "util/comparator.cc",
        "util/crc32c.cc",
        "util/crc32c.h",
        "util/env.cc",
        "util/filter_policy.cc",
        "util/hash.cc",
        "util/hash.h",
        "util/logging.cc",
        "util/logging.h",
        "util/options.cc",
        "util/status.cc",
    ],
)

filegroup(
    name = "util_headers_group",
    srcs = [
        "leveldb/cache.h",
        "leveldb/comparator.h",
        "leveldb/env.h",
        "leveldb/export.h",
        "leveldb/filter_policy.h",
        "leveldb/options.h",
        "leveldb/slice.h",
        "leveldb/status.h",
    ],
)

cc_library(
    name = "util",
    srcs = [":util_sources_group"] + select({
        "@platforms//os:windows": ["util/env_windows.cc"],
        "//conditions:default": [
            "util/env_posix.cc",
        ],
    }),
    hdrs = [":util_headers_group"],
    copts = select({
        # Visual Studio compiler does no support "no-implicit-fallthrough"
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wno-implicit-fallthrough",
        ],
    }),
    visibility = ["//visibility:public"],
    deps = [
        ":internal_headers",
        ":port",
    ],
)
