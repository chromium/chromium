package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "zlib",
    srcs = [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "crc32.h",
        "deflate.c",
        "deflate.h",
        "gzclose.c",
        "gzguts.h",
        "gzlib.c",
        "gzread.c",
        "gzwrite.c",
        "infback.c",
        "inffast.c",
        "inffast.h",
        "inffixed.h",
        "inflate.c",
        "inflate.h",
        "inftrees.c",
        "inftrees.h",
        "trees.c",
        "trees.h",
        "uncompr.c",
        "zutil.c",
        "zutil.h",
    ],
    hdrs = [
        "zconf.h",
        "zlib.h",
    ],
    copts = ["-Wno-implicit-function-declaration"],
    includes = ["."],
)

cc_library(
    name = "zlib_minizip",
    srcs = [
        "contrib/minizip/ioapi.c",
        "contrib/minizip/miniunz.c",
        "contrib/minizip/minizip.c",
        "contrib/minizip/unzip.c",
        "contrib/minizip/zip.c",
    ],
    hdrs = [
        "contrib/minizip/crypt.h",
        "contrib/minizip/ioapi.h",
        "contrib/minizip/mztools.h",
        "contrib/minizip/unzip.h",
        "contrib/minizip/zip.h",
    ],
    copts = [
        "-Wno-dangling-else",
        "-Wno-format",
        "-Wno-incompatible-pointer-types",
        "-Wno-incompatible-pointer-types-discards-qualifiers",
        "-Wno-parentheses",
        "-DIOAPI_NO_64",
    ],
    deps = [":zlib"],
)
