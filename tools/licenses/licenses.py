#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility for checking and processing licensing information in third_party
directories.
"""
import argparse
import csv
import html
import io
import json
import logging
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional

from spdx_writer import SpdxWriter

_REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(_REPOSITORY_ROOT, 'build'))
import action_helpers

METADATA_FILE_NAMES = frozenset({
    "README.chromium", "README.crashpad", "README.v8", "README.pdfium",
    "README.angle"
})

_DEFAULT_FILE_TEMPLATE_FILE = os.path.join(_REPOSITORY_ROOT, 'components',
                                           'webui', 'about', 'resources',
                                           'about_credits.tmpl')
_DEFAULT_ENTRY_TEMPLATE_FILE = os.path.join(_REPOSITORY_ROOT, 'components',
                                            'webui', 'about', 'resources',
                                            'about_credits_entry.tmpl')
_DEFAULT_RECIPROCAL_TEMPLATE_FILE = os.path.join(
    _REPOSITORY_ROOT, 'components', 'webui', 'about', 'resources',
    'about_credits_reciprocal.tmpl')

_CHROMIUM_LICENSE_METADATA = {
    'Name': 'The Chromium Project',
    'URL': 'https://www.chromium.org',
    'License': 'BSD-3-Clause',
    'Shipped': 'yes',
    'License File': ['LICENSE'],
    'dir': '',  # Relative to _REPOSITORY_ROOT
}

# Paths from the root of the tree to directories to skip.
PRUNE_PATHS = set([
    # Placeholder directory only, not third-party code.
    os.path.join('third_party', 'adobe'),

    # Will remove it once converted private sdk using cipd.
    os.path.join('third_party', 'android_tools_internal'),

    # Build files only, not third-party code.
    os.path.join('third_party', 'widevine'),

    # Only binaries, used during development.
    os.path.join('third_party', 'valgrind'),

    # Supplies configuration setting for enabling or disabling field trials and
    # features in Chromium projects.
    os.path.join('components', 'variations', 'test_data', 'cipd'),

    # Used for development and test, not in the shipping product.
    os.path.join('build', 'secondary'),
    os.path.join('third_party', 'bison'),
    os.path.join('third_party', 'chromite'),
    os.path.join('third_party', 'clang-format'),
    os.path.join('third_party', 'cygwin'),
    os.path.join('third_party', 'gnu_binutils'),
    os.path.join('third_party', 'gold'),
    os.path.join('third_party', 'gperf'),
    os.path.join('third_party', 'lighttpd'),
    os.path.join('third_party', 'llvm'),
    os.path.join('third_party', 'llvm-bootstrap'),
    os.path.join('third_party', 'llvm-bootstrap-install'),
    os.path.join('third_party', 'llvm-build'),
    os.path.join('third_party', 'llvm-build-tools'),
    os.path.join('third_party', 'mingw-w64'),
    os.path.join('third_party', 'nacl_sdk_binaries'),
    os.path.join('third_party', 'pefile'),
    os.path.join('third_party', 'perl'),
    os.path.join('third_party', 'psyco_win32'),
    os.path.join('third_party', 'pyelftools'),
    os.path.join('third_party', 'pylib'),
    os.path.join('third_party', 'pywebsocket'),
    os.path.join('third_party', 'rust-src'),
    os.path.join('third_party', 'rust-toolchain-intermediate'),
    os.path.join('third_party', 'syzygy'),

    # Stuff pulled in from chrome-internal for official builds/tools.
    os.path.join('third_party', 'amd'),
    os.path.join('third_party', 'clear_cache'),
    os.path.join('third_party', 'gnu'),
    os.path.join('third_party', 'googlemac'),
    os.path.join('third_party', 'pcre'),
    os.path.join('third_party', 'psutils'),
    os.path.join('third_party', 'sawbuck'),
    os.path.join('third_party', 'wix'),
    # See crbug.com/350472
    os.path.join('chrome', 'browser', 'resources', 'chromeos', 'quickoffice'),

    # Proprietary barcode detection library.
    os.path.join('third_party', 'barhopper'),

    # Internal Chrome Build only for proprietary webref library.
    os.path.join('components', 'optimization_guide', 'internal', 'third_party'),

    # Proprietary DevTools code.
    os.path.join('third_party', 'devtools-frontend-internal'),

    # Redistribution does not require attribution in documentation.
    os.path.join('third_party', 'directxsdk'),

    # For testing only, presents on some bots.
    os.path.join('isolate_deps_dir'),

    # Mock test data.
    os.path.join('tools', 'binary_size', 'libsupersize', 'testdata'),

    # Overrides some WebRTC files, same license. Skip this one.
    os.path.join('third_party', 'webrtc_overrides'),

    # Integration tests entry
    os.path.join('third_party', 'pruned'),
])

# Directories we don't scan through.
VCS_METADATA_DIRS = ('.svn', '.git')
PRUNE_DIRS = VCS_METADATA_DIRS + ('layout_tests', )  # lots of subdirs

# A third_party directory can define this file, containing a list of
# subdirectories to process in addition to itself. Intended for directories
# that contain multiple others as transitive dependencies.
ADDITIONAL_PATHS_FILENAME = 'additional_readme_paths.json'

# A list of paths that contain license information but that would otherwise
# not be included.  Possible reasons include:
#   - Third party directories in //internal which are considered to be Google-owned
#   - Directories that are directly checked out from upstream, and thus
#     don't have a README.chromium
#   - Directories that contain example code, or build tooling.
#   - Nested third_party code inside other third_party libraries.
ADDITIONAL_PATHS = (
    os.path.join('chrome', 'test', 'chromeos', 'autotest'),
    os.path.join('chrome', 'test', 'data'),
    os.path.join('native_client'),
    os.path.join('third_party', 'android_deps', 'autorolled'),
    os.path.join('third_party', 'boringssl', 'src', 'third_party', 'fiat'),
    os.path.join('third_party', 'devtools-frontend', 'src', 'front_end',
                 'third_party'),
    os.path.join('third_party', 'devtools-frontend-internal', 'front_end',
                 'third_party'),
    os.path.join('tools', 'page_cycler', 'acid3'),
    os.path.join('url', 'third_party', 'mozilla'),
    os.path.join('v8'),
    # Fake directories to include the strongtalk and fdlibm licenses.
    os.path.join('v8', 'strongtalk'),
    os.path.join('v8', 'fdlibm'),
)

# SPECIAL_CASES are used for historical directories where we checked out
# directly from upstream into the same directory as we would put metadata.
# In new cases, you should check out upstream source into //root/foo/src
# and keep the corresponding README.chromium file at //root/foo/README.chromium
# instead of adding a SPECIAL_CASE.
# These SPECIAL_CASES should not be used to suppress errors. Please fix
# any metadata files with errors and if you encounter a parsing issue,
# please file a bug.
SPECIAL_CASES = {
    os.path.join('native_client'): {
        "Name": "native client",
        "URL": "https://code.google.com/p/nativeclient",
        "Shipped": "yes",
        "License": "BSD",
        "License File": ["//native_client/LICENSE"],
    },
    os.path.join('third_party', 'angle'): {
        "Name": "Almost Native Graphics Layer Engine",
        "URL": "https://chromium.googlesource.com/angle/angle/",
        "Shipped": "yes",
        "License": "BSD",
    },
    os.path.join('third_party', 'cros_system_api'): {
        "Name": "Chromium OS system API",
        "URL": "https://www.chromium.org/chromium-os",
        "Shipped": "yes",
        "License": "BSD",
        # Absolute path here is resolved as relative to the source root.
        "License File": ["//LICENSE.chromium_os"],
    },
    os.path.join('third_party', 'ipcz'): {
        "Name": "ipcz",
        "URL": (
            "https://chromium.googlesource.com/chromium/src/third_party/ipcz"),
        "Shipped": "yes",
        "License": "BSD",
        "License File": ["//third_party/ipcz/LICENSE"],
    },
    os.path.join('third_party', 'lss'): {
        "Name": "linux-syscall-support",
        "URL": "https://chromium.googlesource.com/linux-syscall-support/",
        "Shipped": "yes",
        "License": "BSD",
        "License File": ["//third_party/lss/LICENSE"],
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party', 'abseil'): {
        "Name": "abseil",
        "URL": "https://github.com/abseil/abseil-cpp/",
        "Shipped": "yes",
        "License": "Apache 2.0",
        "License File": ["//third_party/abseil-cpp/LICENSE"],
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party',
                 'boringssl'): {
        "Name": "BoringSSL",
        "URL": "https://boringssl.googlesource.com/boringssl/",
        "Shipped": "yes",
        "License": "BSDish",
        "License File": ["//third_party/boringssl/src/LICENSE"],
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party',
                 'jsoncpp'): {
        "Name": "jsoncpp",
        "URL": "https://github.com/open-source-parsers/jsoncpp",
        "Shipped": "yes",
        "License": "MIT",
        "License File": ["//third_party/jsoncpp/LICENSE"],
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party',
                 'mozilla'): {
        "Name": "mozilla",
        "URL": "https://github.com/mozilla",
        "Shipped": "yes",
        "License": "MPL 1.1/GPL 2.0/LGPL 2.1",
        "License File": ["LICENSE.txt"],
    },
    os.path.join('third_party', 'pdfium'): {
        "Name": "PDFium",
        "URL": "https://pdfium.googlesource.com/pdfium/",
        "Shipped": "yes",
        "License": "BSD",
    },
    os.path.join('third_party', 'ppapi'): {
        "Name": "ppapi",
        "URL": "https://code.google.com/p/ppapi/",
        "Shipped": "yes",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party',
                 'getopt'): {
        "Name": "getopt",
        "URL": "https://sourceware.org/ml/newlib/2005/msg00758.html",
        "Shipped": "yes",
        "License": "Public domain",
        "License File": [
            "//third_party/crashpad/crashpad/third_party/getopt/LICENSE",
        ],
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party', 'xnu'): {
        "Name": "xnu",
        "URL": "https://opensource.apple.com/source/xnu/",
        "Shipped": "yes",
        "License": "Apple Public Source License 2.0",
        "License File": ["APPLE_LICENSE"],
    },
    os.path.join('third_party', 'v8-i18n'): {
        "Name": "Internationalization Library for v8",
        "URL": "https://code.google.com/p/v8-i18n/",
        "Shipped": "yes",
        "License": "Apache 2.0",
    },
    os.path.join('third_party', 'blink'): {
        # about:credits doesn't show "Blink" but "WebKit".
        # Blink is a fork of WebKit, and Chromium project has maintained it
        # since the fork.  about:credits needs to mention the code before
        # the fork.
        "Name": "WebKit",
        "URL": "https://webkit.org/",
        "Shipped": "yes",
        "License": "BSD and LGPL v2 and LGPL v2.1",
        # Absolute path here is resolved as relative to the source root.
        "License File": ["//third_party/blink/LICENSE_FOR_ABOUT_CREDITS"],
    },
    os.path.join('v8'): {
        "Name": "V8 JavaScript Engine",
        "URL": "https://v8.dev/",
        "Shipped": "yes",
        "License": "BSD",
    },
    os.path.join('v8', 'strongtalk'): {
        "Name": "Strongtalk",
        "URL": "https://www.strongtalk.org/",
        "Shipped": "yes",
        "License": "BSD",
        # Absolute path here is resolved as relative to the source root.
        "License File": ["//v8/LICENSE.strongtalk"],
    },
    os.path.join('v8', 'fdlibm'): {
        "Name": "fdlibm",
        "URL": "https://www.netlib.org/fdlibm/",
        "Shipped": "yes",
        "License": "Freely Distributable",
        # Absolute path here is resolved as relative to the source root.
        "License File": ["//v8/LICENSE.fdlibm"],
        "License Android Compatible": "yes",
    },
    os.path.join('third_party', 'swiftshader'): {
        "Name": "SwiftShader",
        "URL": "https://swiftshader.googlesource.com/SwiftShader",
        "Shipped": "yes",
        "License": "Apache 2.0 and compatible licenses",
        "License Android Compatible": "yes",
        "License File": ["//third_party/swiftshader/LICENSE.txt"],
    },
    os.path.join('third_party', 'swiftshader', 'third_party', 'SPIRV-Tools'): {
        "Name": "SPIRV-Tools",
        "URL": "https://github.com/KhronosGroup/SPIRV-Tools",
        "Shipped": "yes",
        "License": "Apache 2.0",
        "License File": [
            "//third_party/swiftshader/third_party/SPIRV-Tools/LICENSE",
        ],
    },
    os.path.join('third_party', 'swiftshader', 'third_party',
                 'SPIRV-Headers'): {
        "Name": "SPIRV-Headers",
        "URL": "https://github.com/KhronosGroup/SPIRV-Headers",
        "Shipped": "yes",
        "License": "Apache 2.0",
        "License File": [
            "//third_party/swiftshader/third_party/SPIRV-Headers/LICENSE",
        ],
    },
    os.path.join('third_party', 'dawn', 'third_party', 'khronos'): {
        "Name": "khronos_platform",
        "URL": "https://registry.khronos.org/EGL/",
        "Shipped": "yes",
        "License": "Apache 2.0",
        "License File": ["//third_party/dawn/third_party/khronos/LICENSE"],
    },
    # This entry is for the integration tests.
    os.path.join('third_party', 'sample3'): {
        "Name": "Sample 3",
        "URL": "https://sample3",
        "Shipped": "yes",
        "License": "Apache 2.0",
        "License File": ["//third_party/sample3/the_license"],
    },
}

# These buildtools/third_party directories only contain
# chromium build files. The actual third_party source files and their
# README.chromium files are under third_party/libc*/.
# So we do not include licensing metadata for these directories.
# See crbug.com/1458042 for more details.
PRUNE_PATHS.update([
    os.path.join('buildtools', 'third_party', 'libc++'),
    os.path.join('buildtools', 'third_party', 'libc++abi'),
    os.path.join('buildtools', 'third_party', 'libunwind'),
])

# The mandatory metadata fields for a single dependency.
MANDATORY_FIELDS = {
    "Name",  # Short name (for header on about:credits).
    "URL",  # Project home page.
    "License",  # Software license.
    "License File",  # Relative paths to license texts.
    "Shipped",  # Whether the package is in the shipped product.
}

# Field aliases (key is the alias, value is the field to map to).
# Note: if both fields are provided, the alias field value will be used.
ALIAS_FIELDS = {
    "Shipped in Chromium": "Shipped",
}

# The metadata fields that can have multiple values.
MULTIVALUE_FIELDS = {
    "License File",
}

# Line used to separate dependencies within the same metadata file.
PATTERN_DEPENDENCY_DIVIDER = re.compile(r"^-{20} DEPENDENCY DIVIDER -{20}$")

# The delimiter used to separate multiple values for one metadata field.
VALUE_DELIMITER = ","

# Soon-to-be-deprecated special value for 'License File' field used to indicate
# that the library is not shipped so the license file should not be used in
# about:credits.
# This value is still supported, but the preferred method is to set the
# 'Shipped' field to 'no' in the library's README.chromium.
NOT_SHIPPED = "NOT_SHIPPED"

# Valid values for the 'Shipped' field used to indicate whether the library is
# shipped and consequently whether the license file should be used in
# about:credits.
YES = "yes"
NO = "no"

# Paths for libraries that we have checked are not shipped on iOS. These are
# left out of the licenses file primarily because we don't want to cause a
# firedrill due to someone thinking that Chrome for iOS is using LGPL code
# when it isn't.
# This is a temporary hack; the real solution is crbug.com/178215
KNOWN_NON_IOS_LIBRARIES = set([
    os.path.join('base', 'third_party', 'symbolize'),
    os.path.join('base', 'third_party', 'xdg_mime'),
    os.path.join('base', 'third_party', 'xdg_user_dirs'),
    os.path.join('chrome', 'installer', 'mac', 'third_party', 'bsdiff'),
    os.path.join('chrome', 'installer', 'mac', 'third_party', 'xz'),
    os.path.join('chrome', 'test', 'data', 'third_party', 'spaceport'),
    os.path.join('chrome', 'third_party', 'mozilla_security_manager'),
    os.path.join('third_party', 'angle'),
    os.path.join('third_party', 'apple_apsl'),
    os.path.join('third_party', 'apple_sample_code'),
    os.path.join('third_party', 'ashmem'),
    os.path.join('third_party', 'blink'),
    os.path.join('third_party', 'bspatch'),
    os.path.join('third_party', 'cld'),
    os.path.join('third_party', 'flot'),
    os.path.join('third_party', 'gtk+'),
    os.path.join('third_party', 'iaccessible2'),
    os.path.join('third_party', 'isimpledom'),
    os.path.join('third_party', 'jsoncpp'),
    os.path.join('third_party', 'khronos'),
    os.path.join('third_party', 'libcxx', 'libc++'),
    os.path.join('third_party', 'libcxx', 'libc++abi'),
    os.path.join('third_party', 'libevent'),
    os.path.join('third_party', 'libjpeg'),
    os.path.join('third_party', 'libusb'),
    os.path.join('third_party', 'libxslt'),
    os.path.join('third_party', 'lss'),
    os.path.join('third_party', 'lzma_sdk'),
    os.path.join('third_party', 'mesa'),
    os.path.join('third_party', 'mozc'),
    os.path.join('third_party', 'mozilla'),
    os.path.join('third_party', 'npapi'),
    os.path.join('third_party', 'ots'),
    os.path.join('third_party', 'perfetto'),
    os.path.join('third_party', 'ppapi'),
    os.path.join('third_party', 're2'),
    os.path.join('third_party', 'safe_browsing'),
    os.path.join('third_party', 'smhasher'),
    os.path.join('third_party', 'swiftshader'),
    os.path.join('third_party', 'swig'),
    os.path.join('third_party', 'talloc'),
    os.path.join('third_party', 'usb_ids'),
    os.path.join('third_party', 'v8-i18n'),
    os.path.join('third_party', 'wtl'),
    os.path.join('third_party', 'yasm'),
    os.path.join('v8', 'strongtalk'),
])


_read_paths = set()


def _read_file(path):
  _read_paths.add(path)
  return pathlib.Path(path).read_text(encoding='utf-8', errors='replace')


class InvalidMetadata(Exception):
  """This exception is raised when metadata is invalid."""
  pass


class LicenseError(Exception):
  """We raise this exception when a dependency's licensing info isn't
  fully filled out.
  """
  pass


def _ResolvePath(path, filename, root):
  """Convert a path in README.chromium to be relative to |root|."""
  if filename.startswith('/'):
    # Absolute-looking paths are relative to the source root
    # (which is the directory we're run from).
    absolute_path = os.path.join(root, os.path.normpath(filename.lstrip('/')))
  else:
    absolute_path = os.path.join(root, path, os.path.normpath(filename))
  if os.path.exists(absolute_path):
    return os.path.relpath(absolute_path, root)
  return None


def ParseMetadataFile(filepath: str,
                      optional_fields: List[str] = []) -> List[Dict[str, Any]]:
  """Parses the metadata from the file.

  Args:
    filepath: the path to a file from which to parse metadata.
    optional_fields: list of optional metadata fields.

  Returns: the metadata for all dependencies described in the file.

  Raises:
    InvalidMetadata - if the metadata in the file has duplicate fields
                      for a dependency.
  """
  known_fields = (list(MANDATORY_FIELDS) + list(ALIAS_FIELDS.keys()) +
                  optional_fields)
  field_lookup = {name.lower(): name for name in known_fields}

  dependencies = []
  metadata = {}
  for line in _read_file(filepath).splitlines():
    line = line.strip()
    # Skip empty lines.
    if not line:
      continue

    # Check if a new dependency will be described.
    if re.match(PATTERN_DEPENDENCY_DIVIDER, line):
      # Save the metadata for the previous dependency.
      if metadata:
        dependencies.append(metadata)
      metadata = {}
      continue

    # Otherwise, try to parse the field name and field value.
    parts = line.split(": ", 1)
    if len(parts) == 2:
      raw_field, value = parts
      field = field_lookup.get(raw_field.lower())
      if field:
        if field in metadata:
          # Duplicate field for this dependency.
          raise InvalidMetadata(f"duplicate '{field}' in {filepath}")
        if field in MULTIVALUE_FIELDS:
          metadata[field] = [
              entry.strip() for entry in value.split(VALUE_DELIMITER)
          ]
        else:
          metadata[field] = value

  # The end of the file has been reached. Save the metadata for the
  # last dependency, if available.
  if metadata:
    dependencies.append(metadata)

  return dependencies


def ProcessMetadata(metadata: Dict[str, Any],
                    readme_path: str,
                    path: str,
                    root: str,
                    require_license_file: bool = True,
                    enable_warnings: bool = False) -> List[str]:
  """Processes a single dependency's metadata and returns the updated
  data if it passes validation. This function updates the given metadata
  to use fallback fields and change any relative paths to absolute.

  Args:
    metadata: a single dependency's metadata.
    readme_path: the source of the metadata (either a metadata file
                 or a SPECIAL_CASES entry).
    path: the source file for the metadata.
    root: the root directory of the repo.
    require_license_file: whether a license file is required.
    enable_warnings: whether warnings should be displayed.

  Returns: error messages, if there were any issues processing the
           metadata for license information.
  """
  errors = []

  # The dependency reference, for more precise error messages.
  dep_ref = os.path.relpath(readme_path, root)
  dep_name = metadata.get("Name")
  if dep_name:
    dep_ref = f"{dep_ref}>>{dep_name}"

  # Set field values for fields with aliases.
  for alias, field in ALIAS_FIELDS.items():
    if alias in metadata:
      metadata[field] = metadata[alias]
      metadata.pop(alias)

  # Set the default "License File" value.
  if metadata.get("License File") is None:
    metadata["License File"] = ["LICENSE"]

  # If the "Shipped" field isn't present (or is empty), set it based on
  # the value of the "License File" field.
  if not metadata.get("Shipped"):
    shipped = YES
    if NOT_SHIPPED in metadata.get("License File"):
      shipped = NO
    metadata["Shipped"] = shipped

  # Check all mandatory fields have a non-empty value.
  for field in MANDATORY_FIELDS:
    if not metadata.get(field):
      errors.append(f"couldn't find '{field}' line in README.chromium or "
                    "licenses.py SPECIAL_CASES")

  license_file_value = metadata.get("License File")
  shipped_value = metadata.get("Shipped")
  if enable_warnings:
    # Check for the deprecated special value used in the "License File"
    # field.
    if NOT_SHIPPED in license_file_value:
      logging.warning(
          f"{dep_ref} is using deprecated {NOT_SHIPPED} value "
          "in 'License File' field - remove this and instead specify "
          f"'Shipped: {NO}'.")

      # Check the "Shipped" field does not contradict the "License File"
      # field.
      if shipped_value == YES:
        logging.warning(
            f"Contradictory metadata for {dep_ref} - 'Shipped: {YES}' "
            f"but 'License File' includes '{NOT_SHIPPED}'")

  # For the modules that are in the shipping product, we need their
  # license in about:credits, so update the license files to be the
  # full paths.
  license_paths = process_license_files(root, path, license_file_value)
  if shipped_value == YES and require_license_file and not license_paths:
    errors.append(
        f"License file not found for {dep_ref}. Either add a file named "
        "LICENSE, import upstream's COPYING if available, or add a "
        "'License File:' line to README.chromium with the appropriate paths.")
  metadata["License File"] = license_paths

  if errors:
    # if there were any errors during parsing, clear all values from
    # the dependenct metadata so no further processing occurs
    metadata = {}

  return errors


def ParseDir(path,
             root,
             require_license_file=True,
             optional_keys=[],
             enable_warnings=False,
             metadata_file_names=METADATA_FILE_NAMES):
  """Examine a third_party path and extract that directory's metadata.

  Note: directory metadata can contain metadata for multiple
  dependencies.

  Returns: A tuple with a list of directory metadata, and accrued parsing errors

  """
  # Get the metadata values, from
  # (a) looking up the path in SPECIAL_CASES; or
  # (b) parsing the metadata from a README.chromium file.
  if path in SPECIAL_CASES:
    readme_path = f"licenses.py SPECIAL_CASES entry for {path}"
    directory_metadata = dict(SPECIAL_CASES[path])
    errors = ProcessMetadata(directory_metadata,
                             readme_path,
                             path,
                             root,
                             require_license_file=require_license_file,
                             enable_warnings=enable_warnings)

    return [directory_metadata], errors

  errors = []
  readmes_in_dir = False
  valid_metadata = []
  directory_metadata = []

  for name in metadata_file_names:
    for readme_path in (pathlib.Path(root) / path).glob(name):
      readmes_in_dir = True

      try:
        file_metadata = ParseMetadataFile(str(readme_path),
                                          optional_fields=optional_keys)
        for dependency_metadata in file_metadata:
          meta_errors = ProcessMetadata(
              dependency_metadata,
              readme_path,
              path,
              root,
              require_license_file=require_license_file,
              enable_warnings=enable_warnings)

          if meta_errors:
            errors.append(
                "Errors in %s:\n %s\n" %
                (os.path.relpath(readme_path, root), ";\n ".join(meta_errors)))
            continue

          if dependency_metadata:
            valid_metadata.append(dependency_metadata)

      except InvalidMetadata as e:
        errors.append(f"Invalid metadata file: {e}")
        continue

  if not readmes_in_dir:
    raise LicenseError(f"missing third party metadata file "
                       f"or licenses.py SPECIAL_CASES entry in {path}\n")

  return valid_metadata, errors


def process_license_files(
    root: str,
    path: str,
    license_files: List[str],
) -> List[str]:
  """
  Convert a list of license file paths which were specified in a
  README.chromium to be absolute paths based on the source root.

  Args:
    root: the repository source root.
    path: the relative path from root.
    license_files: list of values specified in the 'License File' field.

  Returns: absolute paths to license files that exist.
  """
  license_paths = []
  for file_path in license_files:
    if file_path == NOT_SHIPPED:
      continue

    license_path = _ResolvePath(path, file_path, root)
    # Check that the license file exists.
    if license_path is not None:
      license_paths.append(license_path)

  # If there are no license files at all, check for the COPYING license file.
  if not license_paths:
    license_path = _ResolvePath(path, "COPYING", root)
    # Check that the license file exists.
    if license_path is not None:
      license_paths.append(license_path)

  return license_paths


def _ContainsFiles(path):
  """Determines whether any files exist in a directory or in any of its
    subdirectories."""
  for _, dirs, files in os.walk(path):
    if files:
      return True
    for vcs_metadata in VCS_METADATA_DIRS:
      if vcs_metadata in dirs:
        dirs.remove(vcs_metadata)
  return False


def _MaybeAddThirdPartyPath(root, dirpath, third_party_dirs):
  """Adds |dirpath| to |third_party_dirs| and processes
  additional_readme_paths.json."""
  # Prune paths and guard against cycles.
  if dirpath in PRUNE_PATHS or dirpath in third_party_dirs:
    return
  # Guard against missing / empty dirs (gclient creates empty directories for
  # conditionally downloaded submodules).
  resolved_path = os.path.join(root, dirpath)
  if not os.path.exists(resolved_path) or not _ContainsFiles(resolved_path):
    return
  third_party_dirs.add(dirpath)

  additional_paths_file = os.path.join(root, dirpath, ADDITIONAL_PATHS_FILENAME)
  if not os.path.exists(additional_paths_file):
    return
  additional_paths = json.loads(_read_file(additional_paths_file))
  for p in additional_paths:
    subpath = os.path.normpath(os.path.join(dirpath, p))
    _MaybeAddThirdPartyPath(root, subpath, third_party_dirs)


def FindThirdPartyDirs(root,
                       exclude_dirs: Optional[List[str]] = None,
                       extra_third_party_dirs: Optional[List[str]] = None):
  """Find all third_party directories underneath the source root."""
  third_party_dirs = set()
  for path, dirs, files in os.walk(root):
    path = path[len(root) + 1:]  # Pretty up the path.

    # .gitignore ignores /out*/, so do the same here.
    if path in PRUNE_PATHS or path.startswith('out'):
      dirs[:] = []
      continue

    # Don't recurse into paths in ADDITIONAL_PATHS, like we do with regular
    # third_party/foo paths.
    if path in ADDITIONAL_PATHS:
      dirs[:] = []

    # Prune out directories we want to skip.
    # (Note that we loop over PRUNE_DIRS so we're not iterating over a
    # list that we're simultaneously mutating.)
    for skip in PRUNE_DIRS:
      if skip in dirs:
        dirs.remove(skip)

    # If `exclude_dirs` are specified then prune them as well.
    if exclude_dirs:
      for skip in exclude_dirs:
        if skip in dirs:
          dirs.remove(skip)

    if os.path.basename(path) == 'third_party':
      # Add all subdirectories that are not marked for skipping.
      for d in dirs:
        dirpath = os.path.join(path, d)
        _MaybeAddThirdPartyPath(root, dirpath, third_party_dirs)


  extra_paths = set(ADDITIONAL_PATHS)
  if extra_third_party_dirs:
    extra_paths.update(extra_third_party_dirs)

  for path in extra_paths:
    _MaybeAddThirdPartyPath(root, path, third_party_dirs)

  return sorted(third_party_dirs)


# Many builders do not contain 'gn' in their PATH, so use the GN binary from
# //buildtools.
def _GnBinary():
  gn_path = os.environ.get('LICENSES_GN_PATH')
  if gn_path:
    return [sys.executable, gn_path]

  exe = 'gn'
  if sys.platform.startswith('linux'):
    subdir = 'linux64'
  elif sys.platform == 'darwin':
    subdir = 'mac'
  elif sys.platform == 'win32':
    subdir, exe = 'win', 'gn.exe'
  else:
    raise RuntimeError("Unsupported platform '%s'." % sys.platform)

  return [os.path.join(_REPOSITORY_ROOT, 'buildtools', subdir, exe)]


def LogParseDirErrors(errors):
  """Provides a convenience method for printing out the errors resulting
  from running ParseDir() over a directory."""

  for error in sorted(errors):
    print(error)


def GetThirdPartyDepsFromGNDepsOutput(
    scan_root: str,
    gn_deps: List[str],
    exclude_dirs: Optional[List[str]] = None,
    extra_allowed_dirs: Optional[List[str]] = None):
  """Returns third_party/foo directories given the output of "gn desc deps".

    Note that it always returns the direct sub-directory of third_party
    where README.chromium and LICENSE files are, so that it can be passed to
    ParseDir(). e.g.:
        third_party/cld_3/src/src/BUILD.gn -> third_party/cld_3/
    Rust dependencies are a special case, with a deeper structure:
        third_party/rust/foo/v1/crate/BUILD.gn -> third_party/rust/foo/v1/

    Returns relative paths from scan_root.
    """
  allowed_paths_list = ['third_party']
  if extra_allowed_dirs:
    allowed_paths_list.extend(extra_allowed_dirs)

  # Use non-capturing group with or's for all possible options.
  allowed_paths = '|'.join([re.escape(x) for x in allowed_paths_list])
  sep = re.escape(os.path.sep)
  path_regex = re.compile(
      r'''^
            (                                     # capture
              (.+{sep})?                          # any prefix
              (?:{allowed_paths})                 # any of the allowed paths
              {sep}
              (?:                                 # either..
                rust{sep}{nonsep}+{sep}v{nonsep}+ #  rust/<crate>/v<version>
                |{nonsep}+)                       #  or any single path element
              {sep}
            )
            (.+{sep})?BUILD\.gn$                  # with filename BUILD.gn
  '''.format(allowed_paths=allowed_paths, sep=sep, nonsep=f'[^{sep}]'),
      re.VERBOSE)

  third_party_deps = set()
  for absolute_build_dep in gn_deps:
    relative_build_dep = os.path.relpath(absolute_build_dep, scan_root)
    m = path_regex.search(relative_build_dep)
    if not m:
      continue
    third_party_path = m.group(1)
    if any(third_party_path.startswith(p + os.sep) for p in PRUNE_PATHS):
      continue
    if exclude_dirs:
      if any(third_party_path.startswith(p + os.sep) for p in exclude_dirs):
        continue
    third_party_deps.add(third_party_path[:-1])
  return third_party_deps


def FindThirdPartyDeps(gn_out_dir: str,
                       gn_target: str,
                       scan_root: str,
                       exclude_dirs: Optional[List[str]] = None,
                       extra_third_party_dirs: Optional[List[str]] = None,
                       extra_allowed_dirs: Optional[List[str]] = None):
  if not gn_out_dir:
    raise RuntimeError("--gn-out-dir is required if --gn-target is used.")

  # Generate gn project in temp directory and use it to find dependencies.
  # Current gn directory cannot be used when we run this script in a gn action
  # rule, because gn always evaluate *.gn/*.gni and causes side-effect
  # by `write_file`, `exec_script` or so, and "gn desc" requires "build.ninja".
  # If only "args.gn", it fails with "ERROR Not a build directory."
  with tempfile.TemporaryDirectory(
      dir=os.path.join(gn_out_dir, '..')) as tmp_dir:
    shutil.copy(os.path.join(gn_out_dir, "args.gn"), tmp_dir)
    # "gn desc" requires "build.ninja", but ok with empty "build.ninja".
    # "gn gen" is slow and requires too much memory.
    with open(os.path.join(tmp_dir, "build.ninja"), "w") as w:
      pass
    cmd = _GnBinary() + [
        "desc",
        "--root=%s" % scan_root, tmp_dir, gn_target, "deps", "--as=buildfile",
        "--all"
    ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, encoding='utf-8')
    if proc.returncode:
      sys.stderr.write('Failed: ' + shlex.join(cmd) + '\n')
      sys.stderr.write(proc.stdout)
      sys.exit(1)
    # Filter out any warnings messages. E.g. those about unused GN args.
    # https://crbug.com/444024516
    gn_deps = [l for l in proc.stdout.splitlines() if l.endswith('BUILD.gn')]

  third_party_deps = GetThirdPartyDepsFromGNDepsOutput(scan_root, gn_deps,
                                                       exclude_dirs,
                                                       extra_allowed_dirs)
  if extra_third_party_dirs:
    third_party_deps.update(extra_third_party_dirs)
  return sorted(third_party_deps)


def _DiscoverMetadatas(args):
  # Convert the path separators to the OS specific ones
  extra_third_party_dirs = args.extra_third_party_dirs
  if extra_third_party_dirs is not None:
    extra_third_party_dirs = [
        os.path.normpath(path) for path in extra_third_party_dirs
    ]

  if args.gn_target is not None:
    third_party_dirs = FindThirdPartyDeps(args.gn_out_dir, args.gn_target,
                                          args.scan_root, args.exclude_dirs,
                                          extra_third_party_dirs,
                                          args.extra_allowed_dirs)

    # Sanity-check to raise a build error if invalid gn_... settings are
    # somehow passed to this script.
    if not third_party_dirs:
      raise RuntimeError("No deps found.")

  else:
    third_party_dirs = FindThirdPartyDirs(args.scan_root, args.exclude_dirs,
                                          extra_third_party_dirs)

  metadatas = []
  had_errors = False
  for d in third_party_dirs:
    try:
      dir_metadatas, errors = ParseDir(d,
                                       args.scan_root,
                                       require_license_file=True,
                                       enable_warnings=args.enable_warnings)

      for m in dir_metadatas:
        if args.shipped_only and m['Shipped'] == NO:
          continue
        m['dir'] = d
        metadatas.append(m)
    except LicenseError as lic_exp:
      had_errors = True
      # TODO(phajdan.jr): Convert to fatal error (https://crbug.com/39240).
      if args.enable_warnings:
        print(f"Error: {lic_exp}")
      continue

    if args.enable_warnings and errors:
      had_errors = True
      LogParseDirErrors(errors)

  metadatas.sort(key=lambda m: (m['Name'].lower(), m['dir']))
  metadatas = [_CHROMIUM_LICENSE_METADATA] + metadatas
  return metadatas, had_errors


def _WriteIfChanged(path, data):
  output_path = pathlib.Path(path)
  changed = True
  try:
    old_output = output_path.read_text('utf-8')
    changed = old_output != data
  except:
    pass
  if changed:
    output_path.write_text(data, 'utf-8')


def _ListLicenses(args, metadatas):
  for m in metadatas:
    if args.verbose:
      print(m)
    else:
      print(f'//{m["dir"]}:', ', '.join(m['License File']))


def GenerateCredits(args, metadatas):
  """Generate about:credits."""
  def EvaluateTemplate(template, env, escape=True):
    """Expand a template with variables like {{foo}} using a
        dictionary of expansions."""
    for key, val in env.items():
      if escape:
        val = html.escape(val)
      template = template.replace('{{%s}}' % key, val)
    return template

  def MetadataToTemplateEntry(metadata, entry_template):
    license_paths = metadata['License File']
    license_content = '\n\n'.join(
        _read_file(os.path.join(args.scan_root, f)) for f in license_paths)

    env = {
        'name': metadata['Name'],
        'url': metadata['URL'],
        'license': license_content,
    }

    return {
        'name': metadata['Name'],
        'content': EvaluateTemplate(entry_template, env),
        'license_file': metadata['License File'],
    }

  entry_template = pathlib.Path(args.entry_template).read_text(encoding='utf-8')
  reciprocal_template = pathlib.Path(
      args.reciprocal_template).read_text('utf-8')
  file_template = pathlib.Path(args.file_template).read_text('utf-8')

  entries = []
  entries_by_name = {}
  for metadata in metadatas:
    new_entry = MetadataToTemplateEntry(metadata, entry_template)
    # Skip entries that we've already seen (it exists in multiple
    # directories).
    prev_entry = entries_by_name.setdefault(new_entry['name'].lower(), new_entry)
    if prev_entry is not new_entry and (prev_entry['content'].lower()
                                        == new_entry['content'].lower()):
      continue

    entries.append(new_entry)

  entries.sort(key=lambda entry: (entry['name'].lower(), entry['content']))
  for entry_id, entry in enumerate(entries):
    entry['content'] = entry['content'].replace('{{id}}', str(entry_id))

  entries_contents = '\n'.join([entry['content'] for entry in entries])

  reciprocal_contents = EvaluateTemplate(reciprocal_template, {
      'opensource_project': 'Chromium',
      'opensource_link': 'https://source.chromium.org/chromium'
  },
                                         escape=False)

  template_contents = "<!-- Generated by licenses.py; do not edit. -->"
  template_contents += EvaluateTemplate(file_template, {
      'entries': entries_contents,
      'reciprocal-license-statement': reciprocal_contents
  },
                                        escape=False)

  _WriteIfChanged(args.output_file, template_contents)


def GenerateDepfile(depfile, output_file):
  """Writes a depfile listing all files read via _read_file()."""
  paths = sorted(os.path.relpath(p) for p in _read_paths)

  # Add in build.ninja so that the target will be considered dirty whenever
  # gn gen is run. Otherwise, it will fail to notice new files being added.
  # This is still not perfect, as it will fail if no build files are changed,
  # but a new README.chromium / LICENSE is added. This shouldn't happen in
  # practice however.
  paths.append('build.ninja')

  action_helpers.write_depfile(depfile, os.path.relpath(output_file), paths)


def GenerateLicenseFile(args, metadatas):
  """Top level function for the license file generation.

  Either saves the text to a file or prints it to stdout depending on if
    args.output_file is set.
  """
  if args.format == 'spdx':
    license_txt = GenerateLicenseFileSpdx(metadatas, args.spdx_link,
                                          args.spdx_root, args.scan_root,
                                          args.spdx_doc_name,
                                          args.spdx_doc_namespace)
  elif args.format == 'txt':
    license_txt = GenerateLicenseFilePlainText(metadatas, args.scan_root)
  elif args.format == 'csv':
    license_txt = GenerateLicenseFileCsv(metadatas)
  elif args.format == 'notice':
    license_txt = GenerateNoticeFilePlainText(metadatas, args.scan_root)
  else:
    raise ValueError(f'Unknown license format: {args.format}')

  pathlib.Path(args.output_file).write_text(license_txt, 'utf-8')


def GenerateLicenseFileCsv(metadatas: List[Dict[str, Any]], ) -> str:
  """Generates a CSV formatted file which contains license data to be used as
    part of the submission to the Open Source Licensing Review process.
  """
  csv_content = io.StringIO()
  csv_writer = csv.writer(csv_content,
                          quoting=csv.QUOTE_NONNUMERIC,
                          lineterminator='\n')

  # These values are applied statically to all dependencies which are included
  # in the exported CSV.
  # Static fields:
  #   * Name of binary which uses dependency,
  #   * License text for library included in product,
  #   * Mirrored source for reciprocal licences.
  #   * Signoff date.
  static_data = ["Chromium", "Yes", "Yes", "N/A"]

  # Add informative CSV header row to make it clear which columns represent
  # which data in the review spreadsheet.
  csv_writer.writerow([
      "Library Name", "Link to LICENSE file", "License Name",
      "Binary which uses library", "License text for library included?",
      "Source code for library includes the mirrored source?",
      "Authorization date"
  ])

  for m in metadatas:
    data_row = [m["Name"]]

    urls = []
    for f in m["License File"]:
      # The review process requires that a link is provided to each license
      # which is included. We can achieve this by combining a static
      # Chromium googlesource URL with the relative path to the license
      # file from the top level Chromium src directory.
      lic_url = f"https://source.chromium.org/chromium/chromium/src/+/main:{f}"

      # Since these are URLs and not paths, replace any Windows path `\`
      # separators with a `/`
      urls.append(lic_url.replace("\\", "/"))

    data_row.append(", ".join(urls) or "UNKNOWN")
    data_row.append(m["License"] or "UNKNOWN")

    # Join the default data which applies to each row
    csv_writer.writerow(data_row + static_data)

  return csv_content.getvalue()


def GenerateNoticeFilePlainText(metadatas: List[Dict[str, Any]],
                                scan_root: str,
                                read_file=_read_file) -> str:
  """Generate a plain-text THIRD_PARTY_NOTICE file which can be used when you
    ship a part of Chromium code (specified by gn_target) as a stand-alone
    library (e.g., //ios/web_view).

    The THIRD_PARTY_NOTICE file contains licenses of third-party libraries
    which gn_target depends on. Each notice contains the following information:
    * The name of the component.
    * Identification of the component's license.
    * The complete text of every unique license (at least once)
      """
  # Start with Chromium's THIRD_PARTY_NOTICE file.
  content = []
  # Add necessary third_party.
  for m in metadatas:
    for license_file in m['License File']:
      content.append('-' * 20)
      content.append('License notice for %s' % m["Name"])
      content.append('-' * 20)
      content.append(read_file(os.path.join(scan_root, license_file)))

  return '\n'.join(content)


def GenerateLicenseFilePlainText(metadatas: List[Dict[str, Any]],
                                 scan_root: str,
                                 read_file=_read_file) -> str:
  """Generate a plain-text LICENSE file which can be used when you ship a part
    of Chromium code (specified by gn_target) as a stand-alone library
    (e.g., //ios/web_view).

    The LICENSE file contains licenses of both Chromium and third-party
    libraries which gn_target depends on. """
  content = []
  for m in metadatas:
    license_files = m['License File']
    if license_files:
      content.append('-' * 20)
      content.append(m['Name'])
      content.append('-' * 20)
      for license_file in license_files:
        content.append(read_file(os.path.join(scan_root, license_file)))

  return '\n'.join(content)


def GenerateLicenseFileSpdx(metadatas: List[Dict[str, Any]],
                            spdx_link_prefix: str,
                            spdx_root: str,
                            scan_root: str,
                            doc_name: Optional[str],
                            doc_namespace: Optional[str],
                            read_file=_read_file) -> str:
  """Generates a LICENSE file in SPDX format.

  The SPDX output contains the following elements:

  1. SPDX Document.
  2. SPDX root Package.
  3. An SPDX Package for each package that has license files.
  4. An SPDX License element for each license file.

  The output is based on the following specification:
  https://spdx.github.io/spdx-spec/v2-draft/
  """
  assert metadatas[0] is _CHROMIUM_LICENSE_METADATA
  root_license = os.path.join(scan_root, metadatas[0]['License File'][0])
  spdx_writer = SpdxWriter(
      spdx_root,
      'Chromium',  # Rather than "The Chromium Project"
      root_license,
      spdx_link_prefix,
      doc_name=doc_name,
      doc_namespace=doc_namespace,
      read_file=read_file)

  # Add all third party libraries
  for m in metadatas[1:]:
    for license_file in m['License File']:
      license_path = os.path.join(scan_root, license_file)
      spdx_writer.add_package(m['Name'], license_path)

  return spdx_writer.write()


def _AddCommonArgs(parser):
  parser.add_argument(
      '--extra-third-party-dirs',
      help='Gn list of additional third_party dirs to look through.')
  parser.add_argument(
      '--extra-allowed-dirs',
      help=('Gn list of extra allowed paths to look for '
            '(deps that will be picked up automatically) besides third_party'))
  parser.add_argument(
      '--exclude-dirs',
      help='Gn list of directories that should be excluded from the output.')
  parser.add_argument('--scan-root',
                      default=_REPOSITORY_ROOT,
                      help='Directory to scan for licenses')
  parser.add_argument('--target-os', help='OS that this build is targeting.')
  parser.add_argument('--gn-out-dir',
                      help='GN output directory for scanning dependencies.')
  parser.add_argument('--gn-target', help='GN target to scan for dependencies.')
  parser.add_argument(
      '--enable-warnings',
      action='store_true',
      help='Enables warning logs when processing directory metadata for '
      'credits or license file generation.')
  action_helpers.add_depfile_arg(parser)


def main():
  parser = argparse.ArgumentParser()
  sub_parsers = parser.add_subparsers(dest='command', required=True)
  sub_parser = sub_parsers.add_parser('scan',
                                      help='Scan for problems with metadata.')
  _AddCommonArgs(sub_parser)
  sub_parser.set_defaults(enable_warnings=True)
  sub_parser.set_defaults(shipped_only=False)

  sub_parser = sub_parsers.add_parser('list', help='List all license paths')
  _AddCommonArgs(sub_parser)
  sub_parser.add_argument('--shipped-only',
                          action='store_true',
                          help='List only shipped licenses')
  sub_parser.add_argument('--verbose',
                          action='store_true',
                          help='Print all metadata')

  sub_parser = sub_parsers.add_parser('credits',
                                      help='Create the about://credits HTML.')
  _AddCommonArgs(sub_parser)
  sub_parser.set_defaults(shipped_only=True)
  sub_parser.add_argument('--file-template',
                          default=_DEFAULT_FILE_TEMPLATE_FILE,
                          help='Template HTML to use for the license page.')
  sub_parser.add_argument('--entry-template',
                          default=_DEFAULT_ENTRY_TEMPLATE_FILE,
                          help='Template HTML to use for each license.')
  sub_parser.add_argument(
      '--reciprocal-template',
      default=_DEFAULT_RECIPROCAL_TEMPLATE_FILE,
      help=('Template HTML to use for adding a link to an open '
            'source code repository to satisfy reciprocal '
            'license requirements. eg Chromium.'))
  sub_parser.add_argument('output_file')

  sub_parser = sub_parsers.add_parser(
      'license_file', help='Generate a LICENSE file in the given format.')
  _AddCommonArgs(sub_parser)
  sub_parser.set_defaults(shipped_only=True)
  sub_parser.add_argument('--format',
                          default='txt',
                          choices=['txt', 'spdx', 'csv', 'notice'],
                          help='What format to output in')
  sub_parser.add_argument('--spdx-root',
                          default=_REPOSITORY_ROOT,
                          help=('Use a different root for license refs than ' +
                                _REPOSITORY_ROOT))
  sub_parser.add_argument(
      '--spdx-link',
      default='https://source.chromium.org/chromium/chromium/src/+/main:',
      help='Link prefix for license cross ref')
  sub_parser.add_argument('--spdx-doc-name',
                          help='Specify a document name for the SPDX doc')
  sub_parser.add_argument(
      '--spdx-doc-namespace',
      default='https://chromium.googlesource.com/chromium/src/tools/',
      help='Specify the document namespace for the SPDX doc')
  sub_parser.add_argument('output_file')

  args = parser.parse_args()
  args.extra_third_party_dirs = action_helpers.parse_gn_list(
      args.extra_third_party_dirs)
  args.extra_allowed_dirs = action_helpers.parse_gn_list(
      args.extra_allowed_dirs)
  args.exclude_dirs = action_helpers.parse_gn_list(args.exclude_dirs)
  if (not args.exclude_dirs) and args.target_os == 'ios':
    args.exclude_dirs = list(KNOWN_NON_IOS_LIBRARIES)

  for p in (args.extra_third_party_dirs + args.extra_allowed_dirs +
            args.exclude_dirs):
    if os.path.isabs(p):
      parser.error('Expected paths to be relative to scan root. Found: ' + p)

  metadatas, had_errors = _DiscoverMetadatas(args)

  if args.command == 'scan':
    return 1 if had_errors else 0
  elif args.command == 'list':
    _ListLicenses(args, metadatas)
  elif args.command == 'credits':
    GenerateCredits(args, metadatas)
  elif args.command == 'license_file':
    GenerateLicenseFile(args, metadatas)

  if args.depfile:
    GenerateDepfile(args.depfile, args.output_file)

  return 0


if __name__ == '__main__':
  sys.exit(main())
