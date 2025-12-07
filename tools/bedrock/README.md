# Bedrock Tools and Utilities

## Overview

This directory contains helper scripts and utilities for Project Bedrock
(Browser modularization). For example these may be utilities for generating
project metrics, or tools to assist prompt-driven test refactors.

## bedrock_metrics.py

`bedrock_metrics.py` is a script that generates a JSON file with metrics
specific to the Bedrock project.

### Usage
```sh
usage: Generates project metrics from the src and build directories.

  $ tools/bedrock/bedrock_metrics.py [src_dir] [build_dir] [output_filepath]
```

### Tests
The `./bedrock_metrics_tests.py` runs a simple test suite leveraging python's
built-in unittest framework.
