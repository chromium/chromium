# Description:
# A collection of source from different Google projects that may be of use to
# developers working other Mac projects.
package(
    default_visibility = ["//visibility:private"],
)

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

exports_files(
    ["UnitTest-Info.plist"],
    visibility = ["//visibility:public"],
)

objc_library(
    name = "GTM_Defines",
    hdrs = ["GTMDefines.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
