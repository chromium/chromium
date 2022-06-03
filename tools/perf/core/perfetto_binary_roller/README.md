<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Perfetto Binary Roller

Results Processor needs some pre-built binaries from Perfetto project, e.g.,
`trace_processor_shell` for converting proto traces and computing TBMv3 metrics.
These binaries can be either built from Chromium checkout or downloaded from
the cloud. This module provides scripts for updating the binary stored in the
cloud.

The update process works as follows:
- CI bots build the binary for all telemetry host platforms (win/lin/mac) and
call `upload_trace_processor` script.
- Someone calls `roll_trace_processor` script locally to update the TP version
in use to the latest version uploaded by the CI.

Please see [this doc](https://docs.google.com/document/d/1b-0Z7HTsAwuqPlWnyy3bW-NSdaRO-clUntW8bYkKxAM)
for discussion and details.
