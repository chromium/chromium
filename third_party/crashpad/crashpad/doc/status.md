<!--
Copyright 2015 The Crashpad Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Project Status

## Completed

Crashpad has complete crash-reporting clients and some related tools for macOS,
Windows, Fuchsia, and Linux (including Android and Chromium OS). Crashpad became
the crash reporter client for [Chromium](https://www.chromium.org/Home) on macOS
as of [March
2015](https://chromium.googlesource.com/chromium/src/\+/d413b2dcb54d523811d386f1ff4084f677a6d089),
Windows as of [November
2015](https://chromium.googlesource.com/chromium/src/\+/cfa5b01bb1d06bf96967bd37e21a44752801948c),
and Android as of [January
2019](https://chromium.googlesource.com/chromium/src/+/f890e4b5495ab693d2d37aec3c378239946154f7).


## In Progress

Chromium is transitioning to Crashpad for [Chromium OS and Desktop
Linux](https://crbug.com/942279).

Work has begun on a Crashpad client for
[iOS](https://crashpad.chromium.org/bug/31).

## Future

There are also plans to implement a [crash report
processor](https://crashpad.chromium.org/bug/29) as part of Crashpad. No
timeline for completing this work has been set yet.
