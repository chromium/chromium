# This isn't a real DEPS file.
# Undo things in third_party/DEPS that we don't want.
include_rules = [
  # Internal includes.
  '+glog',
  '+prng',
  '+testing',
  # Base test_runner includes.
  '+base/functional/bind.h',
  '+base/test',
  # Hooking up internal logging references.
  '+base/logging.h',
  # The lib will be able to use abseil, but without being imported directly in
  # chromium.
  '+absl',
]
