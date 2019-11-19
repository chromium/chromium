#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that all files contain proper licensing information."""

from __future__ import print_function

import json
import optparse
import os.path
import re
import subprocess
import sys


def PrintUsage():
  print("""Usage: python checklicenses.py [--root <root>] [tocheck]
  --root   Specifies the repository root. This defaults to "../.." relative
           to the script file. This will be correct given the normal location
           of the script in "<root>/tools/checklicenses".

  --ignore-suppressions  Ignores path-specific license whitelist. Useful when
                         trying to remove a suppression/whitelist entry.

  tocheck  Specifies the directory, relative to root, to check. This defaults
           to "." so it checks everything.

Examples:
  python checklicenses.py
  python checklicenses.py --root ~/chromium/src third_party""")


WHITELISTED_LICENSES = [
    'APSL (v2) BSD (4 clause)',
    'APSL (v2)',
    'Anti-Grain Geometry',
    'Apache (v2.0) BSD (2 clause)',
    'Apache (v2.0) BSD-like',
    'Apache (v2.0) GPL (v2)',
    'Apache (v2.0) ISC',
    'Apache (v2.0)',
    'Apple MIT',  # https://fedoraproject.org/wiki/Licensing/Apple_MIT_License
    'BSD (2 clause) ISC',
    'BSD (2 clause) MIT/X11 (BSD like)',
    'BSD (2 clause)',
    'BSD (3 clause) GPL (v2)',
    'BSD (3 clause) ISC',
    'BSD (3 clause) LGPL (v2 or later)',
    'BSD (3 clause) LGPL (v2.1 or later)',
    'BSD (3 clause) MIT/X11 (BSD like)',
    'BSD (3 clause)',
    'BSD (4 clause)',
    'BSD',
    'BSD-like',

    # TODO(phajdan.jr): Make licensecheck not print BSD-like twice.
    'BSD MIT/X11 (BSD like)',
    'BSD-like MIT/X11 (BSD like)',

    'BSL (v1.0)',
    'BSL (v1) LGPL (v2.1 or later)',
    'FreeType (BSD like) with patent clause',
    'FreeType (BSD like)',
    'GPL (v2 or later) with Bison parser exception',
    'GPL (v2 or later) with libtool exception',
    'GPL (v2) LGPL (v2.1 or later)',
    'GPL (v3 or later) LGPL (v2.1 or later) with Bison parser exception',
    'GPL (v3 or later) with Bison parser exception',
    'GPL with Bison parser exception',
    'ISC',
    'Independent JPEG Group License',
    'LGPL (unversioned/unknown version)',
    'LGPL (v2 or later)',
    'LGPL (v2)',
    'LGPL (v2.1 or later)',
    'LGPL (v2.1)',
    'LGPL (v3 or later)',
    'MIT/X11 (BSD like) LGPL (v2.1 or later)',
    'MIT/X11 (BSD like)',
    'MIT/X11 (BSD like) Public domain MIT/X11 (BSD like)',
    'MPL (v1.0) LGPL (v2 or later)',
    'MPL (v1.1) BSD (3 clause) GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) BSD (3 clause) LGPL (v2.1 or later)',
    'MPL (v1.1) BSD-like GPL (unversioned/unknown version)',
    'MPL (v1.1) BSD-like GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) BSD-like LGPL (v2.1 or later)',
    'MPL (v1.1) BSD-like',
    'MPL (v1.1) GPL (unversioned/unknown version)',
    'MPL (v1.1) GPL (v2) LGPL (v2 or later)',
    'MPL (v1.1) GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) GPL (v2)',
    'MPL (v1.1) LGPL (v2 or later)',
    'MPL (v1.1) LGPL (v2.1 or later)',
    'MPL (v1.1)',
    'MPL (v2.0)',
    'Ms-PL',
    'Public domain BSD (3 clause)',
    'Public domain BSD',
    'Public domain BSD-like',
    'Public domain LGPL (v2.1 or later)',
    'Public domain University of Illinois/NCSA Open Source License (BSD like)',
    'Public domain',
    'SGI Free Software License B',
    'SunSoft (BSD like)',
    'libpng',
    'zlib/libpng',
    'University of Illinois/NCSA Open Source License (BSD like)',
    ('University of Illinois/NCSA Open Source License (BSD like) '
     'MIT/X11 (BSD like)'),
]


PATH_SPECIFIC_WHITELISTED_LICENSES = {
    'base/third_party/icu': [  # http://crbug.com/98087
        'UNKNOWN',
    ],

    'base/third_party/libevent': [  # http://crbug.com/98309
        'UNKNOWN',
    ],

    'buildtools/third_party/libc++/trunk/test': [
        # http://llvm.org/bugs/show_bug.cgi?id=25980
        'UNKNOWN',
    ],
    # http://llvm.org/bugs/show_bug.cgi?id=25976
    'buildtools/third_party/libc++/trunk/src/include/atomic_support.h': [
      'UNKNOWN'
    ],
    'buildtools/third_party/libc++/trunk/utils/gen_link_script': [ 'UNKNOWN' ],
    'buildtools/third_party/libc++/trunk/utils/not': [ 'UNKNOWN' ],
    'buildtools/third_party/libc++/trunk/utils/sym_check': [ 'UNKNOWN' ],
    'buildtools/third_party/libc++abi/trunk/test': [ 'UNKNOWN' ],

    'chrome/common/extensions/docs/examples': [  # http://crbug.com/98092
        'UNKNOWN',
    ],
    # This contains files copied from elsewhere from the tree. Since the copied
    # directories might have suppressions below (like simplejson), whitelist the
    # whole directory. This is also not shipped code.
    'chrome/common/extensions/docs/server2/third_party': [
        'UNKNOWN',
    ],
    'native_client': [  # http://crbug.com/98099
        'UNKNOWN',
    ],
    'native_client/toolchain': [
        'BSD GPL (v2 or later)',
        'BSD (2 clause) GPL (v2 or later)',
        'BSD (3 clause) GPL (v2 or later)',
        'BSD (4 clause) ISC',
        'BSL (v1.0) GPL',
        'BSL (v1.0) GPL (v3.1)',
        'GPL',
        'GPL (unversioned/unknown version)',
        'GPL (v2)',
        'GPL (v2 or later)',
        'GPL (v3.1)',
        'GPL (v3 or later)',
        'MPL (v1.1) LGPL (unversioned/unknown version)',
    ],

    # The project is BSD-licensed but the individual files do not have
    # consistent license headers. Also, this is just used in a utility
    # and not shipped. https://github.com/waylan/Python-Markdown/issues/435
    'third_party/Python-Markdown': [
        'UNKNOWN',
    ],

    # https://bugs.chromium.org/p/swiftshader/issues/detail?id=1
    'third_party/swiftshader': [
        'UNKNOWN',
    ],

    # http://code.google.com/p/angleproject/issues/detail?id=217
    'third_party/angle': [
        'UNKNOWN',
    ],

    'third_party/blink': [
        'UNKNOWN',
    ],

    # https://crbug.com/google-breakpad/450
    'third_party/breakpad/breakpad': [
        'UNKNOWN',
    ],

    # http://crbug.com/603946
    # https://github.com/google/oauth2client/issues/331
    # Just imports googleapiclient. Chromite is not shipped.
    'third_party/chromite/third_party/apiclient': [
        'UNKNOWN',
    ],

    # http://crbug.com/603946
    # https://github.com/google/google-api-python-client/issues/216
    # Apache (v2.0) license. Chromite is not shipped.
    'third_party/chromite/third_party/googleapiclient/channel.py': [
        'UNKNOWN',
    ],

    # http://crbug.com/222828
    # http://bugs.python.org/issue17514
    'third_party/chromite/third_party/argparse.py': [
        'UNKNOWN',
    ],

    # http://crbug.com/603939
    # https://github.com/jcgregorio/httplib2/issues/307
    # MIT license. Chromite is not shipped.
    'third_party/chromite/third_party/httplib2': [
        'UNKNOWN',
    ],

    # http://crbug.com/326117
    # https://bitbucket.org/chrisatlee/poster/issue/21
    'third_party/chromite/third_party/poster': [
        'UNKNOWN',
    ],

    # http://crbug.com/603944
    # https://github.com/kennethreitz/requests/issues/1610
    # Apache (v2.0) license. Chromite is not shipped
    'third_party/chromite/third_party/requests': [
        'UNKNOWN',
    ],

    # http://crbug.com/333508
    'buildtools/clang_format/script': [
        'UNKNOWN',
    ],

    'third_party/devscripts': [
        'GPL (v2 or later)',
    ],
    'third_party/catapult/firefighter/default/tracing/third_party/devscripts': [
        'GPL (v2 or later)',
    ],
    'third_party/catapult/tracing/third_party/devscripts': [
        'GPL (v2 or later)',
    ],

    # https://github.com/shazow/apiclient/issues/8
    # MIT license.
    'third_party/catapult/third_party/apiclient': [
        'UNKNOWN',
    ],

    # https://bugs.launchpad.net/beautifulsoup/+bug/1481316
    # MIT license.
    'third_party/catapult/third_party/beautifulsoup': [
        'UNKNOWN'
    ],

    # https://bitbucket.org/ned/coveragepy/issue/313/add-license-file-containing-2-3-or-4
    # Apache (v2.0) license, not shipped
    'third_party/catapult/third_party/coverage': [
        'UNKNOWN'
    ],

    # https://code.google.com/p/graphy/issues/detail?id=6
    # Apache (v2.0)
    'third_party/catapult/third_party/graphy': [
        'UNKNOWN',
    ],

    # https://github.com/GoogleCloudPlatform/gsutil/issues/305
    ('third_party/catapult/third_party/gsutil/gslib/third_party/'
     'storage_apitools'): [
        'UNKNOWN',
    ],

    # https://github.com/google/apitools/issues/63
    'third_party/catapult/third_party/gsutil/third_party/apitools': [
        'UNKNOWN',
    ],

    # https://github.com/boto/boto/issues/3373
    'third_party/catapult/third_party/gsutil/third_party/boto': [
        'UNKNOWN',
    ],

    # https://bitbucket.org/cmcqueen1975/crcmod/issues/1/please-add-per-file-licenses
    # Includes third_party/catapult/third_party/gsutil/third_party/crcmod_osx.
    'third_party/catapult/third_party/gsutil/third_party/crcmod': [
        'UNKNOWN',
    ],

    # https://github.com/jcgregorio/httplib2/issues/307
    'third_party/catapult/third_party/gsutil/third_party/httplib2': [
        'UNKNOWN',
    ],

    # https://github.com/google/oauth2client/issues/331
    'third_party/catapult/third_party/gsutil/third_party/oauth2client': [
        'UNKNOWN',
    ],

    # https://github.com/google/protorpc/issues/14
    'third_party/catapult/third_party/gsutil/third_party/protorpc': [
        'UNKNOWN',
    ],

    # https://sourceforge.net/p/pyasn1/tickets/4/
    # Includes
    # third_party/catapult/third_party/gsutil/third_party/pyasn1-modules.
    'third_party/catapult/third_party/gsutil/third_party/pyasn1': [
        'UNKNOWN',
    ],

    # https://github.com/pnpnpn/retry-decorator/issues/4
    'third_party/catapult/third_party/gsutil/third_party/retry-decorator': [
        'UNKNOWN',
    ],

    # https://bitbucket.org/sybren/python-rsa/issues/28/please-add-per-file-licenses
    'third_party/catapult/third_party/gsutil/third_party/rsa': [
        'UNKNOWN',
    ],

    # https://bitbucket.org/gutworth/six/issues/137/please-add-per-file-licenses
    # Already fixed upstream. https://crbug.com/573341
    'third_party/catapult/third_party/gsutil/third_party/six': [
        'UNKNOWN',
    ],

    # https://github.com/html5lib/html5lib-python/issues/125
    # MIT license.
    'third_party/catapult/third_party/html5lib-python': [
        'UNKNOWN',
    ],

    # https://github.com/GoogleCloudPlatform/appengine-mapreduce/issues/71
    # Apache (v2.0)
    'third_party/catapult/third_party/mapreduce': [
        'UNKNOWN',
    ],

    # https://code.google.com/p/webapp-improved/issues/detail?id=103
    # Apache (v2.0).
    'third_party/catapult/third_party/webapp2': [
        'UNKNOWN',
    ],

    # https://github.com/Pylons/webob/issues/211
    # MIT license.
    'third_party/catapult/third_party/WebOb': [
        'UNKNOWN',
    ],

    # https://github.com/Pylons/webtest/issues/141
    # MIT license.
    'third_party/catapult/third_party/webtest': [
        'UNKNOWN',
    ],

    # https://bitbucket.org/ianb/paste/issues/12/add-license-headers-to-source-files
    # MIT license.
    'third_party/catapult/third_party/Paste': [
        'UNKNOWN',
    ],

    'third_party/expat/files/lib': [  # http://crbug.com/98121
        'UNKNOWN',
    ],
    'third_party/ffmpeg': [
        'GPL',
        'GPL (v2)',
        'GPL (v2 or later)',
        'GPL (v3 or later)',
        'UNKNOWN',  # http://crbug.com/98123
    ],
    'third_party/fontconfig': [
        # https://bugs.freedesktop.org/show_bug.cgi?id=73401
        'UNKNOWN',
    ],
    'third_party/freetype2': [ # http://crbug.com/177319
        'UNKNOWN',
    ],
    'third_party/freetype-android': [ # http://crbug.com/177319
        'UNKNOWN',
    ],
    'third_party/grpc': [ # https://github.com/grpc/grpc/issues/6951
        'UNKNOWN',
    ],
    'third_party/hunspell': [  # http://crbug.com/98134
        'UNKNOWN',
    ],
    'third_party/iccjpeg': [  # http://crbug.com/98137
        'UNKNOWN',
    ],
    'third_party/icu': [  # http://crbug.com/98301
        'UNKNOWN',
    ],
    'third_party/jmake': [  # Used only at build time.
        'GPL (v2)',
    ],
    'third_party/jsoncpp/source': [
        # https://github.com/open-source-parsers/jsoncpp/issues/234
        'UNKNOWN',
    ],
    'third_party/junit/src': [
        # Pulled in via DEPS for Android only.
        # Eclipse Public License / not shipped.
        # Bug filed but upstream prefers not to fix.
        # https://github.com/junit-team/junit/issues/1132
        'UNKNOWN',
    ],
    'third_party/lcov': [  # http://crbug.com/98304
        'UNKNOWN',
    ],
    'third_party/lcov/contrib/galaxy/genflat.pl': [
        'GPL (v2 or later)',
    ],
    'third_party/libjpeg_turbo': [  # http://crbug.com/98314
        'UNKNOWN',
    ],

    # Many liblouis files are mirrored but not used in the NaCl module.
    # They are not excluded from the mirror because of lack of infrastructure
    # support.  Getting license headers added to the files where missing is
    # tracked in https://github.com/liblouis/liblouis/issues/22.
    'third_party/liblouis/src': [
        'GPL (v3 or later)',
        'UNKNOWN',
    ],

    # The following files have a special license.
    'third_party/libovr/src': [
        'UNKNOWN',
    ],

    # The following files lack license headers, but are trivial.
    'third_party/libusb/src/libusb/os/poll_posix.h': [
        'UNKNOWN',
    ],

    'third_party/libxml': [
        'UNKNOWN',
    ],
    'third_party/libxslt': [
        'UNKNOWN',
    ],
    'third_party/lzma_sdk': [
        'UNKNOWN',
    ],
    'third_party/modp_b64': [
        'UNKNOWN',
    ],
    'third_party/nvml': [
        'UNKNOWN',
    ],
    # Missing license headers in openh264 sources: https://github.com/cisco/openh264/issues/2233
    'third_party/openh264/src': [
        'UNKNOWN',
    ],
    'third_party/boringssl': [
        # There are some files in BoringSSL which came from OpenSSL and have no
        # license in them. We don't wish to add the license header ourselves
        # thus we don't expect to pass license checks.
        'UNKNOWN',
    ],
    'third_party/molokocacao': [  # http://crbug.com/98453
        'UNKNOWN',
    ],
    'third_party/ocmock/OCMock': [  # http://crbug.com/98454
        'UNKNOWN',
    ],
    'third_party/protobuf': [  # http://crbug.com/98455
        'UNKNOWN',
    ],

    # https://bitbucket.org/ned/coveragepy/issue/313/add-license-file-containing-2-3-or-4
    # BSD 2-clause license.
    'third_party/pycoverage': [
        'UNKNOWN',
    ],

    'third_party/pyelftools': [ # http://crbug.com/222831
        'UNKNOWN',
    ],
    'third_party/scons-2.0.1/engine/SCons': [  # http://crbug.com/98462
        'UNKNOWN',
    ],
    'third_party/sfntly/src/java': [  # Apache 2.0, not shipped.
        'UNKNOWN',
    ],
    'third_party/simplejson': [
        'UNKNOWN',
    ],
    'third_party/skia': [  # http://crbug.com/98463
        'UNKNOWN',
    ],
    'third_party/snappy/src': [  # http://crbug.com/98464
        'UNKNOWN',
    ],
    'third_party/smhasher/src': [  # http://crbug.com/98465
        'UNKNOWN',
    ],
    'third_party/speech-dispatcher/libspeechd.h': [
        'GPL (v2 or later)',
    ],
    'third_party/sqlite': [
        'UNKNOWN',
    ],

    # New BSD license. http://crbug.com/98455
    'tools/swarming_client/third_party/google': [
        'UNKNOWN',
    ],

    # https://github.com/google/google-api-python-client/issues/216
    # Apache v2.0.
    'tools/swarming_client/third_party/googleapiclient': [
        'UNKNOWN',
    ],

    # http://crbug.com/334668
    # https://github.com/jcgregorio/httplib2/issues/307
    # MIT license.
    'tools/swarming_client/third_party/httplib2': [
        'UNKNOWN',
    ],

    # http://crbug.com/471372
    # BSD
    'tools/swarming_client/third_party/pyasn1': [
        'UNKNOWN',
    ],

    # https://github.com/kennethreitz/requests/issues/1610
    'tools/swarming_client/third_party/requests': [
        'UNKNOWN',
    ],

    'third_party/minizip': [
        'UNKNOWN',
    ],

    # BSD License. http://bugzilla.maptools.org/show_bug.cgi?id=2532
    'third_party/pdfium/third_party/libtiff/tif_ojpeg.c': [
        'UNKNOWN',
    ],
    'third_party/pdfium/third_party/libtiff/tiffvers.h': [
        'UNKNOWN',
    ],
    'third_party/pdfium/third_party/libtiff/uvcode.h': [
        'UNKNOWN',
    ],

    'third_party/talloc': [
        'GPL (v3 or later)',
        'UNKNOWN',  # http://crbug.com/98588
    ],
    'third_party/tcmalloc': [
        'UNKNOWN',  # http://crbug.com/98589
    ],
    'third_party/tlslite': [
        'UNKNOWN',
    ],
    # MIT license but some files contain no licensing info. e.g. autogen.sh.
    # Files missing licensing info are not shipped.
    'third_party/wayland': [  #  http://crbug.com/553573
        'UNKNOWN',
    ],
    'third_party/webdriver': [  # http://crbug.com/98590
        'UNKNOWN',
    ],

    # https://github.com/html5lib/html5lib-python/issues/125
    # https://github.com/KhronosGroup/WebGL/issues/435
    'third_party/webgl/src': [
        'UNKNOWN',
    ],

    'third_party/webrtc': [  # http://crbug.com/98592
        'UNKNOWN',
    ],
    'third_party/xdg-utils': [  # http://crbug.com/98593
        'UNKNOWN',
    ],
    'third_party/yasm/source': [  # http://crbug.com/98594
        'UNKNOWN',
    ],
    'third_party/zlib/contrib/minizip': [
        'UNKNOWN',
    ],
    'third_party/zlib/trees.h': [
        'UNKNOWN',
    ],
    'tools/emacs': [  # http://crbug.com/98595
        'UNKNOWN',
    ],
    'tools/gyp/test': [
        'UNKNOWN',
    ],
    # Perf test data from Google Maps team. Not shipped.
    'tools/perf/page_sets/maps_perf_test': [
        'UNKNOWN',
    ],
    'tools/python/google/__init__.py': [
        'UNKNOWN',
    ],
    'tools/stats_viewer/Properties/AssemblyInfo.cs': [
        'UNKNOWN',
    ],
    'tools/symsrc/pefile.py': [
        'UNKNOWN',
    ],
    # Not shipped, MIT license but the header files contain no licensing info.
    'third_party/catapult/telemetry/third_party/altgraph': [
        'UNKNOWN',
    ],
    # Not shipped, MIT license but the header files contain no licensing info.
    'third_party/catapult/telemetry/third_party/modulegraph': [
        'UNKNOWN',
    ],
    'third_party/catapult/telemetry/third_party/pyserial': [
        # https://sourceforge.net/p/pyserial/feature-requests/35/
        'UNKNOWN',
    ],
    # Not shipped, GPL license but some header files contain no licensing info.
    'third_party/logilab/logilab/astroid': [
        'GPL (v2 or later)',
        # https://github.com/PyCQA/astroid/issues/336
        'UNKNOWN',
    ],
    # Not shipped, GPL license but some header files contain no licensing info.
    'third_party/logilab/logilab/common': [
        'GPL (v2 or later)',
        # Look for email by nednguyen@google.com in May 5th in
        # https://lists.logilab.org/pipermail/python-projects/
        'UNKNOWN',
    ],
    # Not shipped, GPL license but some header files contain no licensing info.
    'third_party/pylint': [
        'GPL (v2 or later)',
        # https://github.com/PyCQA/pylint/issues/894
        'UNKNOWN',
    ],
}

EXCLUDED_PATHS = [
    # Don't check generated files
    re.compile('^out/'),

    # Don't check downloaded goma client binaries
    re.compile('^build/goma/client/'),

    # Don't check sysroot directories
    re.compile('^build/linux/.+-sysroot/'),
]


def check_licenses(options, args):
  # Figure out which directory we have to check.
  if len(args) == 0:
    # No directory to check specified, use the repository root.
    start_dir = options.base_directory
  elif len(args) == 1:
    # Directory specified. Start here. It's supposed to be relative to the
    # base directory.
    start_dir = os.path.abspath(os.path.join(options.base_directory, args[0]))
  else:
    # More than one argument, we don't handle this.
    PrintUsage()
    return 1

  print("Using base directory:", options.base_directory)
  print("Checking:", start_dir)
  print()

  licensecheck_path = os.path.abspath(os.path.join(options.base_directory,
                                                   'third_party',
                                                   'devscripts',
                                                   'licensecheck.pl'))

  licensecheck = subprocess.Popen([licensecheck_path,
                                   '-l', '100',
                                   '-r', start_dir],
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
  stdout, stderr = licensecheck.communicate()
  if options.verbose:
    print('----------- licensecheck stdout -----------')
    print(stdout)
    print('--------- end licensecheck stdout ---------')
  if licensecheck.returncode != 0 or stderr:
    print('----------- licensecheck stderr -----------')
    print(stderr)
    print('--------- end licensecheck stderr ---------')
    print("\nFAILED\n")
    return 1

  used_suppressions = set()
  errors = []

  for line in stdout.splitlines():
    filename, license = line.split(':', 1)
    filename = os.path.relpath(filename.strip(), options.base_directory)

    # Check if the file belongs to one of the excluded paths.
    if any((pattern.match(filename) for pattern in EXCLUDED_PATHS)):
      continue

    # For now we're just interested in the license.
    license = license.replace('*No copyright*', '').strip()

    # Skip generated files.
    if 'GENERATED FILE' in license:
      continue

    if license in WHITELISTED_LICENSES:
      continue

    if not options.ignore_suppressions:
      matched_prefixes = [
          prefix for prefix in PATH_SPECIFIC_WHITELISTED_LICENSES
          if filename.startswith(prefix) and
          license in PATH_SPECIFIC_WHITELISTED_LICENSES[prefix]]
      if matched_prefixes:
        used_suppressions.update(set(matched_prefixes))
        continue

    errors.append({'filename': filename, 'license': license})

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(errors, f)

  if errors:
    for error in errors:
      print("'%s' has non-whitelisted license '%s'" % (error['filename'],
                                                       error['license']))
    print("\nFAILED\n")
    print("Please read", end=' ')
    print("http://www.chromium.org/developers/adding-3rd-party-libraries")
    print("for more info how to handle the failure.")
    print()
    print("Please respect OWNERS of checklicenses.py. Changes violating")
    print("this requirement may be reverted.")

    # Do not print unused suppressions so that above message is clearly
    # visible and gets proper attention. Too much unrelated output
    # would be distracting and make the important points easier to miss.

    return 1

  print("\nSUCCESS\n")

  if not len(args):
    unused_suppressions = set(
        PATH_SPECIFIC_WHITELISTED_LICENSES.iterkeys()).difference(
            used_suppressions)
    if unused_suppressions:
      print("\nNOTE: unused suppressions detected:\n")
      print('\n'.join(unused_suppressions))

  return 0


def main():
  default_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..'))
  option_parser = optparse.OptionParser()
  option_parser.add_option('--root', default=default_root,
                           dest='base_directory',
                           help='Specifies the repository root. This defaults '
                           'to "../.." relative to the script file, which '
                           'will normally be the repository root.')
  option_parser.add_option('-v', '--verbose', action='store_true',
                           default=False, help='Print debug logging')
  option_parser.add_option('--ignore-suppressions',
                           action='store_true',
                           default=False,
                           help='Ignore path-specific license whitelist.')
  option_parser.add_option('--json', help='Path to JSON output file')
  options, args = option_parser.parse_args()
  return check_licenses(options, args)


if '__main__' == __name__:
  sys.exit(main())
