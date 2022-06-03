# AVD config files
This directory contains textpb files used as AVD config files by the script
`tools/android/avd/avd.py`
1. textpb files under `./creation` are for `avd.py create`.
2. The ones **directly** under this directory are for `avd.py install`
and `avd.py start`.

## Config files for `avd.py create`
These files are normally used only by the builder
[android-avd-pacakager](https://ci.chromium.org/p/chromium/builders/ci/android-avd-packager).
It is not expected for end users to use them.

## Config files for `avd.py install` and `avd.py start`
When updating these files, please make sure the versions of emulator
and system image are the **same** as the tag values in the to-be-updated
AVD CIPD package. Failure to do so will cause the AVD
[quick boot](https://android-developers.googleblog.com/2017/12/quick-boot-top-features-in-android.html)
feature to fail to work properly.
