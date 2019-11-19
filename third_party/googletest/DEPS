# This file is a dummy used so that non-Chromium clients can specify
# recursive DEPS. Otherwise the clients would need to nest DEPS inside
# each other. Nested DEPS are not supported by gclient.
#
# Clients *must* specify googletest_revision when using this DEPS file.

use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # We must specify a dummy variable here for recursedeps to work.
  'googletest_revision': 'master',
}

deps = {
  'src': '{chromium_git}/external/github.com/google/googletest.git@{googletest_revision}'
}
