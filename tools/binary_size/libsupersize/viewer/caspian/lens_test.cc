// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/lens.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {

TEST(LensTest, TestGeneratedLensRegisterJNI) {
  Symbol sym;
  sym.full_name_ =
      "base::android::JNI_TraceEvent_RegisterEnabledObserver(_JNIEnv*)";
  sym.section_id_ = SectionId::kText;
  sym.source_path_ = "a/b/c.java";
  EXPECT_EQ("RegisterJNI", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensGlBindings) {
  Symbol sym;
  sym.full_name_ = "gl::TraceGLApi::glCopyTexImage2DFn(unsigned int, int, int)";
  sym.section_id_ = SectionId::kText;
  sym.source_path_ = "a/b/gl_bindings_autogen_gl.cc";
  EXPECT_EQ("gl_bindings_autogen", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensNotGenerated) {
  Symbol sym;
  sym.full_name_ = "NotAGeneratedSymbol";
  sym.section_id_ = SectionId::kText;
  sym.source_path_ = "a/b/c.cc";
  EXPECT_EQ("Not generated", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensJavaProtoFromFilename) {
  Symbol sym;
  sym.section_id_ = SectionId::kDex;
  sym.source_path_ = "a/b/FooProto.java";
  sym.flags_ |= SymbolFlag::kGeneratedSource;
  // Java filename match is insufficient for "Java Protocol Buffers" detection.
  EXPECT_EQ("Generated (other)", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensJavaProto) {
  Symbol sym;
  sym.section_id_ = SectionId::kDex;
  sym.source_path_ = "a/b/foo_proto_java__protoc_java.srcjar";
  sym.flags_ |= SymbolFlag::kGeneratedSource;
  EXPECT_EQ("Java Protocol Buffers", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensCppProto) {
  Symbol sym;
  sym.section_id_ = SectionId::kDex;
  sym.object_path_ = "a/b/sync.pb.o";
  sym.flags_ |= SymbolFlag::kGeneratedSource;
  EXPECT_EQ("C++ Protocol Buffers", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensMojo1) {
  Symbol sym;
  sym.section_id_ = SectionId::kText;
  sym.source_path_ = "a.mojom";
  sym.flags_ |= SymbolFlag::kGeneratedSource;
  EXPECT_EQ("Mojo", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensMojo2) {
  Symbol sym;
  sym.section_id_ = SectionId::kText;
  sym.flags_ |= SymbolFlag::kGeneratedSource;
  sym.full_name_ = "mojom::foo()";
  EXPECT_EQ("Mojo", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensDevTools) {
  Symbol sym;
  sym.section_id_ = SectionId::kText;
  sym.flags_ |= SymbolFlag::kGeneratedSource;

  sym.source_path_ = "a/b/protocol/Foo.cpp";
  EXPECT_EQ("DevTools", GeneratedLens().ParentName(sym));

  sym.source_path_ = "a/b/devtools/Foo.cpp";
  EXPECT_EQ("DevTools", GeneratedLens().ParentName(sym));
}

TEST(LensTest, TestGeneratedLensBlinkBindings) {
  Symbol sym;
  sym.section_id_ = SectionId::kText;
  sym.flags_ |= SymbolFlag::kGeneratedSource;

  sym.object_path_ = "blink/foo/bindings/bar";
  EXPECT_EQ("Blink (bindings)", GeneratedLens().ParentName(sym));
}

}  // namespace caspian
