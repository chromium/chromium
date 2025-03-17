## Devil Util

This directory builds tools used by the
[device interaction layer](https://chromium.googlesource.com/catapult/+/HEAD/devil/README.md),
which is also known as devil.

The source for the tools are located here rather than inside devil since these
tools use chromium third_party sources that aren't available from catapult
(devil's project).

When updating the source files here, please make sure to also update the
[prebuilt binaries inside devil](https://chromium.googlesource.com/catapult/+/HEAD/devil/devil/devil_dependencies.json).


### More Comments Regarding Devil

Devil tries to be supported in both chromium checkouts and also standalone
catapult checkouts, because bits of catapult including devil were getting
slurped into the android SDK at one point.

The former can build devil_util from sources, and doing so lets you test out
changes to devil_util source files in chromium's CQ. However, the latter cannot
build devil_util from sources, so we fetch the prebuilt binaries from
Google Storage.

When you update the devil_util source files in this directory, you can update
the prebuilt binaries in devil by running
[this script](https://chromium.googlesource.com/catapult/+/HEAD/devil/devil/utils/update_dependencies.py).
