# Python Utility / Helper Modules for Android Tools

## Overview

This directory contains shared Python utility/helper modules for use by scripts
and other modules in `//tools/android`.

## Usage

1. Add `chromium_git/src/tools/android` to your Python module search path, e.g.:
    ```
    _TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve().parents[2]

    if str(_TOOLS_ANDROID_PATH) not in sys.path:
        sys.path.append(str(_TOOLS_ANDROID_PATH))
    ```
1. Import the desired utility modules using:
    ```
    from python_utils import some_util_module
    ```

Although the module search path can be modified in many ways, the example code
above ensures that `some_util_module` is specifically the one from
`//tools/android/python_utils`.

Note: A Python file's directory gets added to the search path automatically,
therefore another module named `some_util_module` in the same directory could
inadvertently be imported if using a plain `import some_util_module` statement.
The above code adds `tools/android` to the end of the module search path in case
you intentionally need to import `some_util_module` from the file's directory.
However, the local module should ideally be renamed in this  case to avoid
confusion.

## Utilities

### Git Metadata Utilities

The `git_metadata_utils` module provides helper functions for retrieving Git
repository metadata, including the SHA1 hash and timestamp of the `HEAD` commit.
Additionally, it provides a simple function to retrieve the absolute path of the
Chromium `src` directory.

### Subprocess Utilities

The `subprocess_utils` module provides helper functions for running commands as
subprocesses and returning the output as a string, as well as emitting debug
logs to the console on failure.
