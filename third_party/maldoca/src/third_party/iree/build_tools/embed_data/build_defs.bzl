# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Embeds data files into a C++ module."""

def cc_embed_data(
        name,
        srcs,
        cc_file_output,
        h_file_output,
        testonly = False,
        cpp_namespace = None,
        strip_prefix = None,
        flatten = False,
        identifier = None,
        **kwargs):
    """Embeds 'srcs' into a C++ module.

    Generates a header like:
      namespace iree {
        struct FileToc {
          const char* name;             // the file's original name
          const char* data;             // beginning of the file
          size_t size;                  // length of the file
        };
      }
      namespace foo {
      extern const struct ::iree::FileToc* this_rule_name_create();
      }

    The 'this_rule_name()' function will return an array of FileToc
    structs terminated by one that has nullptr 'name' and 'data' fields.
    The 'data' field always has an extra null terminator at the end (which
    is not included in the size).

    Args:
      name: The rule name, which will also be the identifier of the generated
        code symbol.
      srcs: List of files to embed.
      cc_file_output: The CC implementation file to output.
      h_file_output: The H header file to output.
      testonly: If True, only testonly targets can depend on this target.
      cpp_namespace: Wraps everything in a C++ namespace.
      strip_prefix: Strips this verbatim prefix from filenames (in the TOC).
      flatten: Removes all directory components from filenames (in the TOC).
      identifier: The identifier to use in generated names (defaults to name).
      **kwargs: Args to pass to the cc_library.
    """
    generator = "//third_party/iree/build_tools/embed_data:generate_cc_embed_data"
    generator_location = "$(location %s)" % generator
    if identifier == None:
        identifier = name
    flags = "--output_header='$(location %s)' --output_impl='$(location %s)'" % (
        h_file_output,
        cc_file_output,
    )
    flags += " --identifier='%s'" % (identifier,)
    if cpp_namespace != None:
        flags += " --cpp_namespace='%s'" % (cpp_namespace,)
    if strip_prefix != None:
        flags += " --strip_prefix='%s'" % (strip_prefix,)
    if flatten:
        flags += " --flatten"

    native.genrule(
        name = name + "__generator",
        srcs = srcs,
        outs = [
            cc_file_output,
            h_file_output,
        ],
        tools = [generator],
        cmd = "%s $(SRCS) %s" % (generator_location, flags),
        testonly = testonly,
    )
    native.cc_library(
        name = name,
        hdrs = [h_file_output],
        srcs = [cc_file_output],
        testonly = testonly,
        **kwargs
    )
