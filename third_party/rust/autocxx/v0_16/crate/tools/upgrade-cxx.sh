#!/bin/bash

# Just specify the final numbers (e.g. 42 for 1.0.42)
OLD=$1
NEW=$2
find . -type f -name "Cargo.toml" -print0 | xargs -0 sed -i -e "s/1.0.$OLD/1.0.$NEW/g" # cxx
find . -type f -name "Cargo.toml" -print0 | xargs -0 sed -i -e "s/0.7.$OLD/0.7.$NEW/g" # cxx-gen
