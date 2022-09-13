// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "minidump/minidump_writable.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

class BaseTestMinidumpWritable : public crashpad::internal::MinidumpWritable {
 public:
  BaseTestMinidumpWritable()
      : MinidumpWritable(),
        children_(),
        expected_offset_(-1),
        alignment_(0),
        phase_(kPhaseEarly),
        has_alignment_(false),
        has_phase_(false),
        verified_(false) {}

  BaseTestMinidumpWritable(const BaseTestMinidumpWritable&) = delete;
  BaseTestMinidumpWritable& operator=(const BaseTestMinidumpWritable&) = delete;

  ~BaseTestMinidumpWritable() { EXPECT_TRUE(verified_); }

  void SetAlignment(size_t alignment) {
    alignment_ = alignment;
    has_alignment_ = true;
  }

  void AddChild(BaseTestMinidumpWritable* child) { children_.push_back(child); }

  void SetPhaseLate() {
    phase_ = kPhaseLate;
    has_phase_ = true;
  }

  void Verify() {
    verified_ = true;
    EXPECT_EQ(state(), kStateWritten);
    for (BaseTestMinidumpWritable* child : children_) {
      child->Verify();
    }
  }

 protected:
  bool Freeze() override {
    EXPECT_EQ(state(), kStateMutable);
    bool rv = MinidumpWritable::Freeze();
    EXPECT_TRUE(rv);
    EXPECT_EQ(state(), kStateFrozen);
    return rv;
  }

  size_t Alignment() override {
    EXPECT_GE(state(), kStateFrozen);
    return has_alignment_ ? alignment_ : MinidumpWritable::Alignment();
  }

  std::vector<MinidumpWritable*> Children() override {
    EXPECT_GE(state(), kStateFrozen);
    if (!children_.empty()) {
      std::vector<MinidumpWritable*> children;
      for (BaseTestMinidumpWritable* child : children_) {
        children.push_back(child);
      }
      return children;
    }
    return MinidumpWritable::Children();
  }

  Phase WritePhase() override {
    return has_phase_ ? phase_ : MinidumpWritable::Phase();
  }

  bool WillWriteAtOffsetImpl(FileOffset offset) override {
    EXPECT_EQ(kStateFrozen, state());
    expected_offset_ = offset;
    bool rv = MinidumpWritable::WillWriteAtOffsetImpl(offset);
    EXPECT_TRUE(rv);
    return rv;
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    EXPECT_EQ(kStateWritable, state());
    EXPECT_EQ(file_writer->Seek(0, SEEK_CUR), expected_offset_);

    // Subclasses must override this.
    return false;
  }

 private:
  std::vector<BaseTestMinidumpWritable*> children_;
  FileOffset expected_offset_;
  size_t alignment_;
  Phase phase_;
  bool has_alignment_;
  bool has_phase_;
  bool verified_;
};

class TestStringMinidumpWritable final : public BaseTestMinidumpWritable {
 public:
  TestStringMinidumpWritable() : BaseTestMinidumpWritable(), data_() {}

  TestStringMinidumpWritable(const TestStringMinidumpWritable&) = delete;
  TestStringMinidumpWritable& operator=(const TestStringMinidumpWritable&) =
      delete;

  ~TestStringMinidumpWritable() {}

  void SetData(const std::string& string) { data_ = string; }

 protected:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return data_.size();
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    BaseTestMinidumpWritable::WriteObject(file_writer);
    bool rv = file_writer->Write(&data_[0], data_.size());
    EXPECT_TRUE(rv);
    return rv;
  }

 private:
  std::string data_;
};

TEST(MinidumpWritable, MinidumpWritable) {
  StringFile string_file;

  {
    SCOPED_TRACE("empty");
    string_file.Reset();
    TestStringMinidumpWritable string_writable;
    EXPECT_TRUE(string_writable.WriteEverything(&string_file));
    EXPECT_TRUE(string_file.string().empty());
    string_writable.Verify();
  }

  {
    SCOPED_TRACE("childless");
    string_file.Reset();
    TestStringMinidumpWritable string_writable;
    string_writable.SetData("a");
    EXPECT_TRUE(string_writable.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 1u);
    EXPECT_EQ(string_file.string(), "a");
    string_writable.Verify();
  }

  {
    SCOPED_TRACE("parent-child");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("b");
    TestStringMinidumpWritable child;
    child.SetData("c");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 5u);
    EXPECT_EQ(string_file.string(), std::string("b\0\0\0c", 5));
    parent.Verify();
  }

  {
    SCOPED_TRACE("base alignment 2");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("de");
    TestStringMinidumpWritable child;
    child.SetData("f");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 5u);
    EXPECT_EQ(string_file.string(), std::string("de\0\0f", 5));
    parent.Verify();
  }

  {
    SCOPED_TRACE("base alignment 3");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("ghi");
    TestStringMinidumpWritable child;
    child.SetData("j");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 5u);
    EXPECT_EQ(string_file.string(), std::string("ghi\0j", 5));
    parent.Verify();
  }

  {
    SCOPED_TRACE("base alignment 4");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("klmn");
    TestStringMinidumpWritable child;
    child.SetData("o");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 5u);
    EXPECT_EQ(string_file.string(), "klmno");
    parent.Verify();
  }

  {
    SCOPED_TRACE("base alignment 5");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("pqrst");
    TestStringMinidumpWritable child;
    child.SetData("u");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 9u);
    EXPECT_EQ(string_file.string(), std::string("pqrst\0\0\0u", 9));
    parent.Verify();
  }

  {
    SCOPED_TRACE("two children");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child_0;
    child_0.SetData("child_0");
    parent.AddChild(&child_0);
    TestStringMinidumpWritable child_1;
    child_1.SetData("child_1");
    parent.AddChild(&child_1);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 23u);
    EXPECT_EQ(string_file.string(),
              std::string("parent\0\0child_0\0child_1", 23));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child;
    child.SetData("child");
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    grandchild.SetData("grandchild");
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 26u);
    EXPECT_EQ(string_file.string(),
              std::string("parent\0\0child\0\0\0grandchild", 26));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild with empty parent");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    TestStringMinidumpWritable child;
    child.SetData("child");
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    grandchild.SetData("grandchild");
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 18u);
    EXPECT_EQ(string_file.string(), std::string("child\0\0\0grandchild", 18));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild with empty child");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child;
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    grandchild.SetData("grandchild");
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 18u);
    EXPECT_EQ(string_file.string(), std::string("parent\0\0grandchild", 18));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild with empty grandchild");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child;
    child.SetData("child");
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 13u);
    EXPECT_EQ(string_file.string(), std::string("parent\0\0child", 13));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild with late-phase grandchild");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child;
    child.SetData("child");
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    grandchild.SetData("grandchild");
    grandchild.SetPhaseLate();
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 26u);
    EXPECT_EQ(string_file.string(),
              std::string("parent\0\0child\0\0\0grandchild", 26));
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchild with late-phase child");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("parent");
    TestStringMinidumpWritable child;
    child.SetData("child");
    child.SetPhaseLate();
    parent.AddChild(&child);
    TestStringMinidumpWritable grandchild;
    grandchild.SetData("grandchild");
    child.AddChild(&grandchild);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 25u);
    EXPECT_EQ(string_file.string(),
              std::string("parent\0\0grandchild\0\0child", 25));
    parent.Verify();
  }

  {
    SCOPED_TRACE("family tree");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("P..");
    TestStringMinidumpWritable child_0;
    child_0.SetData("C0.");
    parent.AddChild(&child_0);
    TestStringMinidumpWritable child_1;
    child_1.SetData("C1.");
    parent.AddChild(&child_1);
    TestStringMinidumpWritable grandchild_00;
    grandchild_00.SetData("G00");
    child_0.AddChild(&grandchild_00);
    TestStringMinidumpWritable grandchild_01;
    grandchild_01.SetData("G01");
    child_0.AddChild(&grandchild_01);
    TestStringMinidumpWritable grandchild_10;
    grandchild_10.SetData("G10");
    child_1.AddChild(&grandchild_10);
    TestStringMinidumpWritable grandchild_11;
    grandchild_11.SetData("G11");
    child_1.AddChild(&grandchild_11);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 27u);
    EXPECT_EQ(string_file.string(),
              std::string("P..\0C0.\0G00\0G01\0C1.\0G10\0G11", 27));
    parent.Verify();
  }

  {
    SCOPED_TRACE("family tree with C0 late");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("P..");
    TestStringMinidumpWritable child_0;
    child_0.SetData("C0.");
    child_0.SetPhaseLate();
    parent.AddChild(&child_0);
    TestStringMinidumpWritable child_1;
    child_1.SetData("C1.");
    parent.AddChild(&child_1);
    TestStringMinidumpWritable grandchild_00;
    grandchild_00.SetData("G00");
    child_0.AddChild(&grandchild_00);
    TestStringMinidumpWritable grandchild_01;
    grandchild_01.SetData("G01");
    child_0.AddChild(&grandchild_01);
    TestStringMinidumpWritable grandchild_10;
    grandchild_10.SetData("G10");
    child_1.AddChild(&grandchild_10);
    TestStringMinidumpWritable grandchild_11;
    grandchild_11.SetData("G11");
    child_1.AddChild(&grandchild_11);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 27u);
    EXPECT_EQ(string_file.string(),
              std::string("P..\0G00\0G01\0C1.\0G10\0G11\0C0.", 27));
    parent.Verify();
  }

  {
    SCOPED_TRACE("family tree with G0 late");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("P..");
    TestStringMinidumpWritable child_0;
    child_0.SetData("C0.");
    parent.AddChild(&child_0);
    TestStringMinidumpWritable child_1;
    child_1.SetData("C1.");
    parent.AddChild(&child_1);
    TestStringMinidumpWritable grandchild_00;
    grandchild_00.SetData("G00");
    grandchild_00.SetPhaseLate();
    child_0.AddChild(&grandchild_00);
    TestStringMinidumpWritable grandchild_01;
    grandchild_01.SetData("G01");
    grandchild_01.SetPhaseLate();
    child_0.AddChild(&grandchild_01);
    TestStringMinidumpWritable grandchild_10;
    grandchild_10.SetData("G10");
    child_1.AddChild(&grandchild_10);
    TestStringMinidumpWritable grandchild_11;
    grandchild_11.SetData("G11");
    child_1.AddChild(&grandchild_11);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 27u);
    EXPECT_EQ(string_file.string(),
              std::string("P..\0C0.\0C1.\0G10\0G11\0G00\0G01", 27));
    parent.Verify();
  }

  {
    SCOPED_TRACE("align 1");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("p");
    TestStringMinidumpWritable child;
    child.SetData("c");
    child.SetAlignment(1);
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 2u);
    EXPECT_EQ(string_file.string(), "pc");
    parent.Verify();
  }

  {
    SCOPED_TRACE("align 2");
    string_file.Reset();
    TestStringMinidumpWritable parent;
    parent.SetData("p");
    TestStringMinidumpWritable child;
    child.SetData("c");
    child.SetAlignment(2);
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));
    EXPECT_EQ(string_file.string().size(), 3u);
    EXPECT_EQ(string_file.string(), std::string("p\0c", 3));
    parent.Verify();
  }
}

template <typename RVAType>
class TTestRVAMinidumpWritable final : public BaseTestMinidumpWritable {
 public:
  TTestRVAMinidumpWritable() : BaseTestMinidumpWritable(), rva_() {}

  TTestRVAMinidumpWritable(const TTestRVAMinidumpWritable&) = delete;
  TTestRVAMinidumpWritable& operator=(const TTestRVAMinidumpWritable&) = delete;

  ~TTestRVAMinidumpWritable() {}

  void SetRVA(MinidumpWritable* other) { other->RegisterRVA(&rva_); }

 protected:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return sizeof(rva_);
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    BaseTestMinidumpWritable::WriteObject(file_writer);
    EXPECT_TRUE(file_writer->Write(&rva_, sizeof(rva_)));
    return true;
  }

 private:
  RVAType rva_;
};

template <typename RVAType>
RVAType TRVAAtIndex(const std::string& string, size_t index) {
  return *reinterpret_cast<const RVAType*>(&string[index * sizeof(RVAType)]);
}

template <typename T>
struct TestNames {};

template <>
struct TestNames<RVA> {
  static constexpr char kName[] = "RVA";
};

template <>
struct TestNames<RVA64> {
  static constexpr char kName[] = "RVA64";
};

class TestTypeNames {
 public:
  template <typename T>
  static std::string GetName(int) {
    return std::string(TestNames<T>::kName);
  }
};

template <typename RVAType>
class MinidumpWritable : public ::testing::Test {};

using RVATypes = ::testing::Types<RVA, RVA64>;
TYPED_TEST_SUITE(MinidumpWritable, RVATypes, TestTypeNames);

TYPED_TEST(MinidumpWritable, RVA) {
  StringFile string_file;
  using TestRVAMinidumpWritable = TTestRVAMinidumpWritable<TypeParam>;
  const auto RVAAtIndex = TRVAAtIndex<TypeParam>;
  constexpr size_t kRVASize = sizeof(TypeParam);

  {
    SCOPED_TRACE("unset");
    string_file.Reset();
    TestRVAMinidumpWritable rva_writable;
    EXPECT_TRUE(rva_writable.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 0 * kRVASize);
    rva_writable.Verify();
  }

  {
    SCOPED_TRACE("self");
    string_file.Reset();
    TestRVAMinidumpWritable rva_writable;
    rva_writable.SetRVA(&rva_writable);
    EXPECT_TRUE(rva_writable.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 0 * kRVASize);
    rva_writable.Verify();
  }

  {
    SCOPED_TRACE("parent-child self");
    string_file.Reset();
    TestRVAMinidumpWritable parent;
    parent.SetRVA(&parent);
    TestRVAMinidumpWritable child;
    child.SetRVA(&child);
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), 2 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 0 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 1), 1 * kRVASize);
    parent.Verify();
  }

  {
    SCOPED_TRACE("parent-child only");
    string_file.Reset();
    TestRVAMinidumpWritable parent;
    TestRVAMinidumpWritable child;
    parent.SetRVA(&child);
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), 2 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 1 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 1), 0 * kRVASize);
    parent.Verify();
  }

  {
    SCOPED_TRACE("parent-child circular");
    string_file.Reset();
    TestRVAMinidumpWritable parent;
    TestRVAMinidumpWritable child;
    parent.SetRVA(&child);
    child.SetRVA(&parent);
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), 2 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 1 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 1), 0 * kRVASize);
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchildren");
    string_file.Reset();
    TestRVAMinidumpWritable parent;
    TestRVAMinidumpWritable child;
    parent.SetRVA(&child);
    parent.AddChild(&child);
    TestRVAMinidumpWritable grandchild_0;
    grandchild_0.SetRVA(&child);
    child.AddChild(&grandchild_0);
    TestRVAMinidumpWritable grandchild_1;
    grandchild_1.SetRVA(&child);
    child.AddChild(&grandchild_1);
    TestRVAMinidumpWritable grandchild_2;
    grandchild_2.SetRVA(&child);
    child.AddChild(&grandchild_2);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), 5 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 0), 1 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 1), 0 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 2), 1 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 3), 1 * kRVASize);
    EXPECT_EQ(RVAAtIndex(string_file.string(), 4), 1 * kRVASize);
    parent.Verify();
  }
}

template <typename MinidumpLocationDescriptorType>
class TTestLocationDescriptorMinidumpWritable final
    : public BaseTestMinidumpWritable {
 public:
  TTestLocationDescriptorMinidumpWritable()
      : BaseTestMinidumpWritable(), location_descriptor_(), string_() {}

  TTestLocationDescriptorMinidumpWritable(
      const TTestLocationDescriptorMinidumpWritable&) = delete;
  TTestLocationDescriptorMinidumpWritable& operator=(
      const TTestLocationDescriptorMinidumpWritable&) = delete;

  ~TTestLocationDescriptorMinidumpWritable() {}

  void SetLocationDescriptor(MinidumpWritable* other) {
    other->RegisterLocationDescriptor(&location_descriptor_);
  }

  void SetString(const std::string& string) { string_ = string; }

 protected:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    // NUL-terminate.
    return sizeof(location_descriptor_) + string_.size() + 1;
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    BaseTestMinidumpWritable::WriteObject(file_writer);
    WritableIoVec iov;
    iov.iov_base = &location_descriptor_;
    iov.iov_len = sizeof(location_descriptor_);
    std::vector<WritableIoVec> iovecs(1, iov);
    // NUL-terminate.
    iov.iov_base = &string_[0];
    iov.iov_len = string_.size() + 1;
    iovecs.push_back(iov);
    EXPECT_TRUE(file_writer->WriteIoVec(&iovecs));
    return true;
  }

 private:
  MinidumpLocationDescriptorType location_descriptor_;
  std::string string_;
};

template <typename MinidumpLocationDescriptorType>
struct TLocationDescriptorAndData {
  MinidumpLocationDescriptorType location_descriptor;
  const char* string;
};

template <typename MinidumpLocationDescriptorType>
TLocationDescriptorAndData<MinidumpLocationDescriptorType> TLDDAtIndex(
    const std::string& str,
    size_t index) {
  const MinidumpLocationDescriptorType* location_descriptor =
      reinterpret_cast<const MinidumpLocationDescriptorType*>(&str[index]);

  const char* string = reinterpret_cast<const char*>(
      &str[index] +
      offsetof(TLocationDescriptorAndData<MinidumpLocationDescriptorType>,
               string));

  return TLocationDescriptorAndData<MinidumpLocationDescriptorType>{
      *location_descriptor,
      string,
  };
}

template <typename MinidumpLocationDescriptorType>
class MinidumpWritableLocationDescriptor : public ::testing::Test {};

template <>
struct TestNames<MINIDUMP_LOCATION_DESCRIPTOR> {
  static constexpr char kName[] = "MINIDUMP_LOCATION_DESCRIPTOR";
};

template <>
struct TestNames<MINIDUMP_LOCATION_DESCRIPTOR64> {
  static constexpr char kName[] = "MINIDUMP_LOCATION_DESCRIPTOR64";
};

using MinidumpLocationDescriptorTypes =
    ::testing::Types<MINIDUMP_LOCATION_DESCRIPTOR,
                     MINIDUMP_LOCATION_DESCRIPTOR64>;
TYPED_TEST_SUITE(MinidumpWritableLocationDescriptor,
                 MinidumpLocationDescriptorTypes,
                 TestTypeNames);

TYPED_TEST(MinidumpWritableLocationDescriptor, LocationDescriptor) {
  StringFile string_file;

  using LocationDescriptorAndData = TLocationDescriptorAndData<TypeParam>;
  const auto LDDAtIndex = TLDDAtIndex<TypeParam>;
  using TestLocationDescriptorMinidumpWritable =
      TTestLocationDescriptorMinidumpWritable<TypeParam>;
  constexpr size_t kMinidumpLocationDescriptorSize = sizeof(TypeParam);

  {
    SCOPED_TRACE("unset");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable location_descriptor_writable;
    EXPECT_TRUE(location_descriptor_writable.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), kMinidumpLocationDescriptorSize + 1);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);
    EXPECT_EQ(ldd.location_descriptor.DataSize, 0u);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    location_descriptor_writable.Verify();
  }

  {
    SCOPED_TRACE("self");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable location_descriptor_writable;
    location_descriptor_writable.SetLocationDescriptor(
        &location_descriptor_writable);
    EXPECT_TRUE(location_descriptor_writable.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), kMinidumpLocationDescriptorSize + 1);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 1);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    location_descriptor_writable.Verify();
  }

  {
    SCOPED_TRACE("self with data");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable location_descriptor_writable;
    location_descriptor_writable.SetLocationDescriptor(
        &location_descriptor_writable);
    location_descriptor_writable.SetString("zz");
    EXPECT_TRUE(location_descriptor_writable.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(), kMinidumpLocationDescriptorSize + 3);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 3);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    EXPECT_STREQ("zz", ldd.string);
    location_descriptor_writable.Verify();
  }

  {
    SCOPED_TRACE("parent-child self");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable parent;
    parent.SetLocationDescriptor(&parent);
    parent.SetString("yy");
    TestLocationDescriptorMinidumpWritable child;
    child.SetLocationDescriptor(&child);
    child.SetString("x");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(),
              kMinidumpLocationDescriptorSize * 2 + 6);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);

    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 3);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    EXPECT_STREQ("yy", ldd.string);
    ldd = LDDAtIndex(string_file.string(), kMinidumpLocationDescriptorSize + 4);

    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 2);

    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("x", ldd.string);
    parent.Verify();
  }

  {
    SCOPED_TRACE("parent-child only");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable parent;
    TestLocationDescriptorMinidumpWritable child;
    parent.SetLocationDescriptor(&child);
    parent.SetString("www");
    child.SetString("vv");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(),
              kMinidumpLocationDescriptorSize * 2 + 7);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);

    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 3);

    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("www", ldd.string);
    ldd = LDDAtIndex(string_file.string(), kMinidumpLocationDescriptorSize + 4);

    EXPECT_EQ(ldd.location_descriptor.DataSize, 0u);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    EXPECT_STREQ("vv", ldd.string);
    parent.Verify();
  }

  {
    SCOPED_TRACE("parent-child circular");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable parent;
    TestLocationDescriptorMinidumpWritable child;
    parent.SetLocationDescriptor(&child);
    parent.SetString("uuuu");
    child.SetLocationDescriptor(&parent);
    child.SetString("tttt");
    parent.AddChild(&child);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(),
              kMinidumpLocationDescriptorSize * 2 + 13);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 5);
    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 8);
    EXPECT_STREQ("uuuu", ldd.string);
    ldd = LDDAtIndex(string_file.string(), kMinidumpLocationDescriptorSize + 8);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 5);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    EXPECT_STREQ("tttt", ldd.string);
    parent.Verify();
  }

  {
    SCOPED_TRACE("grandchildren");
    string_file.Reset();
    TestLocationDescriptorMinidumpWritable parent;
    TestLocationDescriptorMinidumpWritable child;
    parent.SetLocationDescriptor(&child);
    parent.SetString("s");
    parent.AddChild(&child);
    child.SetString("r");
    TestLocationDescriptorMinidumpWritable grandchild_0;
    grandchild_0.SetLocationDescriptor(&child);
    grandchild_0.SetString("q");
    child.AddChild(&grandchild_0);
    TestLocationDescriptorMinidumpWritable grandchild_1;
    grandchild_1.SetLocationDescriptor(&child);
    grandchild_1.SetString("p");
    child.AddChild(&grandchild_1);
    TestLocationDescriptorMinidumpWritable grandchild_2;
    grandchild_2.SetLocationDescriptor(&child);
    grandchild_2.SetString("o");
    child.AddChild(&grandchild_2);
    EXPECT_TRUE(parent.WriteEverything(&string_file));

    ASSERT_EQ(string_file.string().size(),
              kMinidumpLocationDescriptorSize * 5 + 18);
    LocationDescriptorAndData ldd = LDDAtIndex(string_file.string(), 0);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 2);
    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("s", ldd.string);
    ldd = LDDAtIndex(string_file.string(), kMinidumpLocationDescriptorSize + 4);
    EXPECT_EQ(ldd.location_descriptor.DataSize, 0u);
    EXPECT_EQ(ldd.location_descriptor.Rva, 0u);
    EXPECT_STREQ("r", ldd.string);
    ldd = LDDAtIndex(string_file.string(),
                     kMinidumpLocationDescriptorSize * 2 + 8);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 2);
    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("q", ldd.string);
    ldd = LDDAtIndex(string_file.string(),
                     kMinidumpLocationDescriptorSize * 3 + 12);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 2);
    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("p", ldd.string);
    ldd = LDDAtIndex(string_file.string(),
                     kMinidumpLocationDescriptorSize * 4 + 16);
    EXPECT_EQ(ldd.location_descriptor.DataSize,
              kMinidumpLocationDescriptorSize + 2);
    EXPECT_EQ(ldd.location_descriptor.Rva, kMinidumpLocationDescriptorSize + 4);
    EXPECT_STREQ("o", ldd.string);
    parent.Verify();
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
