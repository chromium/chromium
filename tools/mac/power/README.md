# Mac power measurement

This directory contains the tools necessary to run different browsers and usage scenarios and measuring their
performance impact. For Chromium it's also possible to generate profiles of CPU use. See `benchmark.py` for
running the full suite. See `collapse_profile.py` for generating profiles.


## Setting Up

### Python Virtual Environment
These scripts use python [Virtual Environments](https://docs.python.org/3/tutorial/venv.html).

Create the venv. Only needs to be done once.
```
python3 -m venv ./env
```
Or using a specific python binary, e.g. from depot_tools
```
~/src/chromium/depot_tools/python-bin/python3 -m venv ./env
```
Activate the venv.
```
source ./env/bin/activate
```
Once the venv is activated, `python` refers to python3.
Upgrade pip and install all python dependencies.
```
python -m pip install -U pip
python -m pip install -r requirements.txt
```

To deactivate venv.
```
deactivate
```

### Chromium build

Chromium needs to be built with the following args.gn 

    is_debug = false
    is_component_build = false
    symbol_level = 0
    blink_symbol_level = 0
    is_official_build = true
    is_chrome_branded = true

If profiling it needs to be built with these additional args.gn 

    enable_profiling = true
    enable_dsyms = false
    enable_stripping = false

If tracing it needs to be built with these additional args.gn 

    extended_tracing_enabled = true

In all cases the build needs to be copied to the "Applications" folder. 
If desired it's possible to build using components builds and dsyms but it
adds the complexity of having to copy over the additional files to the test
machine when building from a different machine.

## Getting around sudo password

To disable asking password for sudo commands (required by powermetrics).
Run `sudo visudo` and add the last line to User specification (replacing `<user>`):
```
# root and users in group wheel can run anything on any machine as any user
root ALL = (ALL) ALL
%admin ALL = (ALL) ALL
<user> ALL = (ALL) NOPASSWD:ALL
```

## power_sampler

A compiled binary of power_sampler is required to run power measurements. It can be compiled using this command:
`autoninja -C out/Release tools/mac/power:power_sampler`


## DTrace

Running benchmark.py in profile mode uses `dtrace` to analyse the chromium processes. By default `dtrace` does not work well with [SIP](https://support.apple.com/en-us/HT204899). Disabling SIP as a whole is not recommended and instead should be done only for dtrace using these steps:

* Reboot in recovery mode
* Start a shell
* Execute `csrutil enable --without dtrace --without debug`
* Reboot

## benchmark.py

A tool that allow you to run different browsers under specific usage scenarios and:

* Measure their impact of system resource use.
* Profile the code that runs and/or is causing wake-ups. (chromium only)

```
./benchmark.py --scenarios idle_on_wiki:chrome idle_on_wiki:safari --power_sampler=/path/to/power_sampler
./benchmark.py --profile_mode cpu_time --scenarios idle_on_wiki:chromium --power_sampler=/path/to/power_sampler
```

## Comparing benchmark executions

To compare the results of two benchmark runs, first generate a `summary.csv` file for each execution using the `analyze.py` tool. Assuming the baseline benchmark was output to `/tools/mac/power/output/<baseline_timestamp>/<scenario_name>` and the alternative was output to `/tools/mac/power/output/<alternative_timestamp>/<scenario_name>`:

```
./analyze.py --data_dir=/tools/mac/power/output/<baseline_timestamp>
./analyze.py --data_dir=/tools/mac/power/output/<alternative_timestamp>
```

Will create a `summary.csv` file in each of these directories. These can be passed to `compare.py`:

```
./compare.py --output_dir=/path/to/output/folder/ --baseline_dir=/tools/mac/power/output/<baseline_timestamp>/<scenario_name> --alternative_dir=/tools/mac/power/output/<alternative_timestamp>/<scenario_name>
```

Will create a `/path/to/output/folder/comparison_summary.csv` file containing information about the difference in metrics between both executions.

## export_dtrace.py

A tool that converts the DTrace results created by benchmark.py into a format
suitable for FlameGraph generation and analysis.

```
./export_dtrace.py --data_dir ./output/idle_on_wiki_chromium_dtraces_cpu_time --output ./output/idle_on_wiki_cpu_profile.pb
```

This command will produce a file at `./output/idle_on_wiki_cpu_profile.pb`.

The script can produce a pprof profile that can be used with
[pprof](https://github.com/google/pprof) or a collapsed profile that can be used
with tools such as [FlameGraph](https://github.com/brendangregg/FlameGraph) and
[SpeedScope](https://www.speedscope.app/).

Known signatures are added to the pprof profile as labels. These can be
appended to the flamegraph with

```
pprof -proto -tagroot signature <profile>
```

## Usage scenario scripts

This directory contains the necessary files to make Chromium based browsers and Safari
execute usage scenarios that represent real world usage.

The scripts differ slightly from similar tools like telemetry tests in that they work for Safari.

### Scenarios
Scenarios are a set of operations to be applied on a set of URLs.

For example:
* Navigate to google.com
* Open the first link
* Wait 60 seconds

It's interesting to gather power metrics, profiles and traces for specific
scenarios to understand their performance characteristics.

Currently supported scenarios are:
* `idle`
* `meet`
* `idle_on_wiki`
* `idle_on_youtube`
* `navigation_top_sites`
* `navigation_heavy_sites`
* `zero_window`

# Tests

Unit tests can be run using `run_tests.py`.
