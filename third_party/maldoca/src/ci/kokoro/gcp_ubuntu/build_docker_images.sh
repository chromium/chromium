#!/bin/bash

# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script should be run at the root of maldoca repo.

# Fail on any error.
set -e

# Print all commands.
set -x

# Clone all submodules.
git submodule update --init --recursive

# Build Docker image "maldoca", which installs all the required packages.
docker build --file docker/Dockerfile --tag maldoca .

# Build Docker image "maldoca-with-repo", which is based on "maldoca" but also
# copies the repository in /maldoca in the guest file system.
docker build --file docker/Dockerfile.maldoca-with-repo --tag maldoca-with-repo .
