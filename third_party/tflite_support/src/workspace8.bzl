"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:java.bzl", "java_import_external")
load("@libedgetpu//:workspace.bzl", "libedgetpu_dependencies")
load("//third_party/tensorflow:version.bzl", "TENSORFLOW_COMMIT", "TENSORFLOW_SHA256")

def tflite_support_workspace8():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""

    # Set up TensorFlow version for Coral.
    libedgetpu_dependencies(TENSORFLOW_COMMIT, TENSORFLOW_SHA256)

    # AutoValue 1.6+ shades Guava, Auto Common, and JavaPoet. That's OK
    # because none of these jars become runtime dependencies.
    java_import_external(
        name = "com_google_auto_value",
        jar_sha256 = "fd811b92bb59ae8a4cf7eb9dedd208300f4ea2b6275d726e4df52d8334aaae9d",
        jar_urls = [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/auto/value/auto-value/1.6/auto-value-1.6.jar",
            "https://repo1.maven.org/maven2/com/google/auto/value/auto-value/1.6/auto-value-1.6.jar",
        ],
        licenses = ["notice"],  # Apache 2.0
        generated_rule_name = "processor",
        exports = ["@com_google_auto_value_annotations"],
        extra_build_file_content = "\n".join([
            "java_plugin(",
            "    name = \"AutoAnnotationProcessor\",",
            "    output_licenses = [\"unencumbered\"],",
            "    processor_class = \"com.google.auto.value.processor.AutoAnnotationProcessor\",",
            "    tags = [\"annotation=com.google.auto.value.AutoAnnotation;genclass=${package}.AutoAnnotation_${outerclasses}${classname}_${methodname}\"],",
            "    deps = [\":processor\"],",
            ")",
            "",
            "java_plugin(",
            "    name = \"AutoOneOfProcessor\",",
            "    output_licenses = [\"unencumbered\"],",
            "    processor_class = \"com.google.auto.value.processor.AutoOneOfProcessor\",",
            "    tags = [\"annotation=com.google.auto.value.AutoValue;genclass=${package}.AutoOneOf_${outerclasses}${classname}\"],",
            "    deps = [\":processor\"],",
            ")",
            "",
            "java_plugin(",
            "    name = \"AutoValueProcessor\",",
            "    output_licenses = [\"unencumbered\"],",
            "    processor_class = \"com.google.auto.value.processor.AutoValueProcessor\",",
            "    tags = [\"annotation=com.google.auto.value.AutoValue;genclass=${package}.AutoValue_${outerclasses}${classname}\"],",
            "    deps = [\":processor\"],",
            ")",
            "",
            "java_library(",
            "    name = \"com_google_auto_value\",",
            "    exported_plugins = [",
            "        \":AutoAnnotationProcessor\",",
            "        \":AutoOneOfProcessor\",",
            "        \":AutoValueProcessor\",",
            "    ],",
            "    exports = [\"@com_google_auto_value_annotations\"],",
            ")",
        ]),
    )

    # Auto value annotations
    java_import_external(
        name = "com_google_auto_value_annotations",
        jar_sha256 = "d095936c432f2afc671beaab67433e7cef50bba4a861b77b9c46561b801fae69",
        jar_urls = [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/auto/value/auto-value-annotations/1.6/auto-value-annotations-1.6.jar",
            "https://repo1.maven.org/maven2/com/google/auto/value/auto-value-annotations/1.6/auto-value-annotations-1.6.jar",
        ],
        licenses = ["notice"],  # Apache 2.0
        neverlink = True,
        default_visibility = ["@com_google_auto_value//:__pkg__"],
    )

    http_archive(
        name = "robolectric",
        urls = ["https://github.com/robolectric/robolectric-bazel/archive/4.7.3.tar.gz"],
        strip_prefix = "robolectric-bazel-4.7.3",
    )
