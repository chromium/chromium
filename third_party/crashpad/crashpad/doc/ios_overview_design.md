<!--
Copyright 2021 The Crashpad Authors

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

# iOS Crashpad Overview Design

[TOC]

## iOS Limitations

Crashpad on other platforms captures exceptions out-of-process. The iOS sandbox,
however, restricts applications from delegating work to separate processes.
This limitation means Crashpad on iOS must combine the work of the handler and
the client into the same process as the main application.

## The Crashpad In-Process Handler

In-process handling comes with a number of limitations and difficulties. It is
not possible to catch the specific Mach exception `EXC_CRASH`, so certain groups
of crashes cannot be captured. This includes some major ones, like out-of-memory
crashes. This also introduces difficulties in capturing all the relevant crash
data and writing the minidump, as the process itself is in an unsafe state.

While handling an exception, the handler may not, for example:

 - Allocate memory.
 - Use libc, or most any library call.

While handling an exception, the handler may only:

 - Use audited syscalls.
 - access memory via `vm_read`.

In conjunction with Crashpad’s existing minidump writer and structural
limitations of the minidump format, it is not possible to write a minidump
immediately from the crash handler. Instead, an intermediate dump is written
when a handler would normally write a minidump (such as during an exception or a
forced dump without crashing). The intermediate dump file will be converted to
a minidump on the next run (or when the application decides it's safe to do so).

During Crashpad initialization, the handler gathers basic system information
and opens a pending intermediate dump adjacent to the Crashpad database.

## The Crashpad IntermediateDump Format

Due to the limitations of in-process handling, an intermediate dump file is
written during exceptions. The data is streamed to a file, which will be used to
generate a final minidump when appropriate.

The file format is similar to binary JSON, supporting keyed properties, maps and
arrays.

 - `Property` [key:int, length:int, value:intarray]
 - `StartMap` [key:int], followed by repeating Properties until `EndMap`
 - `StartArray` [key:int], followed by repeating Maps until `EndArray`
 - `EndMap`, `EndArray`, `EndDocument`

Similar to JSON, maps can contain other maps, arrays and properties.

## The life of an iOS crash report

Immediately upon calling StartCrashpadInProcessHandler, the iOS in-process
handler is installed. This will open a temporary file within the database
directory, in a subdirectory named `pending-serialized-ios-dump`. This file will
be used to write an intermediate dump in the event of a crash. This must happen
before installing the various types of crash handlers, as each depends on having
a valid handler with an intermediate dump ready to be written to.

After the in-process handler is initialized, the Mach exception, POSIX signal
and Objective-C exception preprocessor handlers are installed.

### Intermediate Dump File Locking

It is expected that multiple Crashpad clients may share the same database
directory, and this directory may be inside an iOS app group directory. While
it's possible for each Crashpad client to write to its own private directory,
if a shared directory is used, it's possible for different applications to
upload a crash report from any application in a shared group. This might be
used, for example, by an application and its various app extensions, where each
client may generate a crash report but only the main application uploads
reports. Alternatively, a suite of applications may upload each other's crash
reports. Otherwise, the only opportunity to upload a report would be when a
specific app that crashed relaunches.

To prevent multiple clients from processing a pending intermediate dump, files
must be locked. However, POSIX locks on app group files will trigger app
termination on app backgrounding, so a custom file locking protocol is used.
Locked temporary files are named `<bundle-id>@<uuid>.locked`. The `.locked`
extension is removed when the file is unlocked. The `bundle-id` is used to
determine which Crashpad clients can process leftover locked files.

### Writing Crashes to Intermediate Dumps

When an app encounters a crash (via a Mach exception, Objective-C exception, or
a POSIX signal), an intermediate dump is written to the temporary locked file,
the .locked extension is removed, and a new temporary locked file is opened.

App terminations not handled by Crashpad will leave behind a temporary
locked file, to be cleaned up on next launch. These files are still processed,
because it is possible for the app to be terminated while writing an
intermediate dump, and if enough data is written this may still be valuable.

Note: Generally iOS apps are single-process, so it's safe for the client to
consider any files matching its `bundle-id`, but there are edge-cases (such as
if a share-to app extension is opened at the same time in two different apps) so
old locked files won't be cleared until after 24 hours. Any locked file found
after 60 days is unlocked regardless of `bundle-id`.

### Writing to Intermediate Dumps without a Crash

Apps may also generate intermediate dumps without a crash, often used for
debugging. Chromium makes heavy use of this for detecting main thread hangs,
something that can appear as a crash for the user, but is uncatchable for crash
handlers like Crashpad. When an app requests this (via DumpWithoutCrash,
DumpWithoutCrashAndDeferProcessing), an intermediate dump is written to the
temporary locked file, the .locked extension is removed, and a new temporary
locked file is opened.

Note: DumpWithoutCrashAndDeferProcessingAtPath writes an intermediate dump to
the requested location, not the previously opened temporary file. This is useful
because Chromium's main thread hang detection will throw away hang reports in
certain circumstances (if the app recovers, if a different crash report is
written, etc).

## The Crashpad In-Process Client

Other Crashpad platforms handle exceptions and upload minidumps out-of-process.
On iOS, everything must happen in-process. Once started, the client will
automatically handle exceptions and capture the crashed process state in an
intermediate dump file. Converting that intermediate dump file into a minidump
is likely not safe to do from within a crashed process, and uploading a minidump
is definitely unsafe to do at crash time. Applications are expected to process
intermediate dumps into pending minidumps and begin processing pending
minidumps, possibly for upload, at suitable times following the next application
restart.

Note: Applications are not required to call either of these methods. For
example, application extensions may choose to generate dumps but leave
processing and uploading to the main applications. Clients that share the
same database directory between apps can take advantage of processing and
uploading crash reports from different applications.

### `ProcessIntermediateDumps`
For performance and stability reasons applications may choose the correct time
to convert intermediate dumps, as well as append metadata to the pending
intermediate dumps. This is expected to happen during application startup, when
suitable. After converting, a minidump will be written to the Crashpad database,
similar to how other platforms write a minidump on exception handling. If
uploading is enabled, this minidump will also be immediately uploaded. New
intermediate dumps generated by exceptions or by
`CRASHPAD_SIMULATE_CRASH_AND_DEFER_PROCESSING` will not be processed until
the next call to `ProcessIntermediateDumps`. Conversely,
`CRASHPAD_SIMULATE_CRASH` can be called when the client has no performance or
stability concerns. In this case, intermediate dumps are automatically
converted to minidumps and immediately eligible for uploading.

Applications can include annotations here as well. Chromium uses this for its
insta-crash logic, which detects if an app is crashing repeatedly on startup.

### `StartProcessingPendingReports`
For similar reasons, applications may choose the correct time to begin uploading
pending reports, such as when ideal network conditions exist. By default,
clients start with uploading disabled. Applications should call this API when
it is determined that it is appropriate to do so (such as on a few seconds after
startup, or when network connectivity is appropriate).
