load("//tools/buck:genrule.bzl", "genrule")

def rust_cxx_bridge(name, src, deps = []):
    genrule(
        name = "%s/header" % name,
        out = src + ".h",
        cmd = "cp $(location :%s/generated)/generated.h ${OUT}" % name,
    )

    genrule(
        name = "%s/source" % name,
        out = src + ".cc",
        cmd = "cp $(location :%s/generated)/generated.cc ${OUT}" % name,
    )

    genrule(
        name = "%s/generated" % name,
        srcs = [src],
        out = ".",
        cmd = "$(exe //:codegen) ${SRCS} -o ${OUT}/generated.h -o ${OUT}/generated.cc",
        type = "cxxbridge",
    )

    cxx_library(
        name = name,
        srcs = [":%s/source" % name],
        preferred_linkage = "static",
        deps = deps + [":%s/include" % name],
    )

    cxx_library(
        name = "%s/include" % name,
        exported_headers = [":%s/header" % name],
    )
