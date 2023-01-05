# Chrome Android Dependency Analysis Tool

## Overview

As part of Chrome Modularization, this directory contains various tools for
analyzing the dependencies contained within the Chrome Android project.

If you'd like to just view the graph, the simplest way is to use the script:

```
tools/android/dependency_analysis/start_server.sh
```

## Usage

Start by generating a JSON dependency file with a snapshot of the dependencies
for your JAR files using the **JSON dependency generator** command-line tool.

This snapshot file can then be used as input for various other analysis tools
listed below.

See the visualization [js/README.md](js/README.md) for details on how to start a
local server to view your local dependency graphs.

## Command-line tools

The usage information for any of the following tools is also accessible via
`toolname -h` or `toolname --help`.

#### JSON Dependency Generator

Runs [jdeps](https://docs.oracle.com/javase/8/docs/technotes/tools/unix/jdeps.html)
(dependency analysis tool) on all JARs a root build target depends
on and writes the resulting dependency graph into a JSON file. The default
root build target is chrome/android:monochrome_public_bundle.

```
usage: generate_json_dependency_graph.py [-h] -o OUTPUT [-C BUILD_OUTPUT_DIR]
                                         [-p PREFIX] [-t TARGET]
                                         [-d CHECKOUT_DIR] [-j JDEPS_PATH]
                                         [-g GN_PATH] [--skip-rebuild]
                                         [--show-ninja] [-v]

Runs jdeps (dependency analysis tool) on all JARs and writes the resulting
dependency graph into a JSON file.
options:
  -h, --help            show this help message and exit
  -C BUILD_OUTPUT_DIR, --build_output_dir BUILD_OUTPUT_DIR
                        Build output directory, will attempt to guess if not
                        provided.
  -p PREFIX, --prefix PREFIX
                        If any package prefixes are passed, these will be used
                        to filter classes so that only classes with a package
                        matching one of the prefixes are kept in the graph. By
                        default no filtering is performed.
  -t TARGET, --target TARGET
                        If a specific target is specified, only transitive
                        deps of that target are included in the graph. By
                        default all known javac jars are included.
  -d CHECKOUT_DIR, --checkout-dir CHECKOUT_DIR
                        Path to the chromium checkout directory. By default
                        the checkout containing this script is used.
  -j JDEPS_PATH, --jdeps-path JDEPS_PATH
                        Path to the jdeps executable.
  -g GN_PATH, --gn-path GN_PATH
                        Path to the gn executable.
  --skip-rebuild        Skip rebuilding, useful on bots where compile is a
                        separate step right before running this script.
  --show-ninja          Used to show ninja output.
  -v, --verbose         Used to display detailed logging.

required arguments:
  -o OUTPUT, --output OUTPUT
                        Path to the file to write JSON output to. Will be
                        created if it does not yet exist and overwrite
                        existing content if it does.
```

#### Class Dependency Audit

Given a JSON dependency graph, output the class-level dependencies for a given
list of classes.

An example is given at the end of this page. To see the options:

```
tools/android/dependency_analysis/print_class_dependencies.py -h
```

#### Package Dependency Audit

Given a JSON dependency graph, output the package-level dependencies for a
given package and the class dependencies comprising those dependencies.

An example is given at the end of this page. To see the options:

```
tools/android/dependency_analysis/print_package_dependencies.py -h
```

#### Package Cycle Counting

Given a JSON dependency graph, counts package cycles up to a given size.

To see the options:

```
tools/android/dependency_analysis/count_cycles.py -h
```

## Example Usage

This Linux example assumes Chromium is contained in a directory `~/cr`
and that Chromium has been built as per the instructions
[here](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md),
although the only things these assumptions affect are the file paths.

```
$ tools/android/dependency_analysis/generate_json_dependency_graph.py -C out/Debug -o ~/graph.json
>>> Running jdeps and parsing output...
>>> Parsed class-level dependency graph, got 3239 nodes and 19272 edges.
>>> Created package-level dependency graph, got 500 nodes and 4954 edges.
>>> Dumping JSON representation to ~/graph.json.

tools/android/dependency_analysis/print_class_dependencies.py -f ~/graph.json -c AppHooks
>>> Printing class dependencies for org.chromium.chrome.browser.AppHooks:
>>> 35 inbound dependency(ies) for org.chromium.chrome.browser.AppHooks:
>>> 	org.chromium.chrome.browser.AppHooksImpl
>>> 	org.chromium.chrome.browser.ChromeActivity
>>> ...

tools/android/dependency_analysis/print_package_dependencies.py -f ~/graph.json -p chrome.browser
>>> Printing package dependencies for org.chromium.chrome.browser:
>>> 121 inbound dependency(ies) for org.chromium.chrome.browser:
>>> 	org.chromium.chrome.browser.about_settings -> org.chromium.chrome.browser
>>> 	1 class edge(s) comprising the dependency:
>>> 		AboutChromeSettings -> ChromeVersionInfo
>>> ...
```
