# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import requests
import shutil
import tempfile
import zipfile

# This script updates the current directory with the parts of the coremltool
# repository needed in chromium.

# Source URL for the current version of the coremltools repository as a zip file.
coremltools_src_url = "https://github.com/apple/coremltools/archive/refs/tags/7.1.zip"
# The directory within the zip archive that we want to extract.
extract_sub_dir = os.path.join("coremltools-7.1", "mlmodel" , "format");
# The destination within the current working directory where we want the files
# to be extracted to.
destination_sub_dir = os.path.join("mlmodel", "format");

# start by deleting the existing directories
for path in [extract_sub_dir, destination_sub_dir]:
    if os.path.exists(path):
        shutil.rmtree(path)

# Download the coreml release zip file
with tempfile.TemporaryDirectory() as temp_dir:
    file_path = os.path.join(temp_dir, "coremltools.zip")
    with open(file_path, "wb") as f:
        response = requests.get(coremltools_src_url)
        f.write(response.content)
    with zipfile.ZipFile(file_path, "r") as zip_ref:
        zip_ref.extractall(os.path.join(temp_dir, "coremltools"));
    # Copy the directory we need to the current working directory.
    shutil.copytree(os.path.join(temp_dir, "coremltools", extract_sub_dir), destination_sub_dir);

# Rename License.txt to License as per chromium requirements
os.rename(os.path.join(destination_sub_dir, "LICENSE.txt"), os.path.join(destination_sub_dir, "LICENSE"))

# Generate the BUILD.gn file

# Helper to get a list of files with a particular extension.
def get_files_with_extension(path, extension):
    files = []
    for root, dirs, filenames in os.walk(path):
        for filename in filenames:
            if filename.endswith(extension):
                files.append(os.path.join(root, filename))
    return files

def format_list(list):
    formatted_string = '[\n'
    for i in range(len(list)):
        formatted_string += f'    "{list[i]}",\n'
    formatted_string += '  ]'
    return formatted_string;

proto_files = get_files_with_extension(destination_sub_dir, ".proto")
proto_files.sort()

# Fill out the list of .proto files in the below template
build_file = '''# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import("//third_party/protobuf/proto_library.gni")

# coremltools is only used by //services/webnn/coreml on macOS.
assert(is_mac)

proto_library("modelformat_proto") {{
  sources = {proto_files}
  cc_generator_options = "lite"
}}
'''.format(proto_files=format_list(proto_files));

with open("BUILD.gn", "w") as text_file:
    text_file.write(build_file)