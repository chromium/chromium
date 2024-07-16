# AVD config files
This directory contains textpb files used as AVD config files by the scripts
like `tools/android/avd/avd.py` and `build/android/test_runner.py`
1. Creation config files under `./proto_creation`.
1. Production config files under `./proto`.

## Naming scheme
As more image types (`google_apis`, `google_atd`, etc.) and cpu architectures
(`x86`, `x86_64`, etc.) are added, the following naming scheme is suggested to
use when adding new config files:
```
android_<API level>_<image type>_<cpu arch>[_<other info>].textpb
```

For simplicity, we do the following convention for the cpu architectures, from
the values in android sdk manager to those in the config file names.
- `armeabi-v7a` -> `arm`
- `arm64-v8a` -> `arm64`
- `x86` -> `x86`
- `x86_64` -> `x64`

So for a new config that uses API level 31, image type "google_apis", cpu arch
"x86_64", the file name would be `android_31_google_apis_x64.textpb`

For another example, a foldable AVD with landscape view can have the file name
`android_32_google_apis_x64_foldable_landscape.textpb`

## Creation config files
These files are normally used only by the builder
[android-avd-packager](https://ci.chromium.org/p/chromium/builders/ci/android-avd-packager).
It is not expected for end users to use them.

For developers who try to create new AVD packages through the above packager,
instead of triggering one build that re-packages every AVD configs which could
takes 3+ hours, the following steps can be used to package only certain configs.

- Ensure you have `depot_tools`([link][depot_tools_link]) checked out in your
  local machine.
- Create a json file say `android-avd.json` and fill in with the "properties"
  value from the android-avd-packager ([example][packager_properties_example]).
  Then remove the unwanted avd configs from the `avd_configs` field.

  Here is an example of a json file that packages only two AVD configs:
  ```
  {
      "$build/avd_packager": {
          "avd_configs": [
              "tools/android/avd/proto_creation/android_30_google_atd_x86.textpb",
              "tools/android/avd/proto_creation/android_30_google_atd_x64.textpb"
          ],
          "gclient_config": "chromium",
          "gclient_apply_config": ["android"]
      }
  }
  ```
- Call `bb` command to trigger a new build on android-avd-packager with the
  property file from the above step.
  ```
  bb add -p @android-avd.json chromium/ci/android-avd-packager
  ```

[depot_tools_link]: https://chromium.googlesource.com/chromium/tools/depot_tools/+/HEAD/README.md
[packager_properties_example]: https://source.chromium.org/chromium/chromium/src/+/main:infra/config/subprojects/chromium/ci/chromium.infra.star;drc=59cbaef70288b956b446139de6c5bc2030bbb277;l=123-157

## Production config files

These files can use by `./avd.py start` and `./avd.py install`, as well as the
android test runner `build/android/test_runner.py`, via the flag `--avd-config`.

When updating these files, please make sure the versions of emulator
and system image are the **same** as the tag values in the to-be-updated
AVD CIPD package. Failure to do so will cause the AVD
[quick boot](https://android-developers.googleblog.com/2017/12/quick-boot-top-features-in-android.html)
feature to fail to work properly.
