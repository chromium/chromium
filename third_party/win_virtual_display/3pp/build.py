#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import argparse

# Windows 11, version 22H2 EWDK with Visual Studio Build Tools 17.1.5
EWDK_URL = "https://go.microsoft.com/fwlink/?linkid=2195661"
# SHA256 hash of the ISO that we expect.
EWDK_HASH = "887d484454c677db191bf444380a3059a20dd87ee56b1028b12fd8cb52b997f0"
# Script in the wdk to run to set up the build environment.
BUILD_ENV = "BuildEnv\\SetupBuildEnv.cmd"
# MSVC++ project file to build.
BUILD_FILE = "third_party\\win_virtual_display\\driver\\ChromiumVirtualDisplayDriver.vcxproj"


def get_ewdk_iso_path():
    return os.path.join(tempfile.gettempdir(), "ewdk.iso")


def get_ewdk_iso_extract_path():
    return os.path.join(tempfile.gettempdir(), "wdk")


def hash_file(file_path):
    """Returns SHA256 hash of a specified file."""
    sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        for b in iter(lambda: f.read(2048), b""):
            sha256.update(b)
    return sha256.hexdigest()


def fetch_and_mount_ewdk():
    """ Fetches the EWK ISO and mounts it as a drive.
    Returns the drive letter that it was mounted to (e.g. "D")."""
    iso_path = get_ewdk_iso_path()
    print(f"Downloading iso to: {iso_path}")
    if os.path.isfile(iso_path) == False:
        urllib.request.urlretrieve(EWDK_URL, iso_path)
    else:
        print(f"File exists. Skipping ISO download.")
    # Check the hash of the download so that unexpected changes are flagged.
    iso_hash = hash_file(iso_path)
    if iso_hash != EWDK_HASH:
        print(f"iso hash mismatch. Expected: {EWDK_HASH}. Got: {iso_hash}")
    cmd = [
        "powershell.exe",
        "-command",
        "Mount-DiskImage",
        "-ImagePath",
        iso_path,
        "|",
        "Get-Volume",
        "|ForEach-Object",
        "DriveLetter",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if (result.returncode!=0):
      raise Exception(f"Failed to mount ISO: {result.stdout}")
    output_drive = result.stdout.strip()
    if (len(output_drive)!=1):
      raise Exception(f"Failed to mount ISO: No drive letter obtained.")
    print(f"ISO mounted to drive letter: {output_drive}")
    return output_drive


def unmount_ewdk():
    """Unmounts the ISO from its drive letter."""
    iso_path = get_ewdk_iso_path()
    cmd = ["powershell.exe", "-command", "Dismount-DiskImage", iso_path]
    subprocess.run(cmd)


def build(ewdk_path, output_path):
    """Build the vcxproj using the specified ewdk path and output path."""
    build_env = os.path.join(ewdk_path, BUILD_ENV)
    build_path = tempfile.gettempdir() + "\\build\\"
    command = (f"{build_env} && msbuild {BUILD_FILE} /t:build "
              f"/property:Platform=x64 /p:OutDir={build_path}")
    cmd = ["cmd", "/c", command]
    result = subprocess.run(cmd, capture_output=True, text=True)
    print(result.stdout)
    if "Build succeeded" not in result.stdout:
        raise Exception("Build failed.")
    # Copy compilation output and test certificate files to the output path.
    print(f"Copying build output to {output_path}")
    shutil.copytree(
        os.path.join(build_path, "ChromiumVirtualDisplayDriver"),
        output_path,
        dirs_exist_ok=True,
    )
    shutil.copy(os.path.join(build_path, "ChromiumVirtualDisplayDriver.cer"),
                output_path)
    # Helps with TraceView and debugging on the bots but not strictly necessary.
    shutil.copy(os.path.join(build_path, "ChromiumVirtualDisplayDriver.pdb"),
            output_path)

def copy_drive(drive_letter, dest):
    """Copy the contents of the specified drive letter to the specified path"""
    src_path = drive_letter + ":\\"
    print(f"Copying directory {src_path} to {dest}")
    shutil.copytree(src_path, dest, dirs_exist_ok=True)

def main():
    parser = argparse.ArgumentParser()
    # Args passed by the 3pp recipe (See: recipes/recipe_modules/support_3pp/spec.proto).
    parser.add_argument('output_prefix')
    parser.add_argument('deps_prefix')
    # Some environments fail when executing binaries directly on a mounted disk.
    # This flag copies the contents to the local disk.
    parser.add_argument('-c', '--copy_ewdk', action='store_true')
    args = parser.parse_args()
    mounted_drive_letter = fetch_and_mount_ewdk()
    try:
        ewdk_path = mounted_drive_letter + ":\\"
        if (args.copy_ewdk):
          ewdk_path = get_ewdk_iso_extract_path()
          copy_drive(mounted_drive_letter, ewdk_path)
        build(ewdk_path, args.output_prefix)
    finally:
        print("Unmounting ISO")
        unmount_ewdk()


if __name__ == "__main__":
    main()
