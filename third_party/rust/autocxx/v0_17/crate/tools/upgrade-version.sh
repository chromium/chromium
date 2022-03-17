#!/bin/bash

OLD=$1
NEW=$2
find . -type f -name "Cargo.toml" -print0 | xargs -0 sed -i '' -e "s/$OLD/$NEW/g"
find . -type f -name "*.md" -print0 | xargs -0 sed -i '' -e "s/$OLD/$NEW/g"
