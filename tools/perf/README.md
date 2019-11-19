<!-- Copyright 2019 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Performance tools

This directory contains a variety of command line tools that can be used to run
benchmarks, interact with speed services, and manage performance waterfall
configurations.

Note you can also read the higher level [Chrome Speed][speed] documentation to
learn more about the team organization and, in particular, the top level view
of [How Chrome Measures Performance][chrome_perf_how].

[speed]: /docs/speed/README.md
[chrome_perf_how]: /docs/speed/how_does_chrome_measure_performance.md

## run_benchmark

This command allows running benchmarks defined in the chromium repository,
specifically in [tools/perf/benchmarks][benchmarks_dir]. If you need it,
documentation is available on how to [run benchmarks locally][run_locally]
and how to properly [set up your device][device_setup].

[benchmarks_dir]: https://cs.chromium.org/chromium/src/tools/perf/benchmarks/
[run_locally]: https://chromium.googlesource.com/catapult.git/+/HEAD/telemetry/docs/run_benchmarks_locally.md
[device_setup]: /docs/speed/benchmark/telemetry_device_setup.md

## update_wpr

A helper script to automate various tasks related to the update of
[Web Page Recordings][wpr] for our benchmarks. In can help creating new
recordings from live websites, replay those to make sure they
work, upload them to cloud storage, and finally send a CL to review with the
new recordings.

[wpr]: https://github.com/catapult-project/catapult/tree/master/web_page_replay_go

## pinpoint_cli

A command line interface to the [pinpoint][] service. Allows to create new
jobs, check the status of jobs, and fetch their measurements as csv files.

[pinpoint]: https://pinpoint-dot-chromeperf.appspot.com

## flakiness_cli

A command line interface to the [flakiness dashboard][].

[flakiness dashboard]: https://test-results.appspot.com/dashboards/flakiness_dashboard.html

## soundwave

Allows to fetch data from the [Chrome Performance Dashboard][chromeperf] and
stores it locally on a SQLite database for further analysis and processing.
It also allows defining [studies][], pre-sets of measurements a team is
interested in tracking, and uploads them to cloud storage to visualize with the
help of [Data Studio][]. This currently backs the [v8][v8_dashboard] and
[health][health_dashboard] dashboards.

[chromeperf]: https://chromeperf.appspot.com/
[studies]: https://cs.chromium.org/chromium/src/tools/perf/cli_tools/soundwave/studies/
[Data Studio]: https://datastudio.google.com/
[v8_dashboard]: https://datastudio.google.com/s/iNcXppkP3DI
[health_dashboard]: https://datastudio.google.com/s/jUXfKZXXfT8

## pinboard

Allows scheduling daily [pinpoint][] jobs to compare measurements with/without
a patch being applied. This is useful for teams developing a new feature behind
a flag, who wants to track the effects on performance as the development of
their feature progresses. Processed data for relevant measurements is uploaded
to cloud storage, where it can be read by [Data Studio][].
This also backs data displayed on the [v8][v8_dashboard] dashboard.
