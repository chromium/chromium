#!/bin/bash

# This script generates BUILD.gn using template/BUILD.chromium.gn.template and
# the gRPC repo in src/

set -e

# change directory to this script's directory
chromium_grpc_path="$(realpath "$0" | xargs dirname)"
cd "${chromium_grpc_path}"

if [[ -d /tmp/grpc ]]; then
  rm -rf /tmp/grpc
fi

revision="$(grep -oP 'Revision: \K[0-9a-f]+' README.chromium)"
echo "Cloning grpc at ${revision}..."
git clone --recurse-submodules --revision="${revision}" \
  https://github.com/grpc/grpc /tmp/grpc


echo "Generating BUILD.gn..."
# copy template into grpc repo and run generate_projects in it
cp template/BUILD.chromium.gn.template \
  /tmp/grpc/templates/BUILD.chromium.gn.template
cd /tmp/grpc
./tools/buildgen/generate_projects.sh
rm templates/BUILD.chromium.gn.template # clean up

# move the generated GN file back to this directory
cd "${chromium_grpc_path}"
mv /tmp/grpc/BUILD.chromium.gn BUILD.gn

echo "Formatting..."
gn format --inplace BUILD.gn

echo "Done"

