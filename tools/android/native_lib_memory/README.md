This directory contains scripts used to assess memory usage on Android, and in
particular the memory cost of Chromium's executable code. This is related to the
`tools/cygprofile` directory.

# How to use

## Prerequisites
Most (if not all) of these tools require ADB to run as root on the
device. Android builds allowing this mode would need this command to execute:

```bash
$ adb root
```

Which must be run beforehand. In some cases, SELinux must be put in permissive
mode, with the following command:

```bash
$ adb shell setenforce 0
```

## Display Proportional Set Size (PSS) for all processes: `code_pages_pss.py`
Shows the Proportional Set Size of code pages for all processes of a given
Chrome instance. Example for a local Chrome build:

```bash
$ tools/android/native_lib_memory/code_pages_pss.py \
    --app-package com.google.android.apps.chrome \
    --chrome-package com.google.android.apps.chrome

INFO:root:Processes:
        com.google.android.apps.chrome_zygote
        com.google.android.apps.chrome
        com.google.android.apps.chrome:sandboxed_process0:org.chromium.content.app.SandboxedProcessService0:0
        com.google.android.apps.chrome:privileged_process0
Total PSS from code pages = 30306kB
```

## Visualize code ordering: `extract_symbols.py`
This is used to visualize the impact of code ordering on a running Chrome instance.

### Prerequisites
An official Chrome build, in order for ThinLTO to be enabled. In args.gn, you need:

```
is_debug = false
is_component_build = false
is_official_build = true
symbol_level = 1
```

And to build a Chrome target, not Chromium, in order to get code ordering,
e.g. `monochrome_apk`. Then, you can either use it purely to visualize code
layout, or to couple it with residency.

### No residency
Run:

```bash
$ tools/android/native_lib_memory/extract_symbols.py \
  --native-library libmonochrome.so \
  --build-directory out/Release \
  --output-directory /tmp/output \
  --start-server
```

Will start an HTTP server on port 8000 by default, and the results are at
<http://127.0.0.1:8000/visualize.html>.

### With residency data

To get residency data as well, you need to:

1. Add `--log-native-library-residency` to Chrome's command line
2. Start Chrome, opening any site
3. Trace the device remotely, and trigger a memory dump with tracing
4. Pull the data from the local device

To add the command line flag, and ensure that the right directory exists on the device:

```bash
$ adb shell "echo '_ --log-native-library-residency' > /data/local/tmp/chrome-command-line"
$ adb shell mkdir /data/local/tmp/chrome
```

To trace the device remotely, open <chrome://inspect/?tracing#devices>. Select
the `memory-infra` category, and wait for a dump to complete. This requires a
device running at least a 4.4 kernel.

To pull the file from the device and convert it to JSON:

```bash
$ tools/android/native_lib_memory/extract_resident_pages.py \
  --device-serial <DEVICE_SERIAL> \
  --on-device-file-path /data/local/tmp/chrome/native-library-resident-pages.txt \
  --output-directory .
```

Finally, the command to process the data is amended to:
```bash
$ tools/android/native_lib_memory/extract_symbols.py \
  --native-library libmonochrome.so \
  --build-directory out/Release \
  --output-directory /tmp/output \
  --residency residency.json \
  --start-server
```
