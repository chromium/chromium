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

# Fail on any error.
set -e

# Print all commands.
set -x

# Code under repo is checked out to ${KOKORO_ARTIFACTS_DIR}/github.
cd ${KOKORO_ARTIFACTS_DIR}/github/maldoca

# Build Docker images.
./ci/kokoro/gcp_ubuntu/build_docker_images.sh

# Run ci/kokoro/gcp_ubuntu/cc_coverage/cc_coverage.sh with pwd=/maldoca in the
# container.
#
# We have to specify --security-opt="seccomp=unconfined" and --privileged,
# because otherwise sandboxed-api fails.
docker run \
  --security-opt="seccomp=unconfined" \
  --privileged \
  --workdir="/maldoca" \
  maldoca-with-repo \
  ci/kokoro/gcp_ubuntu/cc_coverage/cc_coverage.sh
