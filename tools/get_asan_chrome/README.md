# Download ASAN Chrome

This directory contains a helper script to download ASAN builds of Chromium.
## Usage

Several command line flags are provided if you want more control over what build
is downloaded. The `--help` flag provides detailed usage information. Read on
for usage examples.

### Get the latest build

By default, the script detects the operating system of the system on which the
script is run and then downloads the most recent ASAN build for that operating
system.

```sh
vpython3 get_asan_chrome.py
```

This mode is useful when you want the most recent ASAN build for whatever
platform the script runs on, which is typically the case when fuzzing and also
when attempting to reproduce a bug report near tip-of-tree.

### Get the latest ASAN build in a specific channel

Sometimes you need the latest ASAN build from a specific channel:

```sh
vpython3 get_asan_chrome.py --channel {canary,dev,beta,stable}
```

Note that not all platforms have all channels. For example, there is no Canary
channel for Linux, so use `dev` instead.

### Get a specific version or branch position

It can also be useful to get an ASAN build for a specific release:

```sh
vpython3 get_asan_chrome.py --version 105.0.5191.2
```

### Override OS detection

You can also download a build for an operating system that is different than
where the script runs:

```sh
vpython3 get_asan_chrome.py --os {linux,mac,win64,lacros}
```

### Combine

Finally, if you need greater control then some flags can be used together:

```sh
vpython3 get_asan_chrome.py --os win64 --version 105.0.5191.2
```
