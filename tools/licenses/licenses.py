#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility for checking and processing licensing information in third_party
directories.

Usage: licenses.py <command>

Commands:
  scan     scan third_party directories, verifying that we have licensing info
  credits  generate about:credits on stdout

(You can also import this as a module.)
"""
from __future__ import print_function

import argparse
import codecs
import csv
import io
import json
import logging
import os
import pathlib
import shutil
import re
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional

if sys.version_info.major == 2:
  import cgi as html
else:
  import html

from spdx_writer import SpdxWriter

_REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(_REPOSITORY_ROOT, 'build'))
import action_helpers

METADATA_FILE_NAMES = frozenset({
    "README.chromium", "README.crashpad", "README.v8", "README.pdfium",
    "README.angle"
})

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

    # Not actually a third party dependency. Supplies configuration for
    # enabling or disabling field trials and features in Chromium projects.
    os.path.join('third_party', 'chromium-variations'),

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
    os.path.join('third_party', 'llvm-build'),
    os.path.join('third_party', 'mingw-w64'),
    os.path.join('third_party', 'nacl_sdk_binaries'),
    os.path.join('third_party', 'pefile'),
    os.path.join('third_party', 'perl'),
    os.path.join('third_party', 'psyco_win32'),
    os.path.join('third_party', 'pyelftools'),
    os.path.join('third_party', 'pylib'),
    os.path.join('third_party', 'pywebsocket'),
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
    # Chrome for Android proprietary code.
    os.path.join('clank'),

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
#   - Third party directories in //clank which are considered to be Google-owned
#   - Directories that are directly checked out from upstream, and thus
#     don't have a README.chromium
#   - Directories that contain example code, or build tooling.
#   - Nested third_party code inside other third_party libraries.
ADDITIONAL_PATHS = (
    os.path.join('chrome', 'test', 'chromeos', 'autotest'),
    os.path.join('chrome', 'test', 'data'),
    os.path.join('native_client'),
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
    os.path.join('chrome', 'test', 'data', 'third_party', 'kraken'),
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
    os.path.join('third_party', 'iccjpeg'),
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
    os.path.join('third_party', 'qcms'),
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


class InvalidMetadata(Exception):
  """This exception is raised when metadata is invalid."""
  pass


class LicenseError(Exception):
  """We raise this exception when a dependency's licensing info isn't
  fully filled out.
  """
  pass


def AbsolutePath(path, filename, root):
  """Convert a path in README.chromium to be absolute based on the source
    root."""
  if filename.startswith('/'):
    # Absolute-looking paths are relative to the source root
    # (which is the directory we're run from).
    absolute_path = os.path.join(root, os.path.normpath(filename.lstrip('/')))
  else:
    absolute_path = os.path.join(root, path, os.path.normpath(filename))
  if os.path.exists(absolute_path):
    return absolute_path
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
  with codecs.open(filepath, encoding="utf-8") as readme:
    for line in readme:
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

    license_path = AbsolutePath(path, file_path, root)
    # Check that the license file exists.
    if license_path is not None:
      license_paths.append(license_path)

  # If there are no license files at all, check for the COPYING license file.
  if not license_paths:
    license_path = AbsolutePath(path, "COPYING", root)
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
  with codecs.open(additional_paths_file, encoding='utf-8') as paths_file:
    additional_paths = json.load(paths_file)
  for p in additional_paths:
    subpath = os.path.normpath(os.path.join(dirpath, p))
    _MaybeAddThirdPartyPath(root, subpath, third_party_dirs)


def FindThirdPartyDirs(root, extra_third_party_dirs=None):
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
  exe = 'gn'
  if sys.platform.startswith('linux'):
    subdir = 'linux64'
  elif sys.platform == 'darwin':
    subdir = 'mac'
  elif sys.platform == 'win32':
    subdir, exe = 'win', 'gn.exe'
  else:
    raise RuntimeError("Unsupported platform '%s'." % sys.platform)

  return os.path.join(_REPOSITORY_ROOT, 'buildtools', subdir, exe)


def LogParseDirErrors(errors):
  """Provides a convenience method for printing out the errors resulting
  from running ParseDir() over a directory."""

  for error in sorted(errors):
    print(error)


def GetThirdPartyDepsFromGNDepsOutput(
    gn_deps: str,
    target_os: str,
    extra_allowed_dirs: Optional[List[str]] = None):
  """Returns third_party/foo directories given the output of "gn desc deps".

    Note that it always returns the direct sub-directory of third_party
    where README.chromium and LICENSE files are, so that it can be passed to
    ParseDir(). e.g.:
        third_party/cld_3/src/src/BUILD.gn -> third_party/cld_3/
    Rust dependencies are a special case, with a deeper structure:
        third_party/rust/foo/v1/crate/BUILD.gn -> third_party/rust/foo/v1/

    It returns relative paths from _REPOSITORY_ROOT, not absolute paths.
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
  for absolute_build_dep in gn_deps.split():
    relative_build_dep = os.path.relpath(absolute_build_dep, _REPOSITORY_ROOT)
    m = path_regex.search(relative_build_dep)
    if not m:
      continue
    third_party_path = m.group(1)
    if any(third_party_path.startswith(p + os.sep) for p in PRUNE_PATHS):
      continue
    if (target_os == 'ios' and any(
        third_party_path.startswith(p + os.sep)
        for p in KNOWN_NON_IOS_LIBRARIES)):
      # Skip over files that are known not to be used on iOS.
      continue
    third_party_deps.add(third_party_path[:-1])
  return third_party_deps


def FindThirdPartyDeps(gn_out_dir: str,
                       gn_target: str,
                       target_os: str,
                       extra_third_party_dirs: Optional[List[str]] = None,
                       extra_allowed_dirs: Optional[List[str]] = None):
  if not gn_out_dir:
    raise RuntimeError("--gn-out-dir is required if --gn-target is used.")

  # Generate gn project in temp directory and use it to find dependencies.
  # Current gn directory cannot be used when we run this script in a gn action
  # rule, because gn doesn't allow recursive invocations due to potential side
  # effects.
  try:
    with tempfile.TemporaryDirectory(dir=gn_out_dir) as tmp_dir:
      shutil.copy(os.path.join(gn_out_dir, "args.gn"), tmp_dir)
      subprocess.check_output(
          [_GnBinary(), "gen",
           "--root=%s" % _REPOSITORY_ROOT, tmp_dir])
      gn_deps = subprocess.check_output([
          _GnBinary(), "desc",
          "--root=%s" % _REPOSITORY_ROOT, tmp_dir, gn_target, "deps",
          "--as=buildfile", "--all"
      ])
      if isinstance(gn_deps, bytes):
        gn_deps = gn_deps.decode("utf-8")
  except:
    if sys.platform == 'win32':
      print("""
      ##########################################################################

      This is a known issue; please report the failure to
      https://crbug.com/1208393.

      ##########################################################################
      """)
      subprocess.check_call(['tasklist.exe'])
    raise

  third_party_deps = GetThirdPartyDepsFromGNDepsOutput(gn_deps, target_os,
                                                       extra_allowed_dirs)
  if extra_third_party_dirs:
    third_party_deps.update(extra_third_party_dirs)
  return sorted(third_party_deps)


def ScanThirdPartyDirs(root=None):
  """Scan a list of directories and report on any problems we find."""
  if root is None:
    root = os.getcwd()
  third_party_dirs = FindThirdPartyDirs(root)

  errors = []
  for path in sorted(third_party_dirs):
    try:
      _, errors = ParseDir(path, root, enable_warnings=True)
    except LicenseError as e:
      errors.append(f"{path}: {e}")
      continue

    LogParseDirErrors(errors)

  return len(errors) == 0


def GenerateCredits(file_template_file,
                    entry_template_file,
                    reciprocal_template_file,
                    output_file,
                    target_os,
                    gn_out_dir,
                    gn_target,
                    extra_third_party_dirs=None,
                    depfile=None,
                    enable_warnings=False):
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
    licenses = []
    for filepath in metadata['License File']:
      licenses.append(
          codecs.open(filepath, errors="replace", encoding='utf-8').read())
    license_content = '\n\n'.join(licenses)

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

  if gn_target:
    third_party_dirs = FindThirdPartyDeps(gn_out_dir, gn_target, target_os,
                                          extra_third_party_dirs)

    # Sanity-check to raise a build error if invalid gn_... settings are
    # somehow passed to this script.
    if not third_party_dirs:
      raise RuntimeError("No deps found.")
  else:
    third_party_dirs = FindThirdPartyDirs(_REPOSITORY_ROOT,
                                          extra_third_party_dirs)

  if not file_template_file:
    file_template_file = os.path.join(_REPOSITORY_ROOT, 'components',
                                      'about_ui', 'resources',
                                      'about_credits.tmpl')
  if not entry_template_file:
    entry_template_file = os.path.join(_REPOSITORY_ROOT, 'components',
                                       'about_ui', 'resources',
                                       'about_credits_entry.tmpl')

  # Used to add a link at the top of credits for Chromium code to
  # satisfy the requirements for reciprocal license types.
  if not reciprocal_template_file:
    reciprocal_template_file = os.path.join(_REPOSITORY_ROOT, 'components',
                                            'about_ui', 'resources',
                                            'about_credits_reciprocal.tmpl')

  entry_template = codecs.open(entry_template_file, encoding='utf-8').read()
  entries = []
  # Start from Chromium's LICENSE file
  chromium_license_metadata = {
      'Name': 'The Chromium Project',
      'URL': 'https://www.chromium.org',
      'Shipped': 'yes',
      'License File': [os.path.join(_REPOSITORY_ROOT, 'LICENSE')],
  }
  entries.append(
      MetadataToTemplateEntry(chromium_license_metadata, entry_template))

  entries_by_name = {}
  for path in third_party_dirs:
    try:
      # Directory metadata can be for multiple dependencies.
      directory_metadata, _ = ParseDir(path,
                                       _REPOSITORY_ROOT,
                                       enable_warnings=enable_warnings)
      if not directory_metadata:
        continue
    except LicenseError:
      # TODO(phajdan.jr): Convert to fatal error (https://crbug.com/39240).
      continue

    for dep_metadata in directory_metadata:
      if dep_metadata['Shipped'] == NO:
        continue
      if target_os == 'ios' and not gn_target:
        # Skip over files that are known not to be used on iOS. But
        # skipping is unnecessary if GN was used to query the actual
        # dependencies.
        # TODO(lambroslambrou): Remove this step once the iOS build is
        # updated to provide --gn-target to this script.
        if path in KNOWN_NON_IOS_LIBRARIES:
          continue

      new_entry = MetadataToTemplateEntry(dep_metadata, entry_template)
      # Skip entries that we've already seen (it exists in multiple
      # directories).
      prev_entry = entries_by_name.setdefault(new_entry['name'], new_entry)
      if prev_entry is not new_entry and (prev_entry['content']
                                          == new_entry['content']):
        continue

      entries.append(new_entry)

  entries.sort(key=lambda entry: (entry['name'].lower(), entry['content']))
  for entry_id, entry in enumerate(entries):
    entry['content'] = entry['content'].replace('{{id}}', str(entry_id))

  entries_contents = '\n'.join([entry['content'] for entry in entries])

  reciprocal_template = codecs.open(reciprocal_template_file,
                                    encoding='utf-8').read()
  reciprocal_contents = EvaluateTemplate(reciprocal_template, {
      'opensource_project': 'Chromium',
      'opensource_link': 'https://source.chromium.org/chromium'
  },
                                         escape=False)

  file_template = codecs.open(file_template_file, encoding='utf-8').read()
  template_contents = "<!-- Generated by licenses.py; do not edit. -->"
  template_contents += EvaluateTemplate(file_template, {
      'entries': entries_contents,
      'reciprocal-license-statement': reciprocal_contents
  },
                                        escape=False)

  if output_file:
    changed = True
    try:
      old_output = codecs.open(output_file, 'r', encoding='utf-8').read()
      if old_output == template_contents:
        changed = False
    except:
      pass
    if changed:
      with codecs.open(output_file, 'w', encoding='utf-8') as output:
        output.write(template_contents)
  else:
    print(template_contents)

  if depfile:
    assert output_file
    # Add in build.ninja so that the target will be considered dirty whenever
    # gn gen is run. Otherwise, it will fail to notice new files being added.
    # This is still no perfect, as it will fail if no build files are changed,
    # but a new README.chromium / LICENSE is added. This shouldn't happen in
    # practice however.
    license_file_list = []
    for entry in entries:
      license_file_list.extend(entry['license_file'])
    license_file_list = (os.path.relpath(p) for p in license_file_list)
    license_file_list = sorted(set(license_file_list))
    action_helpers.write_depfile(depfile, output_file,
                                 license_file_list + ['build.ninja'])

  return True


def GenerateLicenseFile(args: argparse.Namespace, root_dir=_REPOSITORY_ROOT):
  """Top level function for the license file generation.

  Either saves the text to a file or prints it to stdout depending on if
    args.output_file is set.

  Args:
    args: parsed arguments passed from the cli
    root_dir: The root directory to start the third party search
  """
  # Convert the path separators to the OS specific ones
  extra_third_party_dirs = args.extra_third_party_dirs
  if extra_third_party_dirs is not None:
    extra_third_party_dirs = [
        os.path.normpath(path) for path in extra_third_party_dirs
    ]

  if args.gn_target is not None:
    third_party_dirs = FindThirdPartyDeps(args.gn_out_dir, args.gn_target,
                                          args.target_os,
                                          extra_third_party_dirs,
                                          args.extra_allowed_dirs)

    # Sanity-check to raise a build error if invalid gn_... settings are
    # somehow passed to this script.
    if not third_party_dirs:
      raise RuntimeError("No deps found.")

  else:
    third_party_dirs = FindThirdPartyDirs(root_dir, extra_third_party_dirs)

  metadatas = {}
  for d in third_party_dirs:
    try:
      directory_metadata, errors = ParseDir(
          d,
          root_dir,
          require_license_file=True,
          enable_warnings=args.enable_warnings)
      if directory_metadata:
        metadatas[d] = directory_metadata
    except LicenseError as lic_exp:
      # TODO(phajdan.jr): Convert to fatal error (https://crbug.com/39240).
      if args.enable_warnings:
        print(f"Error: {lic_exp}")
      continue

    if args.enable_warnings:
      LogParseDirErrors(errors)

  if args.format == 'spdx':
    license_txt = GenerateLicenseFileSpdx(metadatas, args.spdx_link,
                                          args.spdx_root, args.spdx_doc_name,
                                          args.spdx_doc_namespace)
  elif args.format == 'txt':
    license_txt = GenerateLicenseFilePlainText(metadatas)

  elif args.format == 'csv':
    license_txt = GenerateLicenseFileCsv(metadatas)

  elif args.format == 'notice':
    license_txt = GenerateNoticeFilePlainText(metadatas)

  else:
    raise ValueError(f'Unknown license format: {args.format}')

  if args.output_file:
    with open(args.output_file, 'w', encoding='utf-8') as f:
      f.write(license_txt)
  else:
    print(license_txt)


def GenerateLicenseFileCsv(
    metadata: Dict[str, List[Dict[str, Any]]],
    repo_root: str = _REPOSITORY_ROOT,
) -> str:
  """Generates a CSV formatted file which contains license data to be used as
    part of the submission to the Open Source Licensing Review process.
  """
  third_party_data = []

  # Collect all the metadata we want to write out and sort it so that the
  # resulting CSV is ordered by dependency name
  for data in metadata.values():
    third_party_data.extend(data)
  third_party_data.sort(key=lambda item: item['Name'])

  csv_content = io.StringIO()
  csv_writer = csv.writer(csv_content, quoting=csv.QUOTE_NONNUMERIC)

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

  # Include Chromium at the top of the file as it's the main artifact.
  csv_writer.writerow([
      "Chromium",
      "https://source.chromium.org/chromium/chromium/src/+/main:LICENSE",
      "BSD 3-Clause"
  ] + static_data)

  for dep_metadata in third_party_data:
    # Only third party libraries which are shipped as part of a final
    # product are in scope for license review.
    if dep_metadata['Shipped'] == NO:
      continue

    data_row = [dep_metadata['Name'] or "UNKNOWN"]

    urls = []
    for lic in dep_metadata['License File']:
      # The review process requires that a link is provided to each license
      # which is included. We can achieve this by combining a static
      # Chromium googlesource URL with the relative path to the license
      # file from the top level Chromium src directory.
      lic_url = ("https://source.chromium.org/chromium/chromium/src/+/main:" +
                 os.path.relpath(lic, repo_root))

      # Since these are URLs and not paths, replace any Windows path `\`
      # separators with a `/`
      urls.append(lic_url.replace("\\", "/"))

    data_row.append(", ".join(urls) or "UNKNOWN")
    data_row.append(dep_metadata["License"] or "UNKNOWN")

    # Join the default data which applies to each row
    csv_writer.writerow(data_row + static_data)

  return csv_content.getvalue()


def GenerateNoticeFilePlainText(
    metadata: Dict[str, List[Dict[str, Any]]],
    repo_root: str = _REPOSITORY_ROOT,
    read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')
) -> str:
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
  for directory in sorted(metadata):
    dir_metadata = metadata[directory]
    for dep_metadata in dir_metadata:
      shipped = dep_metadata['Shipped']
      license_files = dep_metadata['License File']
      if shipped == YES and license_files:
        for license_file in license_files:
          content.append('-' * 20)
          content.append('License notice for %s' % dep_metadata["Name"])
          content.append('-' * 20)
          content.append(read_file(os.path.join(repo_root, license_file)))

  return '\n'.join(content)


def GenerateLicenseFilePlainText(
    metadata: Dict[str, List[Dict[str, Any]]],
    repo_root: str = _REPOSITORY_ROOT,
    read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')
) -> str:
  """Generate a plain-text LICENSE file which can be used when you ship a part
    of Chromium code (specified by gn_target) as a stand-alone library
    (e.g., //ios/web_view).

    The LICENSE file contains licenses of both Chromium and third-party
    libraries which gn_target depends on. """
  # Start with Chromium's LICENSE file.
  content = [read_file(os.path.join(repo_root, 'LICENSE'))]

  # Add necessary third_party.
  for directory in sorted(metadata):
    dir_metadata = metadata[directory]
    for dep_metadata in dir_metadata:
      shipped = dep_metadata['Shipped']
      license_files = dep_metadata['License File']
      if shipped == YES and license_files:
        content.append('-' * 20)
        content.append(dep_metadata["Name"])
        content.append('-' * 20)
        for license_file in license_files:
          content.append(read_file(os.path.join(repo_root, license_file)))

  return '\n'.join(content)


def GenerateLicenseFileSpdx(
    metadata: Dict[str, List[Dict[str, Any]]],
    spdx_link_prefix: str,
    spdx_root: str,
    doc_name: Optional[str],
    doc_namespace: Optional[str],
    repo_root: str = _REPOSITORY_ROOT,
    read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')
) -> str:
  """Generates a LICENSE file in SPDX format.

  The SPDX output contains the following elements:

  1. SPDX Document.
  2. SPDX root Package.
  3. An SPDX Package for each package that has license files.
  4. An SPDX License element for each license file.

  The output is based on the following specification:
  https://spdx.github.io/spdx-spec/v2-draft/
  """
  root_license = os.path.join(repo_root, 'LICENSE')
  spdx_writer = SpdxWriter(spdx_root,
                           'Chromium',
                           root_license,
                           spdx_link_prefix,
                           doc_name=doc_name,
                           doc_namespace=doc_namespace,
                           read_file=read_file)

  # Add all third party libraries
  for directory in sorted(metadata):
    dir_metadata = metadata[directory]
    for dep_metadata in dir_metadata:
      shipped = dep_metadata['Shipped']
      license_files = dep_metadata['License File']
      if shipped == YES and license_files:
        for license_file in license_files:
          license_path = os.path.join(repo_root, license_file)
          spdx_writer.add_package(dep_metadata['Name'], license_path)

  return spdx_writer.write()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--file-template',
                      help='Template HTML to use for the license page.')
  parser.add_argument('--entry-template',
                      help='Template HTML to use for each license.')
  parser.add_argument('--reciprocal-template',
                      help=('Template HTML to use for adding a link to an open '
                            'source code repository to satisfy reciprocal '
                            'license requirements. eg Chromium.'))
  parser.add_argument(
      '--extra-third-party-dirs',
      help='Gn list of additional third_party dirs to look through.')
  parser.add_argument(
      '--extra-allowed-dirs',
      help=('List of extra allowed paths to look for '
            '(deps that will be picked up automatically) besides third_party'),
  )
  parser.add_argument('--target-os', help='OS that this build is targeting.')
  parser.add_argument('--gn-out-dir',
                      help='GN output directory for scanning dependencies.')
  parser.add_argument('--gn-target', help='GN target to scan for dependencies.')
  parser.add_argument('--format',
                      default='txt',
                      choices=['txt', 'spdx', 'csv', 'notice'],
                      help='What format to output in')
  parser.add_argument('--spdx-root',
                      default=_REPOSITORY_ROOT,
                      help=('Use a different root for license refs than ' +
                            _REPOSITORY_ROOT))
  parser.add_argument(
      '--spdx-link',
      default='https://source.chromium.org/chromium/chromium/src/+/main:',
      help='Link prefix for license cross ref')
  parser.add_argument('--spdx-doc-name',
                      help='Specify a document name for the SPDX doc')
  parser.add_argument(
      '--spdx-doc-namespace',
      default='https://chromium.googlesource.com/chromium/src/tools/',
      help='Specify the document namespace for the SPDX doc')
  parser.add_argument(
      '--enable-warnings',
      action='store_true',
      help='Enables warning logs when processing directory metadata for '
      'credits or license file generation.')
  parser.add_argument('command',
                      choices=['help', 'scan', 'credits', 'license_file'])
  parser.add_argument('output_file', nargs='?')
  action_helpers.add_depfile_arg(parser)
  args = parser.parse_args()
  args.extra_third_party_dirs = action_helpers.parse_gn_list(
      args.extra_third_party_dirs)
  args.extra_allowed_dirs = action_helpers.parse_gn_list(
      args.extra_allowed_dirs)

  if args.command == 'scan':
    if not ScanThirdPartyDirs():
      return 1
  elif args.command == 'credits':
    if not GenerateCredits(
        args.file_template, args.entry_template, args.reciprocal_template,
        args.output_file, args.target_os, args.gn_out_dir, args.gn_target,
        args.extra_third_party_dirs, args.depfile, args.enable_warnings):
      return 1
  elif args.command == 'license_file':
    try:
      GenerateLicenseFile(args)
    except LicenseError as e:
      print("Failed to parse README.chromium: {}".format(e))
      return 1
  else:
    print(__doc__)
    return 1


if __name__ == '__main__':
  sys.exit(main())
