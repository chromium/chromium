/*
 * Copyright 2019 The gRPC Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Generates Java gRPC service interface out of Protobuf IDL.
//
// This is a Proto2 compiler plugin.  See net/proto2/compiler/proto/plugin.proto
// and net/proto2/compiler/public/plugin.h for more information on plugins.

#include <memory>

#include "java_generator.h"
#include <google/protobuf/compiler/code_generator.h>
#if GOOGLE_PROTOBUF_VERSION >= 5027000
#include <google/protobuf/compiler/java/java_features.pb.h>
#endif
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace protobuf = google::protobuf;

static std::string JavaPackageToDir(const std::string& package_name) {
  std::string package_dir = package_name;
  for (size_t i = 0; i < package_dir.size(); ++i) {
    if (package_dir[i] == '.') {
      package_dir[i] = '/';
    }
  }
  if (!package_dir.empty()) package_dir += "/";
  return package_dir;
}

class JavaGrpcGenerator : public protobuf::compiler::CodeGenerator {
 public:
  JavaGrpcGenerator() {}
  virtual ~JavaGrpcGenerator() {}

// Protobuf 5.27 released edition 2023.
#if GOOGLE_PROTOBUF_VERSION >= 5027000
  uint64_t GetSupportedFeatures() const override {
    return Feature::FEATURE_PROTO3_OPTIONAL |
           Feature::FEATURE_SUPPORTS_EDITIONS;
  }
  protobuf::Edition GetMinimumEdition() const override {
    return protobuf::Edition::EDITION_PROTO2;
  }
  protobuf::Edition GetMaximumEdition() const override {
    return protobuf::Edition::EDITION_2023;
  }
  std::vector<const protobuf::FieldDescriptor*> GetFeatureExtensions()
      const override {
    return {GetExtensionReflection(pb::java)};
  }
#else
  uint64_t GetSupportedFeatures() const override {
    return Feature::FEATURE_PROTO3_OPTIONAL;
  }
#endif

  virtual bool Generate(const protobuf::FileDescriptor* file,
                        const std::string& parameter,
                        protobuf::compiler::GeneratorContext* context,
                        std::string* error) const override {
    std::vector<std::pair<std::string, std::string> > options;
    protobuf::compiler::ParseGeneratorParameter(parameter, &options);

    java_grpc_generator::ProtoFlavor flavor =
        java_grpc_generator::ProtoFlavor::NORMAL;
    java_grpc_generator::GeneratedAnnotation generated_annotation =
        java_grpc_generator::GeneratedAnnotation::JAVAX;

    bool disable_version = false;
    for (size_t i = 0; i < options.size(); i++) {
      if (options[i].first == "lite") {
        flavor = java_grpc_generator::ProtoFlavor::LITE;
      } else if (options[i].first == "noversion") {
        disable_version = true;
      } else if (options[i].first == "@generated") {
         if (options[i].second == "omit") {
           generated_annotation = java_grpc_generator::GeneratedAnnotation::OMIT;
         } else if (options[i].second == "javax") {
           generated_annotation = java_grpc_generator::GeneratedAnnotation::JAVAX;
         }
      }
    }

    std::string package_name = java_grpc_generator::ServiceJavaPackage(file);
    std::string package_filename = JavaPackageToDir(package_name);
    for (int i = 0; i < file->service_count(); ++i) {
      const protobuf::ServiceDescriptor* service = file->service(i);
      std::string filename = package_filename
          + java_grpc_generator::ServiceClassName(service) + ".java";
      std::unique_ptr<protobuf::io::ZeroCopyOutputStream> output(
          context->Open(filename));
      java_grpc_generator::GenerateService(
          service, output.get(), flavor, disable_version, generated_annotation);
    }
    return true;
  }
};

int main(int argc, char* argv[]) {
  JavaGrpcGenerator generator;
  return protobuf::compiler::PluginMain(argc, argv, &generator);
}
