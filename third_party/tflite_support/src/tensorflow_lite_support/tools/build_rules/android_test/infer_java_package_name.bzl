"""A rule for inferring a java package name."""

_JAVA_ROOTS = [
    "javatests/",
    "javatest/",
    "java/",
]

def infer_java_package_name():
    """Infer a java package name based on current path below 'javatests' or 'java'"""
    return _infer_java_package_name_from_path(native.package_name())

def infer_java_package_name_from_label(label):
    package_path = _get_path_from_label(label)
    return _infer_java_package_name_from_path(package_path)

def _infer_java_package_name_from_path(package_path):
    for root in _JAVA_ROOTS:
        if root in package_path:
            root_index = package_path.rindex(root) + len(root)
            return package_path[root_index:].replace("/", ".")
    fail("Could not find one of java roots %s in %s" % (_JAVA_ROOTS, package_path))

def _get_path_from_label(label_string):
    label_string = label_string.split(":")[0]
    if not label_string.startswith("//"):
        label_string = "//%s%s" % (native.package_name(), label_string)
    return label_string
