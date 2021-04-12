// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Plugin for the protobuf compiler (protoc) that ensures proto definitions are
// compiled in a way that they can be used with libprotobuf-mutator. Compiles
// protobufs to C++ like the normal protoc (using the cpp plugin).

#include <assert.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_generator.h"
#include "third_party/protobuf/src/google/protobuf/compiler/plugin.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.pb.h"

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::compiler::cpp::CppGenerator;

// Class that generates C++ code that can be used by LPM from proto libraries.
class LpmCppCodeGenerator : public CppGenerator {
 public:
  // Overrides the GenerateAll method on CppGenerator. This method essentially
  // does the same thing except it ensures that files are not optimized for
  // LITE_RUNTIME.
  virtual bool GenerateAll(const std::vector<const FileDescriptor*>& files,
                           const std::string& parameter,
                           GeneratorContext* generator_context,
                           std::string* error) const {
    if (files.size() == 0)
      return true;

    // Created a DescriptorPool once here so that modified files will use the
    // modified versions when importing.
    google::protobuf::DescriptorPool descriptor_pool(files[0]->pool());

    // Keep a list of files we have generated already, so that
    // GenerateFileAndDependencies won't generate the same file twice.
    std::unordered_set<const FileDescriptor*> prev_generated;

    // Mostly copied from GenerateAll from
    // //third_party/protobuf/src/google/protobuf/compiler/code_generator.cc
    bool succeeded = true;
    for (size_t idx = 0; idx < files.size(); idx++) {
      const FileDescriptor* file = files[idx];
      succeeded =
          GenerateFileAndDependencies(file, parameter, generator_context, error,
                                      &descriptor_pool, &prev_generated);

      if (!succeeded && error && error->empty()) {
        *error =
            "Code generator returned false but provided no error "
            "description.";
      }
      if (error && !error->empty()) {
        *error = file->name() + ": " + *error;
        break;
      }
      if (!succeeded)
        break;
    }
    return succeeded;
  }

  // Ensures that file and its dependancies are optimized for LPM by making them
  // optimized for speed (as opposed to LITE_RUNTIME which would make file
  // usable for LPM) then returns the result of a call to Generate on the
  // modified file and the other arguments to this method. Needs to modify
  // dependencies before file because protobuf doesn't allow a file to import
  // another if file is not optimized_for LITE_RUNTIME but the dependency is.
  // Returns true if file is in prev_generated.
  virtual bool GenerateFileAndDependencies(
      const FileDescriptor* file,
      const std::string& parameter,
      GeneratorContext* generator_context,
      std::string* error,
      google::protobuf::DescriptorPool* descriptor_pool,
      std::unordered_set<const FileDescriptor*>* prev_generated) const {
    if (prev_generated->find(file) != prev_generated->end())
      return true;

    // Make a copy of the file that we can modify.
    google::protobuf::FileDescriptorProto file_proto;
    file->CopyTo(&file_proto);

    // Fix all dependencies before fixing this file (A file must be optimized
    // for the lite runtime if it imports files that are.
    for (int idx = 0; idx < file->dependency_count(); idx++) {
      const FileDescriptor* dependent_file = file->dependency(idx);
      assert(dependent_file);
      bool result = GenerateFileAndDependencies(
          dependent_file, parameter, generator_context, error, descriptor_pool,
          prev_generated);
      assert(result);
      if (!result)
        return result;
    }

    // Base case:
    // Now make sure we aren't using the LITE_RUNTIME.
    file_proto.mutable_options()->set_optimize_for(
        google::protobuf::FileOptions::SPEED);

    // Convert it back to a FileDescriptor and pass it to the parent Generate
    // method.
    const FileDescriptor* modified_file =
        descriptor_pool->BuildFile(file_proto);
    assert(modified_file);

    // Ensure we only generate code once for file.
    prev_generated->insert(file);
    return CppGenerator::Generate(modified_file, parameter, generator_context,
                                  error);
  }
};

int main(int argc, char** argv) {
  // Invoke our lightly modified C++ code generator on the inputs.
  LpmCppCodeGenerator generator;
  return PluginMain(argc, argv, &generator);
}
