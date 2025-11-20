#!/bin/sh
set -ex
docker buildx build --progress=plain --tag libdmg-hfsplus "$(dirname "$0")/.."
docker run libdmg-hfsplus
