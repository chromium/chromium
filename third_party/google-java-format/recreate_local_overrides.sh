#!/bin/bash
set -x
set -e
cd $(dirname "$0")
diff -r -u originals local_modifications > changes.patch && true
for f in $(cd originals; find java -name "*.java"); do
  echo "https://raw.githubusercontent.com/google/google-java-format/refs/heads/master/core/src/main/$f"
  curl -o originals/$f "https://raw.githubusercontent.com/google/google-java-format/refs/heads/master/core/src/main/$f"
  cp originals/$f local_modifications/$f
done
(cd local_modifications; patch -p1 < ../changes.patch)
