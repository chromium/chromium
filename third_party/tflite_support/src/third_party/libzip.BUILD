package(
    default_visibility = ["//visibility:public"],
)

load("@org_tensorflow_lite_support//tensorflow_lite_support/tools/build_rules:expand_template.bzl", "cmake_substitutions", "expand_template")

_CMAKE_VARIABLES = {
    "INT16_T_LIBZIP": 2,
    "INT32_T_LIBZIP": 4,
    "INT64_T_LIBZIP": 8,
    "INT8_T_LIBZIP": 1,
    "INT_LIBZIP": 4,
    "LIBZIP_TYPES_INCLUDE": "#include <stdint.h>",
    "LONG_LIBZIP": 8,
    "LONG_LONG_LIBZIP": 8,
    "PACKAGE_VERSION": "1.5.1",
    "PACKAGE_VERSION_MAJOR": "1",
    "PACKAGE_VERSION_MICRO": "1",
    "PACKAGE_VERSION_MINOR": "5",
    "SHORT_LIBZIP": 2,
    "SIZEOF_OFF_T": 8,
    "SIZE_T_LIBZIP": 8,
    "SSIZE_T_LIBZIP": 8,
    "UINT16_T_LIBZIP": 2,
    "UINT32_T_LIBZIP": 4,
    "UINT64_T_LIBZIP": 8,
    "UINT8_T_LIBZIP": 1,
    "__INT16_LIBZIP": None,
    "__INT32_LIBZIP": None,
    "__INT64_LIBZIP": None,
    "__INT8_LIBZIP": None,
}

_CMAKE_VARIABLES.update(dict([
    (
        "ZIP_{sign}INT{size}_T".format(
            size = size,
            sign = sign.upper(),
        ),
        "{sign}int{size}_t".format(
            size = size,
            sign = sign.lower(),
        ),
    )
    for sign in ("U", "")
    for size in (8, 16, 32, 64)
]))

_SUBSTITUTIONS = {
    "@PACKAGE@": "libzip",
    "@VERSION@": "1.5.1",  # Keep in sync with actual package!
}

_DEFINES = {
    "HAVE_CLONEFILE": False,
    "HAVE_COMMONCRYPTO": False,
    "HAVE_CRYPTO": False,
    "HAVE_DIRENT_H": False,
    "HAVE_FICLONERANGE": False,
    "HAVE_FILENO": True,
    "HAVE_FSEEK": True,
    "HAVE_FSEEKO": True,
    "HAVE_FTELLO": True,
    "HAVE_FTS_H": True,
    "HAVE_GETPROGNAME": False,
    "HAVE_GNUTLS": False,
    "HAVE_LIBBZ2": False,
    "HAVE_MKSTEMP": True,
    "HAVE_NDIR_H": False,
    "HAVE_OPEN": True,
    "HAVE_OPENSSL": False,
    "HAVE_SETMODE": False,
    "HAVE_SHARED": True,
    "HAVE_SNPRINTF": True,
    "HAVE_SSIZE_T_LIBZIP": True,
    "HAVE_STDBOOL_H": True,
    "HAVE_STRCASECMP": True,
    "HAVE_STRDUP": True,
    "HAVE_STRICMP": False,
    "HAVE_STRINGS_H": True,
    "HAVE_STRTOLL": True,
    "HAVE_STRTOULL": True,
    "HAVE_STRUCT_TM_TM_ZONE": False,
    "HAVE_SYS_DIR_H": False,
    "HAVE_SYS_NDIR_H": False,
    "HAVE_UNISTD_H": True,
    "HAVE__CHMOD": False,
    "HAVE__CLOSE": False,
    "HAVE__DUP": False,
    "HAVE__FDOPEN": False,
    "HAVE__FILENO": False,
    "HAVE__OPEN": False,
    "HAVE__SETMODE": False,
    "HAVE__SNPRINTF": False,
    "HAVE__STRDUP": False,
    "HAVE__STRICMP": False,
    "HAVE__STRTOI64": False,
    "HAVE__STRTOUI64": False,
    "HAVE__UMASK": False,
    "HAVE__UNLINK": False,
    "HAVE___PROGNAME": False,
    "WORDS_BIGENDIAN": False,
}

_DEFINES.update(dict([(
    key,
    value != None,
) for key, value in _CMAKE_VARIABLES.items()]))

_SUBSTITUTIONS.update(cmake_substitutions(
    defines = _DEFINES,
    vars = _CMAKE_VARIABLES,
))

expand_template(
    name = "config_h",
    out = "config.h",
    substitutions = _SUBSTITUTIONS,
    template = "cmake-config.h.in",
)

_VARS = {
    "LIBZIP_TYPES_INCLUDE": "#include <stdint.h>",
    "PACKAGE_VERSION": "1.5.1",
    "PACKAGE_VERSION_MAJOR": "1",
    "PACKAGE_VERSION_MICRO": "1",
    "PACKAGE_VERSION_MINOR": "5",
}

_VARS.update(dict([
    (
        "ZIP_{sign}INT{size}_T".format(
            size = size,
            sign = sign.upper(),
        ),
        "{sign}int{size}_t".format(
            size = size,
            sign = sign.lower(),
        ),
    )
    for sign in ("U", "")
    for size in (8, 16, 32, 64)
]))

expand_template(
    name = "zipconf_h",
    out = "lib/zipconf.h",
    substitutions = cmake_substitutions(
        defines = {
            "LIBZIP_VERSION": True,
            "LIBZIP_VERSION_MAJOR": True,
            "LIBZIP_VERSION_MICRO": True,
            "LIBZIP_VERSION_MINOR": True,
            "ZIP_STATIC": False,
        },
        vars = _VARS,
    ),
    template = "cmake-zipconf.h.in",
)

cc_library(
    name = "zip",
    srcs = glob(
        [
            "lib/*.c",
            "lib/*.h",
        ],
        exclude = [
            "lib/*win32*",
            "lib/zip_random_uwp.c",
            "lib/*crypto*",
            "lib/*aes*",
            "lib/*bzip2*",
        ],
    ) + [
        "config.h",
    ],
    hdrs = [
        "lib/zip.h",
        "lib/zipconf.h",
    ],
    copts = [
        "-DHAVE_CONFIG_H",
    ],
    includes = ["lib"],
    deps = [
        "@zlib",
    ],
)
