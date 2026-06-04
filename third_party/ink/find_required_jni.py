#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to analyze and identify JNI usage in androidx.ink libraries.

This script helps to shrink the native C++ build footprint for
`androidx.ink` in Chromium by identifying which JNI classes and methods are
actually reachable from the root PDF Ink library (`pdf-ink.aar`).

Use it to know what files can be omitted from BUILD.gn srcs.

It performs the following steps:
1. Discovers all `androidx_ink_*` AARs in the prebuilts directory.
2. Extracts `classes.jar` from each discovered AAR and the root
   `pdf-ink.aar` to a temporary directory.
3. Runs `jdeps` recursively to find all transitive class-level
   dependencies on `androidx.ink` package starting from the
   `androidx.pdf.ink` package.
4. Runs `javap` on each reachable `androidx.ink` class to check if it
   contains any `native` methods.
5. Outputs the list of reachable JNI classes and their native methods.

Usage: third_party/ink/find_required_jni.py
"""

import argparse
import glob
import os
import re
import subprocess
import tempfile
import zipfile

# Root AAR that uses Ink
ROOT_AAR_REL_PATH = "androidx_pdf_pdf_ink/pdf-ink.aar"

ANDROID_INK_RE = re.compile(r"->\s+(androidx\.ink\.\S+)")


def discover_ink_aars(androidx_dir):
    """Discovers androidx.ink AARs in the given directory."""
    ink_aars = {}
    pattern = os.path.join(androidx_dir, "androidx_ink_*", "*.aar")
    for aar_path in glob.glob(pattern):
        file = os.path.basename(aar_path)
        name = os.path.splitext(file)[0]
        ink_aars[name] = aar_path
    return ink_aars


def extract_classes_jar(aar_path, dest_jar_path):
    """Extracts classes.jar from an AAR file."""
    if not os.path.exists(aar_path):
        raise FileNotFoundError(f"AAR not found: {aar_path}")
    with zipfile.ZipFile(aar_path, 'r') as z:
        with z.open('classes.jar') as jar_file:
            with open(dest_jar_path, 'wb') as out_file:
                out_file.write(jar_file.read())


def main():
    parser = argparse.ArgumentParser(
        description="Find required JNI classes for PDF Ink.")
    parser.add_argument("--androidx-dir",
                        default="third_party/androidx/cipd/libs",
                        help="Path to third_party/androidx/cipd/libs")
    parser.add_argument("--jdeps-path",
                        default="jdeps",
                        help="Path to jdeps executable")
    parser.add_argument("--javap-path",
                        default="javap",
                        help="Path to javap executable")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as temp_dir:
        ink_aars = discover_ink_aars(args.androidx_dir)
        if not ink_aars:
            print("No androidx.ink AARs discovered!")
            return

        pdf_ink_aar_path = os.path.join(args.androidx_dir, ROOT_AAR_REL_PATH)
        pdf_ink_jar = os.path.join(temp_dir, "pdf-ink.jar")
        try:
            extract_classes_jar(pdf_ink_aar_path, pdf_ink_jar)
        except Exception as e:
            print(f"Error extracting root AAR {pdf_ink_aar_path}: {e}")
            return

        extracted_jars = {}
        for name, aar_path in ink_aars.items():
            dest_jar = os.path.join(temp_dir, f"{name}.jar")
            try:
                extract_classes_jar(aar_path, dest_jar)
                extracted_jars[name] = dest_jar
            except Exception as e:
                print(f"Warning: Error extracting {name} from {aar_path}: {e}")

        classpath = ":".join(extracted_jars.values())

        # Run jdeps to find transitive dependencies recursively
        cmd = [
            args.jdeps_path,
            "-R",
            "-verbose:class",
            "-cp",
            classpath,
            pdf_ink_jar,
        ]

        print(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"jdeps failed with exit code {result.returncode}")
            print(result.stderr)
            return

        # Parse jdeps output to find reachable androidx.ink classes
        # Output format is usually:
        #   source_class -> target_class  [JAR]
        reachable_classes = set()
        for line in result.stdout.splitlines():
            # Look only for "androidx.ink".
            if m := ANDROID_INK_RE.search(line):
                # Strip potential inner class suffixes or trailing info
                class_name = m.group(1).split()[0]
                # Remove inner class part for javap lookup (we check the
                # outer class).
                outer_class = class_name.split("$")[0]
                reachable_classes.add(outer_class)

        print(f"\nReachable androidx.ink classes ({len(reachable_classes)}):")
        for c in sorted(reachable_classes):
            print(f"  {c}")

        # Run javap on classes to find native methods.
        jni_classes = set()
        for class_name in sorted(reachable_classes):
            # Find which JAR has this class
            class_path_in_jar = class_name.replace(".", "/") + ".class"
            found_jar = None
            all_jars = list(extracted_jars.values()) + [pdf_ink_jar]
            for jar_path in all_jars:
                with zipfile.ZipFile(jar_path, 'r') as z:
                    if class_path_in_jar in z.namelist():
                        found_jar = jar_path
                        break

            if not found_jar:
                # Might be an inner class that we simplified, or from
                # stdlib/android SDK.
                continue

            javap_cmd = [
                args.javap_path,
                "-private",
                "-cp",
                found_jar,
                class_name,
            ]

            javap_result = subprocess.run(javap_cmd,
                                          capture_output=True,
                                          text=True)
            if javap_result.returncode == 0:
                if "native " in javap_result.stdout:
                    print(f"\nClass {class_name} has JNI methods:")
                    jni_classes.add(class_name)
                    # Print native methods
                    for line in javap_result.stdout.splitlines():
                        if "native " in line:
                            print(f"  {line.strip()}")
            else:
                # Try with inner classes if it failed?
                # Sometimes outer class doesn't exist as a separate
                # file if it's just an interface/namespace, but
                # usually it does.
                pass

        print(f"\nSummary of reachable JNI classes ({len(jni_classes)}):")
        for c in sorted(jni_classes):
            print(f"  {c}")


if __name__ == "__main__":
    main()
