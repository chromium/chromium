# Usage scenario scripts

This directory contains the necessary files to make Chromium based browsers and Safari
execute usage scenarios that represent real world usage.

The scripts differ slightly from similar tools like telemetry tests in that they work for Safari.

## Scenarios
Scenarios are a set of operations to be applied on a set of URLs.

For example:
* Navigate to google.com
* Open the first link
* Wait 60 seconds

It's interesting to gather power metrics, profiles and traces for specific
scenarios to understand their performance characteristics.

# Usage

First `generate_scripts.py` needs to be used to convert the templates in `driver_script_templates/` into
working AppleScript. The templating allows for the generation of scripts that work with many different
browsers in the same way without having to modify each file by hand which is error
prone.

Once generated the driver scripts are found in `driver_scripts/` and can be invoked directly like this:
```
osascript ./driver_scripts/chrome_navigation.scpt
```

Once the scenario has run it's course the script will exit. If the desired the
browser can be opened by hand before running the scenario to modify the starting
state.

# Formats

Files in `driver_script_templates/` directory that do not end in .scpt are
jinja2 templates that need to be rendered into usable Applescript.

Files in `driver_script_templates/` that end in .scpt are already working
Applescript and will be copied as is to `driver_script/`.
