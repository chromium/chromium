# Bisect tool

bisect_builds.py is a script to use pre-built binaries to bisect Chrome and
Chromium.

bisect_gtests.py is a script to help bisect gtest failures.

The following documents are all for bisect_builds.py.

## Binary sources
There are 3 binary sources:
1.  Release build(-r)

This is closest to the binary we release to users.
  * Almost always able to repro production bugs.
  * Bisect to a Chrome version. Blamelist contains ~12 hours of CLs.
  * Binaries are stored forever. You can bisect range can be years old.
  * Most platforms are supported.
  * Google employees only.

2.  Official build(-o)

This is the Chrome build with is_branded_build=true.
*  Highly likely able to repro production bugs.
*  Bisect to a single commit.
*  Binaries are stored ~1 month. So only recent regressions can use this.
*  Most platforms are supported.
*  Google employees only.

3.  Snapshot

The default option. This is public Chromium build.
*  Lowest repro possibilities among the 3 sources.
*  Bisect to a range of blamelists. Depends on the platform, roughly around 1
   hour to 2 hours of CLs.
*  Binaries are stored for ~1 year.
*  Not all platforms are supported.
*  External developers available.

## Quick start
For Chromium developers who can already build Chromium:

`$ python3 tools/bisect/bisect_builds.py -b 126.0.6436.0
 -g 126.0.6435.0 -a linux64`

For everyone else:

1.  follow
[instructions](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)
to download depot_tools.

Enter depot tools folder:

`$ cd depot_tools`

2.  For Googlers, you need to sign in gcloud to access internal buckets.

`depot_tools$ python3 gsutil.py config'

Add `sudo` if you hit permission issues.

Proceed by following the instructions (you can pass in 0 for project id),
use google.com account.

3.  Use this commandline to download the script to the depot_tools folder.

`depot_tools$ curl -s --basic -n
"https://chromium.googlesource.com/chromium/src/+/main/tools/bisect/bisect_builds.py?format=TEXT"
| base64 -d > bisect_builds.py`

4.  Start bisection

`depot_tools$ python3 bisect_builds.py -b 126.0.6436.0
 -g 126.0.6435.0 -a linux64`

## Tips
* For Googlers, you should use official build for most cases to get to a single
commit. If official build doesn't work for you, please file a bug.

* If a regression happens to both x64 and arm, using x64 would have a higher
chance to bisect to a single commit for official build.

## Modify the script
Please don't add dependencies. There are people who need to only checkout the
script file and do bisection.

## Support
For feature requests and bugs, please file a trooper bug.
