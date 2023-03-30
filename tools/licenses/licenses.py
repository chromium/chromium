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
import json
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

    # Used for development and test, not in the shipping product.
    os.path.join('build', 'secondary'),
    os.path.join('third_party', 'bison'),
    os.path.join('third_party', 'blanketjs'),
    os.path.join('third_party', 'chromite'),
    os.path.join('third_party', 'cygwin'),
    os.path.join('third_party', 'gles2_conform'),
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
    os.path.join('third_party', 'clear_cache'),
    os.path.join('third_party', 'gnu'),
    os.path.join('third_party', 'googlemac'),
    os.path.join('third_party', 'pcre'),
    os.path.join('third_party', 'psutils'),
    os.path.join('third_party', 'sawbuck'),
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
    os.path.join('testing', 'gmock'),
    os.path.join('testing', 'gtest'),
    os.path.join('third_party', 'boringssl', 'src', 'third_party', 'fiat'),
    os.path.join('third_party', 'devtools-frontend', 'src', 'front_end',
                 'third_party'),
    os.path.join('third_party', 'devtools-frontend-internal', 'front_end',
                 'third_party'),
    os.path.join('tools', 'gyp'),
    os.path.join('tools', 'page_cycler', 'acid3'),
    os.path.join('url', 'third_party', 'mozilla'),
    os.path.join('v8'),
    # Fake directories to include the strongtalk and fdlibm licenses.
    os.path.join('v8', 'strongtalk'),
    os.path.join('v8', 'fdlibm'),
)


# Directories where we check out directly from upstream, and therefore
# can't provide a README.chromium.  Please prefer a README.chromium
# wherever possible.
SPECIAL_CASES = {
    os.path.join('native_client'): {
        "Name": "native client",
        "URL": "http://code.google.com/p/nativeclient",
        "License": "BSD",
    },
    os.path.join('testing', 'gmock'): {
        "Name": "gmock",
        "URL": "http://code.google.com/p/googlemock",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('testing', 'gtest'): {
        "Name": "gtest",
        "URL": "http://code.google.com/p/googletest",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'angle'): {
        "Name": "Almost Native Graphics Layer Engine",
        "URL": "http://code.google.com/p/angleproject/",
        "License": "BSD",
    },
    os.path.join('third_party', 'cros_system_api'): {
        "Name": "Chromium OS system API",
        "URL": "http://www.chromium.org/chromium-os",
        "License": "BSD",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/LICENSE.chromium_os",
    },
    os.path.join('third_party', 'ipcz'): {
        "Name": "ipcz",
        "URL": "https://chromium.googlesource.com/chromium/src/third_party/ipcz",
        "License": "BSD",
        "License File": "/third_party/ipcz/LICENSE",
    },
    os.path.join('third_party', 'lss'): {
        "Name": "linux-syscall-support",
        "URL": "http://code.google.com/p/linux-syscall-support/",
        "License": "BSD",
        "License File": "/LICENSE",
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party', 'abseil'): {
        "Name": "abseil",
        "URL": "https://github.com/abseil/abseil-cpp/",
        "License": "Apache 2.0",
        "License File": "/third_party/abseil-cpp/LICENSE",
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party',
                 'boringssl'):
    {
        "Name": "BoringSSL",
        "URL": "https://boringssl.googlesource.com/boringssl/",
        "License": "BSDish",
        "License File": "/third_party/boringssl/src/LICENSE",
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party', 'jsoncpp'):
    {
        "Name": "jsoncpp",
        "URL": "https://github.com/open-source-parsers/jsoncpp",
        "License": "MIT",
        "License File": "/third_party/jsoncpp/LICENSE",
    },
    os.path.join('third_party', 'openscreen', 'src', 'third_party', 'mozilla'):
    {
        "Name": "mozilla",
        "URL": "https://github.com/mozilla",
        "License": "MPL 1.1/GPL 2.0/LGPL 2.1",
        "License File": "LICENSE.txt",
    },
    os.path.join('third_party', 'pdfium'): {
        "Name": "PDFium",
        "URL": "http://code.google.com/p/pdfium/",
        "License": "BSD",
    },
    os.path.join('third_party', 'ppapi'): {
        "Name": "ppapi",
        "URL": "http://code.google.com/p/ppapi/",
    },
    os.path.join('third_party', 'scons-2.0.1'): {
        "Name": "scons-2.0.1",
        "URL": "http://www.scons.org",
        "License": "MIT",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'catapult'): {
        "Name": "catapult",
        "URL": "https://github.com/catapult-project/catapult",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party', 'getopt'): {
        "Name": "getopt",
        "URL": "https://sourceware.org/ml/newlib/2005/msg00758.html",
        "License": "Public domain",
        "License File": "/third_party/crashpad/crashpad/third_party/getopt/LICENSE",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party', 'lss'): {
        "Name": "linux-syscall-support",
        "URL": "https://chromium.googlesource.com/linux-syscall-support/",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party',
                 'mini_chromium'): {
        "Name": "mini_chromium",
        "URL": "https://chromium.googlesource.com/chromium/mini_chromium/",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party', 'xnu'): {
        "Name": "xnu",
        "URL": "https://opensource.apple.com/source/xnu/",
        "License": "Apple Public Source License 2.0",
        "License File": "APPLE_LICENSE",
    },
    os.path.join('third_party', 'crashpad', 'crashpad', 'third_party', 'zlib'):
    {
        "Name": "zlib",
        "URL": "https://zlib.net/",
        "License": "zlib",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'v8-i18n'): {
        "Name": "Internationalization Library for v8",
        "URL": "http://code.google.com/p/v8-i18n/",
        "License": "Apache 2.0",
    },
    os.path.join('third_party', 'blink'): {
        # about:credits doesn't show "Blink" but "WebKit".
        # Blink is a fork of WebKit, and Chromium project has maintained it
        # since the fork.  about:credits needs to mention the code before
        # the fork.
        "Name": "WebKit",
        "URL": "http://webkit.org/",
        "License": "BSD and LGPL v2 and LGPL v2.1",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/third_party/blink/LICENSE_FOR_ABOUT_CREDITS",
    },
    os.path.join('third_party', 'webpagereplay'): {
        "Name": "webpagereplay",
        "URL": "http://code.google.com/p/web-page-replay",
        "License": "Apache 2.0",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('tools', 'gyp'): {
        "Name": "gyp",
        "URL": "http://code.google.com/p/gyp",
        "License": "BSD",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('v8'): {
        "Name": "V8 JavaScript Engine",
        "URL": "http://code.google.com/p/v8",
        "License": "BSD",
    },
    os.path.join('v8', 'strongtalk'): {
        "Name": "Strongtalk",
        "URL": "http://www.strongtalk.org/",
        "License": "BSD",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/v8/LICENSE.strongtalk",
    },
    os.path.join('v8', 'fdlibm'): {
        "Name": "fdlibm",
        "URL": "http://www.netlib.org/fdlibm/",
        "License": "Freely Distributable",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/v8/LICENSE.fdlibm",
        "License Android Compatible": "yes",
    },
    os.path.join('third_party', 'khronos_glcts'): {
        # These sources are not shipped, are not public, and it isn't
        # clear why they're tripping the license check.
        "Name": "khronos_glcts",
        "URL": "http://no-public-url",
        "License": "Khronos",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('tools', 'telemetry', 'third_party', 'gsutil'): {
        "Name": "gsutil",
        "URL": "https://cloud.google.com/storage/docs/gsutil",
        "License": "Apache 2.0",
        "License File": "NOT_SHIPPED",
    },
    os.path.join('third_party', 'swiftshader'): {
        "Name": "SwiftShader",
        "URL": "https://swiftshader.googlesource.com/SwiftShader",
        "License": "Apache 2.0 and compatible licenses",
        "License Android Compatible": "yes",
        "License File": "/third_party/swiftshader/LICENSE.txt",
    },
    os.path.join('third_party', 'swiftshader', 'third_party', 'SPIRV-Tools'): {
        "Name": "SPIRV-Tools",
        "URL": "https://github.com/KhronosGroup/SPIRV-Tools",
        "License": "Apache 2.0",
        "License File":
        "/third_party/swiftshader/third_party/SPIRV-Tools/LICENSE",
    },
    os.path.join('third_party', 'swiftshader', 'third_party', 'SPIRV-Headers'): {
        "Name": "SPIRV-Headers",
        "URL": "https://github.com/KhronosGroup/SPIRV-Headers",
        "License": "Apache 2.0",
        "License File":
        "/third_party/swiftshader/third_party/SPIRV-Headers/LICENSE",
    },
    os.path.join('third_party', 'dawn', 'third_party', 'khronos'): {
        "Name": "khronos_platform",
        "URL": "http://www.khronos.org/registry/egl",
        "License": "Apache 2.0",
        "License File": "/third_party/dawn/third_party/khronos/LICENSE",
    },
    # Dependencies of Selenium Atoms
    os.path.join('third_party', 'selenium-atoms', 'sizzle'): {
        "Name": "Sizzle",
        "URL": "http://sizzlejs.com/",
        "License": "MIT, BSD and GPL v2",
        "License File": "/third_party/selenium-atoms/LICENSE.sizzle",
    },
    os.path.join('third_party', 'selenium-atoms', 'wgxpath'): {
        "Name": "Wicked Good XPath",
        "URL": "https://github.com/google/wicked-good-xpath",
        "License": "MIT",
        "License File": "/third_party/selenium-atoms/LICENSE.wgxpath",
    },
    os.path.join('third_party', 'selenium-atoms', 'closure-lib'): {
        "Name": "Closure Library",
        "URL": "https://developers.google.com/closure/library",
        "License": "Apache 2.0",
        "License File": "/third_party/selenium-atoms/LICENSE.closure",
    },
}

# Special value for 'License File' field used to indicate that the license file
# should not be used in about:credits.
NOT_SHIPPED = "NOT_SHIPPED"

# Paths for libraries that we have checked are not shipped on iOS. These are
# left out of the licenses file primarily because we don't want to cause a
# firedrill due to someone thinking that Chrome for iOS is using LGPL code
# when it isn't.
# This is a temporary hack; the real solution is crbug.com/178215
KNOWN_NON_IOS_LIBRARIES = set([
    os.path.join('base', 'third_party', 'symbolize'),
    os.path.join('base', 'third_party', 'xdg_mime'),
    os.path.join('base', 'third_party', 'xdg_user_dirs'),
    os.path.join('buildtools', 'third_party', 'libc++'),
    os.path.join('buildtools', 'third_party', 'libc++abi'),
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
    os.path.join('third_party', 'libevent'),
    os.path.join('third_party', 'libjpeg'),
    os.path.join('third_party', 'libusb'),
    os.path.join('third_party', 'libxslt'),
    os.path.join('third_party', 'lss'),
    os.path.join('third_party', 'lzma_sdk'),
    os.path.join('third_party', 'mesa'),
    os.path.join('third_party', 'motemplate'),
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
    os.path.join('third_party', 'sudden_motion_sensor'),
    os.path.join('third_party', 'swiftshader'),
    os.path.join('third_party', 'swig'),
    os.path.join('third_party', 'talloc'),
    os.path.join('third_party', 'usb_ids'),
    os.path.join('third_party', 'v8-i18n'),
    os.path.join('third_party', 'wtl'),
    os.path.join('third_party', 'yasm'),
    os.path.join('v8', 'strongtalk'),
])


class LicenseError(Exception):
  """We raise this exception when a directory's licensing info isn't
    fully filled out."""
  pass


def AbsolutePath(path, filename, root):
  """Convert a path in README.chromium to be absolute based on the source
    root."""
  if filename.startswith('/'):
    # Absolute-looking paths are relative to the source root
    # (which is the directory we're run from).
    absolute_path = os.path.join(root, filename[1:])
  else:
    absolute_path = os.path.join(root, path, filename)
  if os.path.exists(absolute_path):
    return absolute_path
  return None


def ParseDir(path, root, require_license_file=True, optional_keys=None):
  """Examine a third_party/foo component and extract its metadata."""
  # Parse metadata fields out of README.chromium.
  # We examine "LICENSE" for the license file by default.
  metadata = {
      "License File": "LICENSE",  # Relative path to license text.
      "Name": None,  # Short name (for header on about:credits).
      "URL": None,  # Project home page.
      "License": None,  # Software license.
  }

  if optional_keys is None:
    optional_keys = []

  if path in SPECIAL_CASES:
    metadata.update(SPECIAL_CASES[path])
  else:
    # Try to find README.chromium.
    readme_path = os.path.join(root, path, 'README.chromium')
    if not os.path.exists(readme_path):
      raise LicenseError("missing README.chromium or licenses.py "
                         "SPECIAL_CASES entry in %s\n" % path)

    for line in codecs.open(readme_path, encoding='utf-8'):
      line = line.strip()
      if not line:
        break
      for key in list(metadata.keys()) + optional_keys:
        field = key + ": "
        if line.startswith(field):
          metadata[key] = line[len(field):]

  # Check that all expected metadata is present.
  errors = []
  for key, value in metadata.items():
    if not value:
      errors.append("couldn't find '" + key + "' line "
                    "in README.chromium or licences.py "
                    "SPECIAL_CASES")

  # Special-case modules that aren't in the shipping product, so don't need
  # their license in about:credits.
  if metadata["License File"] != NOT_SHIPPED:
    # Check that the license file exists.
    for filename in (metadata["License File"], "COPYING"):
      license_path = AbsolutePath(path, filename, root)
      if license_path is not None:
        break

    if require_license_file and not license_path:
      errors.append("License file not found. "
                    "Either add a file named LICENSE, "
                    "import upstream's COPYING if available, "
                    "or add a 'License File:' line to "
                    "README.chromium with the appropriate path.")
    metadata["License File"] = license_path

  if errors:
    raise LicenseError("Errors in %s:\n %s\n" % (path, ";\n ".join(errors)))
  return metadata


def ContainsFiles(path, root):
  """Determines whether any files exist in a directory or in any of its
    subdirectories."""
  for _, dirs, files in os.walk(os.path.join(root, path)):
    if files:
      return True
    for vcs_metadata in VCS_METADATA_DIRS:
      if vcs_metadata in dirs:
        dirs.remove(vcs_metadata)
  return False


def FilterDirsWithFiles(dirs_list, root):
  # If a directory contains no files, assume it's a DEPS directory for a
  # project not used by our current configuration and skip it.
  return [x for x in dirs_list if ContainsFiles(x, root)]


def ProcessAdditionalReadmePathsJson(root, dirname, third_party_dirs):
  """For a given directory, process the additional readme paths, and add to
    third_party_dirs."""
  additional_paths_file = os.path.join(root, dirname, ADDITIONAL_PATHS_FILENAME)
  if os.path.exists(additional_paths_file):
    with codecs.open(additional_paths_file, encoding='utf-8') as paths_file:
      extra_paths = json.load(paths_file)
      third_party_dirs.update([os.path.join(dirname, p) for p in extra_paths])


def FindThirdPartyDirs(prune_paths, root, extra_third_party_dirs=None):
  """Find all third_party directories underneath the source root."""
  third_party_dirs = set()
  for path, dirs, files in os.walk(root):
    path = path[len(root) + 1:]  # Pretty up the path.

    # .gitignore ignores /out*/, so do the same here.
    if path in prune_paths or path.startswith('out'):
      dirs[:] = []
      continue

    # Prune out directories we want to skip.
    # (Note that we loop over PRUNE_DIRS so we're not iterating over a
    # list that we're simultaneously mutating.)
    for skip in PRUNE_DIRS:
      if skip in dirs:
        dirs.remove(skip)

    if os.path.basename(path) == 'third_party':
      # Add all subdirectories that are not marked for skipping.
      for dir in dirs:
        dirpath = os.path.join(path, dir)
        if dirpath not in prune_paths:
          third_party_dirs.add(dirpath)

        ProcessAdditionalReadmePathsJson(root, dirpath, third_party_dirs)

      # Don't recurse into any subdirs from here.
      dirs[:] = []
      continue

    # Don't recurse into paths in ADDITIONAL_PATHS, like we do with regular
    # third_party/foo paths.
    if path in ADDITIONAL_PATHS:
      dirs[:] = []

  extra_paths = set(ADDITIONAL_PATHS)
  if extra_third_party_dirs:
    extra_paths.update(extra_third_party_dirs)

  for dir in extra_paths:
    if dir not in prune_paths:
      third_party_dirs.add(dir)
      ProcessAdditionalReadmePathsJson(root, dir, third_party_dirs)

  return sorted(third_party_dirs)


def FindThirdPartyDirsWithFiles(root):
  third_party_dirs = FindThirdPartyDirs(PRUNE_PATHS, root)
  return FilterDirsWithFiles(third_party_dirs, root)


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


def GetThirdPartyDepsFromGNDepsOutput(
    gn_deps: str,
    target_os: str,
    extra_allowed_dirs: Optional[List[str]] = None):
  """Returns third_party/foo directories given the output of "gn desc deps".

    Note that it always returns the direct sub-directory of third_party
    where README.chromium and LICENSE files are, so that it can be passed to
    ParseDir(). e.g.:
        third_party/cld_3/src/src/BUILD.gn -> third_party/cld_3

    It returns relative paths from _REPOSITORY_ROOT, not absolute paths.
    """
  allowed_paths_list = ['third_party']
  if extra_allowed_dirs:
    allowed_paths_list.extend(extra_allowed_dirs)

  # Use non-capturing group with or's for all possible options.
  allowed_paths = '|'.join([re.escape(x) for x in allowed_paths_list])
  path_regex = re.compile(r'^((.+[/\\])?(?:' + allowed_paths +
                          r')[/\\][^/\\]+[/\\])(.+[/\\])?BUILD\.gn$')

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
      ############################################################################

      This is known issue, please report the failure to https://crbug.com/1208393.

      ############################################################################
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
  third_party_dirs = FindThirdPartyDirsWithFiles(root)

  errors = []
  for path in sorted(third_party_dirs):
    try:
      metadata = ParseDir(path, root)
    except LicenseError as e:
      errors.append((path, e.args[0]))
      continue

  for path, error in sorted(errors):
    print(path + ": " + error)

  return len(errors) == 0


def GenerateCredits(file_template_file,
                    entry_template_file,
                    output_file,
                    target_os,
                    gn_out_dir,
                    gn_target,
                    extra_third_party_dirs=None,
                    depfile=None):
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
    env = {
        'name': metadata['Name'],
        'url': metadata['URL'],
        'license': codecs.open(metadata['License File'],
                               encoding='utf-8').read(),
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
    third_party_dirs = FindThirdPartyDirs(PRUNE_PATHS, _REPOSITORY_ROOT,
                                          extra_third_party_dirs)

  if not file_template_file:
    file_template_file = os.path.join(_REPOSITORY_ROOT, 'components',
                                      'about_ui', 'resources',
                                      'about_credits.tmpl')
  if not entry_template_file:
    entry_template_file = os.path.join(_REPOSITORY_ROOT, 'components',
                                       'about_ui', 'resources',
                                       'about_credits_entry.tmpl')

  entry_template = codecs.open(entry_template_file, encoding='utf-8').read()
  entries = []
  # Start from Chromium's LICENSE file
  chromium_license_metadata = {
      'Name': 'The Chromium Project',
      'URL': 'http://www.chromium.org',
      'License File': os.path.join(_REPOSITORY_ROOT, 'LICENSE')
  }
  entries.append(
      MetadataToTemplateEntry(chromium_license_metadata, entry_template))

  entries_by_name = {}
  for path in third_party_dirs:
    try:
      metadata = ParseDir(path, _REPOSITORY_ROOT)
    except LicenseError:
      # TODO(phajdan.jr): Convert to fatal error (http://crbug.com/39240).
      continue
    if metadata['License File'] == NOT_SHIPPED:
      continue
    if target_os == 'ios' and not gn_target:
      # Skip over files that are known not to be used on iOS. But
      # skipping is unnecessary if GN was used to query the actual
      # dependencies.
      # TODO(lambroslambrou): Remove this step once the iOS build is
      # updated to provide --gn-target to this script.
      if path in KNOWN_NON_IOS_LIBRARIES:
        continue

    new_entry = MetadataToTemplateEntry(metadata, entry_template)
    # Skip entries that we've already seen (it exists in multiple directories).
    prev_entry = entries_by_name.setdefault(new_entry['name'], new_entry)
    if prev_entry is not new_entry and (
        prev_entry['content'] == new_entry['content']):
      continue

    entries.append(new_entry)

  entries.sort(key=lambda entry: (entry['name'].lower(), entry['content']))
  for entry_id, entry in enumerate(entries):
    entry['content'] = entry['content'].replace('{{id}}', str(entry_id))

  entries_contents = '\n'.join([entry['content'] for entry in entries])
  file_template = codecs.open(file_template_file, encoding='utf-8').read()
  template_contents = "<!-- Generated by licenses.py; do not edit. -->"
  template_contents += EvaluateTemplate(
      file_template, {'entries': entries_contents}, escape=False)

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
    license_file_list = (entry['license_file'] for entry in entries)
    license_file_list = (os.path.relpath(p) for p in license_file_list)
    license_file_list = sorted(set(license_file_list))
    action_helpers.write_depfile(depfile, output_file,
                                 license_file_list + ['build.ninja'])

  return True


def GenerateLicenseFile(args: argparse.Namespace):
  """Top level function for the license file generation.

  Either saves the text to a file or prints it to stdout depending on if
    args.output_file is set.

  Args:
    args: parsed arguments passed from the cli
  """
  # Convert the path separators to the OS specific ones
  extra_third_party_dirs = args.extra_third_party_dirs
  if extra_third_party_dirs is not None:
    extra_third_party_dirs = [
        os.path.normpath(path) for path in extra_third_party_dirs
    ]

  third_party_dirs = FindThirdPartyDeps(args.gn_out_dir, args.gn_target,
                                        args.target_os, extra_third_party_dirs,
                                        args.extra_allowed_dirs)

  metadatas = {
      d: ParseDir(d, _REPOSITORY_ROOT, require_license_file=True)
      for d in third_party_dirs
  }

  if args.format == 'spdx':
    license_txt = GenerateLicenseFileSpdx(metadatas, args.spdx_link,
                                          args.spdx_root, args.spdx_doc_name,
                                          args.spdx_doc_namespace)
  elif args.format == 'txt':
    license_txt = GenerateLicenseFilePlainText(metadatas)
  else:
    raise ValueError(f'Unknown license format: {args.format}')

  if args.output_file:
    with open(args.output_file, 'w', encoding='utf-8') as f:
      f.write(license_txt)
  else:
    print(license_txt)


def GenerateLicenseFilePlainText(
    metadata: Dict[str, Dict[str, Any]],
    repo_root: str = _REPOSITORY_ROOT,
    read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')) -> str:
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
    license_file = dir_metadata['License File']
    if license_file and license_file != NOT_SHIPPED:
      content.append('-' * 20)
      content.append(directory.split(os.sep)[-1])
      content.append('-' * 20)
      content.append(read_file(os.path.join(repo_root, license_file)))

  return '\n'.join(content)


def GenerateLicenseFileSpdx(
    metadata: Dict[str, Dict[str, Any]],
    spdx_link_prefix: str,
    spdx_root: str,
    doc_name: Optional[str],
    doc_namespace: Optional[str],
    repo_root: str = _REPOSITORY_ROOT,
    read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')) -> str:
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
    license_file = dir_metadata['License File']
    if license_file and license_file != NOT_SHIPPED:
      license_file = os.path.join(repo_root, license_file)
      spdx_writer.add_package(dir_metadata['Name'], license_file)

  return spdx_writer.write()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--file-template', help='Template HTML to use for the license page.')
  parser.add_argument(
      '--entry-template', help='Template HTML to use for each license.')
  parser.add_argument(
      '--extra-third-party-dirs',
      help='Gn list of additional third_party dirs to look through.')
  parser.add_argument(
      '--extra-allowed-dirs',
      help=('List of extra allowed paths to look for '
            '(deps that will be picked up automatically) besides third_party'),
  )
  parser.add_argument('--target-os', help='OS that this build is targeting.')
  parser.add_argument(
      '--gn-out-dir', help='GN output directory for scanning dependencies.')
  parser.add_argument('--gn-target', help='GN target to scan for dependencies.')
  parser.add_argument('--format',
                      default='txt',
                      choices=['txt', 'spdx'],
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
      'command', choices=['help', 'scan', 'credits', 'license_file'])
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
    if not GenerateCredits(args.file_template, args.entry_template,
                           args.output_file, args.target_os, args.gn_out_dir,
                           args.gn_target, args.extra_third_party_dirs,
                           args.depfile):
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
