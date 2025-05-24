"""Blaze support for C.
Defines c_binary, c_library, c_test rules for targets written in C. They are
like their cc_* counterparts, but compile with C89 standard compatibility.
"""

WARNING_OPTS =  select({
    "@bazel_tools//src/conditions:windows": [],
    "//conditions:default": [
         # Suppress "unused function" warnings on `static` functions in .h files.
         # Excluded from Windows due to lack of support by Visual Studio 2017. 
         "-Wno-unused-function",
    ]
})

# Build with C89 standard compatibility.
DEFAULT_C_OPTS = WARNING_OPTS + ["-std=c89"]

def c_binary(name = None, **kwargs):
    """cc_binary with DEFAULT_COPTS."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})
    return native.cc_binary(name = name, **kwargs)

def c_library(name = None, **kwargs):
    """cc_library with DEFAULT_C_OPTS, and hdrs is used as textual_hrds."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})

    # Use "hdrs" as "textual_hdrs". All code that cannot be standalone-compiled
    # as C++ must be listed in textual_hdrs.
    kwargs.setdefault("textual_hdrs", kwargs.pop("hdrs", None))
    return native.cc_library(name = name, **kwargs)

def c_test(name = None, **kwargs):
    """cc_test with DEFAULT_COPTS."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})
    return native.cc_test(name = name, **kwargs)
