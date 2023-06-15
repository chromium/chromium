// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/lens.h"

#include <string>

#include "third_party/re2/src/re2/re2.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace {

constexpr const char* kDefaultContainer = "(Default container)";
constexpr const char* kNoComponent = "(No component)";

bool PartialMatch(const char* string, const RE2& regex) {
  if (string) {
    return RE2::PartialMatch(string, regex);
  }
  return false;
}

}  // namespace

namespace caspian {

std::string_view IdPathLens::ParentName(const BaseSymbol& symbol) {
  return "";
}

std::string_view ContainerLens::ParentName(const BaseSymbol& symbol) {
  std::string_view ret = symbol.ContainerName();
  return ret.empty() ? kDefaultContainer : ret;
}

std::string_view ComponentLens::ParentName(const BaseSymbol& symbol) {
  if (symbol.Component() && *symbol.Component()) {
    return symbol.Component();
  }
  return kNoComponent;
}

std::string_view TemplateLens::ParentName(const BaseSymbol& symbol) {
  return symbol.Name();
}

std::string_view GeneratedLens::ParentName(const BaseSymbol& symbol) {
  static LazyRE2 register_jni_regex = {
      R"(Register.*JNIEnv\*\)|RegisteredMethods$)"};
  if (RE2::PartialMatch(symbol.FullName(), *register_jni_regex)) {
    return "RegisterJNI";
  }

  static LazyRE2 gl_bindings_autogen_regex = {"gl_bindings_autogen"};
  if (PartialMatch(symbol.SourcePath(), *gl_bindings_autogen_regex) ||
      PartialMatch(symbol.ObjectPath(), *gl_bindings_autogen_regex)) {
    return "gl_bindings_autogen";
  }
  if (!symbol.IsGeneratedSource()) {
    return "Not generated";
  }

  static LazyRE2 java_protobuf_regex = {R"(__protoc_java\.srcjar)"};
  if (PartialMatch(symbol.SourcePath(), *java_protobuf_regex)) {
    return "Java Protocol Buffers";
  }

  static LazyRE2 cc_protobuf_regex = {R"(/protobuf/|\.pbzero\.o$|\.pb\.o$)"};
  if (PartialMatch(symbol.ObjectPath(), *cc_protobuf_regex)) {
    return "C++ Protocol Buffers";
  }

  static LazyRE2 mojo_regex = {"\\bmojom?\\b"};
  if (symbol.FullName().substr(0, 7) == "mojom::" ||
      PartialMatch(symbol.SourcePath(), *mojo_regex)) {
    return "Mojo";
  }

  static LazyRE2 dev_tools_regex = {R"(\b(?:protocol|devtools)\b)"};
  if (PartialMatch(symbol.SourcePath(), *dev_tools_regex)) {
    return "DevTools";
  }

  static LazyRE2 blink_bindings_regex = {R"((?:blink|WebKit)/.*bindings)"};
  if (PartialMatch(symbol.ObjectPath(), *blink_bindings_regex)) {
    return "Blink (bindings)";
  }

  static LazyRE2 blink_regex = {"WebKit|blink/"};
  if (PartialMatch(symbol.ObjectPath(), *blink_regex)) {
    return "Blink (other)";
  }

  static LazyRE2 v8_builtins = {"embedded.S$"};
  if (PartialMatch(symbol.ObjectPath(), *v8_builtins)) {
    return "V8 Builtins";
  }

  static LazyRE2 prepopulated_engines_regex = {"prepopulated_engines"};
  if (PartialMatch(symbol.ObjectPath(), *prepopulated_engines_regex)) {
    return "Metrics-related code";
  }

  static LazyRE2 gpu_driver_regex = {"gpu_driver_bug_list"};
  if (PartialMatch(symbol.ObjectPath(), *gpu_driver_regex)) {
    return "gpu_driver_bug_list_autogen.cc";
  }

  static LazyRE2 components_policy_regex = {"components/policy"};
  if (PartialMatch(symbol.ObjectPath(), *components_policy_regex)) {
    return "components/policy";
  }

  return "Generated (other)";
}

}  // namespace caspian
