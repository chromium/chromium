#!/bin/bash

set -x  # print commands as they are executed
set -e  # fail and exit on any command erroring

./oss_scripts/configure.sh
bazel test --test_output=errors tensorflow_text:all
