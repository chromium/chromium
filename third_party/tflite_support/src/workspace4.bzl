"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@org_tensorflow//tensorflow:workspace1.bzl", "workspace")

# buildifier: disable=unnamed-macro
def tflite_support_workspace4(with_rules_cc = True):
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage.

    Args:
      with_rules_cc: whether to load and patch rules_cc repository.
    """
    workspace(with_rules_cc)
