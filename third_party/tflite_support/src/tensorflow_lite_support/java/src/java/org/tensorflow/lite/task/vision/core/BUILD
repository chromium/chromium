package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

# BaseVisionTaskApi is built with the source code of vision tasks together.
# Alternatively, we could build BaseVisionTaskApi into an individual library
# with its own .so, and vision tasks could all depend on it. However, it will
# increate the binary size of task-library-vision.aar by 3 MB, because the
# native dependencies of BaseVisionTaskApi are not shared with those vision
# tasks.
filegroup(
    name = "base_vision_api_src",
    srcs = glob(["**/*.java"]),
)
