#!/bin/bash

# This script generates BUILD.gn using template/BUILD.chromium.gn.template and
# the gRPC repo in src/

set -e

# change directory to this script's directory
cd `dirname "$0"`

# copy template into grpc repo and run generate_projects in it
cp template/BUILD.chromium.gn.template src/templates/BUILD.chromium.gn.template
cd src
./tools/buildgen/generate_projects.sh
rm templates/BUILD.chromium.gn.template # clean up
cd ..

# move the generated GN file back to this directory
mv src/BUILD.chromium.gn BUILD.gn

gn format --inplace BUILD.gn

