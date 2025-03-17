#!/bin/bash
cd $(dirname "$0")
mkdir tmp
../jdk/current/bin/javac \
    -d tmp \
    --add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED \
    --add-exports=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED \
    --add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED \
    --add-exports jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED \
    --add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED \
    --add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED \
    --add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED \
    --add-exports=jdk.internal.opt/jdk.internal.opt=ALL-UNNAMED \
    -cp cipd/google-java-format.jar \
    -source 11 \
    -target 11 \
    local_modifications/java/com/google/googlejavaformat/java/*.java
rm -f chromium-overrides.jar
(cd tmp && zip -rD0 ../chromium-overrides.jar *)
rm -r tmp
