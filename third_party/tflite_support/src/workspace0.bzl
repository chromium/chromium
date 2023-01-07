"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@tf_toolchains//toolchains/embedded/arm-linux:arm_linux_toolchain_configure.bzl", "arm_linux_toolchain_configure")

def tflite_support_workspace0():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""

    # TFLite crossbuild toolchain for embeddeds Linux
    arm_linux_toolchain_configure(
        name = "local_config_embedded_arm",
        build_file = "@tf_toolchains//toolchains/embedded/arm-linux:BUILD",
        aarch64_repo = "../aarch64_linux_toolchain",
        armhf_repo = "../armhf_linux_toolchain",
    )
