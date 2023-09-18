#!/bin/bash
cd $(dirname "$0")
../jdk/current/bin/javac \
    -d tmp \
    --add-exports jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED \
    -cp google-java-format.jar \
    local_modifications/java/com/google/googlejavaformat/java/*.java
rm chromium-overrides.jar
(cd tmp && zip -rD0 ../chromium-overrides.jar *)
rm -r tmp
