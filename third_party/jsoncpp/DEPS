# This file is a dummy used so that non-Chromium clients can specify
# recursive DEPS. Otherwise the clients would need to nest DEPS inside
# each other. Nested DEPS are not supported by gclient.
#
# Clients *must* specify jsoncpp_revision when using this DEPS file.

use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # We must specify a dummy variable here for recursedeps to work.
  'jsoncpp_revision': 'master',
}

deps = {
  'source': '{chromium_git}/external/github.com/open-source-parsers/jsoncpp.git@{jsoncpp_revision}'
}
