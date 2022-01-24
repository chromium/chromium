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

If measuring of profiling Chromium it needs to be built with the following args.gn and copied to the "Applications" folder.

    use_goma = true
    is_debug = false
    is_component_build = false
    symbol_level = 0
    blink_symbol_level = 0
    is_official_build = true

## Getting around sudo password

To disable asking password for sudo commands (required by powermetrics).
Run `sudo visudo` and add the last line to User specification (replacing `<user>`):
```
# root and users in group wheel can run anything on any machine as any user
root ALL = (ALL) ALL
%admin ALL = (ALL) ALL
<user> ALL = (ALL) NOPASSWD:ALL
```

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
./benchmark.py ./results --measure 
./benchmark.py ./profile --profile_mode cpu_time
```

## collapse_profile.py

A tool that converts the DTrace results created by benchmark.py into a format suitable for 
FlameGraph generation and analysis. It also applies some Chromium specific filtering and enhancements.

```
./collapse_profile.py ./profile --profile_mode cpu_time
```

This command will produce a file at `./samples/samples.collapsed.cpu_time`.

This file can be used with tools such as:

* [FlameGraph](https://github.com/brendangregg/FlameGraph)
* [SpeedScope](https://www.speedscope.app/)

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

### Usage

First `generate_scripts.py` needs to be used to convert the templates in `driver_script_templates/` into
working AppleScript. The templating allows for the generation of scripts that work with many different
browsers in the same way without having to modify each file by hand which is error
prone.

Once generated the driver scripts are found in `driver_scripts/` and can be invoked directly like this:
```
osascript ./driver_scripts/chrome_navigation.scpt
```

Once the scenario has run its course the script will exit. If the desired the
browser can be opened by hand before running the scenario to modify the starting
state.

# Formats

Files in `driver_script_templates/` directory that do not end in .scpt are
jinja2 templates that need to be rendered into usable Applescript.

Files in `driver_script_templates/` that end in .scpt are already working
Applescript and will be copied as is to `driver_script/`.

# Tests

Unit tests can be run using `run_tests.py`.
