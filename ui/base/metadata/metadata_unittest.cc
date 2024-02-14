// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/gfx/geometry/insets.h"

namespace UM = ui::metadata;

class MetadataTest : public PlatformTest {
 public:
  MetadataTest() = default;
  ~MetadataTest() override = default;

  bool float_property_changed() const { return float_property_changed_; }
  void OnFloatPropertyChanged() { float_property_changed_ = true; }

 protected:
  template <typename T>
  UM::MemberMetaDataBase* GetMemberMetaData(T* obj,
                                            const std::string& member_name) {
    UM::ClassMetaData* meta_data = obj->GetClassMetaData();
    if (meta_data == nullptr)
      return nullptr;

    UM::MemberMetaDataBase* member_data =
        meta_data->FindMemberData(member_name);
    return member_data;
  }

 private:
  bool float_property_changed_ = false;
};

// Base class in which a simple hierarchy is created for testing metadata
// iteration across class types.
class MetadataTestBaseClass : public ui::metadata::MetaDataProvider,
                              public ui::PropertyHandler {
 public:
  MetadataTestBaseClass() = default;
  ~MetadataTestBaseClass() override = default;

  METADATA_HEADER_BASE(MetadataTestBaseClass);

  void SetIntProperty(int new_value) {
    if (new_value == int_property_)
      return;
    int_property_ = new_value;
    TriggerChangedCallback(&int_property_);
  }
  int GetIntProperty() const { return int_property_; }
  [[nodiscard]] base::CallbackListSubscription AddIntPropertyChangedCallback(
      ui::metadata::PropertyChangedCallback callback) {
    return AddPropertyChangedCallback(&int_property_, std::move(callback));
  }

 private:
  int int_property_ = 0;
};

BEGIN_METADATA_BASE(MetadataTestBaseClass)
ADD_PROPERTY_METADATA(int, IntProperty)
END_METADATA

// Descendent class in the simple hierarchy. The inherited properties are
// visible within the metadata.
class MetadataTestClass : public MetadataTestBaseClass {
  METADATA_HEADER(MetadataTestClass, MetadataTestBaseClass)

 public:
  MetadataTestClass() = default;
  ~MetadataTestClass() override = default;

  void SetFloatProperty(float new_value) {
    if (float_property_ == new_value)
      return;
    float_property_ = new_value;
    TriggerChangedCallback(&float_property_);
  }
  float GetFloatProperty() const { return float_property_; }
  [[nodiscard]] base::CallbackListSubscription AddFloatPropertyChangedCallback(
      ui::metadata::PropertyChangedCallback callback) {
    return AddPropertyChangedCallback(&float_property_, std::move(callback));
  }

 private:
  float float_property_ = 0.f;
};

BEGIN_METADATA(MetadataTestClass)
ADD_PROPERTY_METADATA(float, FloatProperty)
END_METADATA

// Test view to which class properties are attached.
class ClassPropertyMetaDataTestClass : public MetadataTestBaseClass {
  METADATA_HEADER(ClassPropertyMetaDataTestClass, MetadataTestBaseClass)

 public:
  ClassPropertyMetaDataTestClass() = default;
  ~ClassPropertyMetaDataTestClass() override = default;
};

// Test view which doesn't have metadata attached.
struct MetadataTestClassNoMetadata : public MetadataTestBaseClass {};

DEFINE_UI_CLASS_PROPERTY_KEY(int, kIntKey, -1)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kOwnedInsetsKey1, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kOwnedInsetsKey2, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(gfx::Insets*, kInsetsKey1, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(gfx::Insets*, kInsetsKey2, nullptr)
DEFINE_UI_CLASS_PROPERTY_TYPE(gfx::Insets*)

BEGIN_METADATA(ClassPropertyMetaDataTestClass)
ADD_CLASS_PROPERTY_METADATA(int, kIntKey)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets, kOwnedInsetsKey1)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets*, kOwnedInsetsKey2)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets, kInsetsKey1)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets*, kInsetsKey2)
END_METADATA

TEST_F(MetadataTest, TestFloatMetadataPropertyAccess) {
  const float start_value = 12.34f;

  MetadataTestClass test_obj;
  test_obj.SetFloatProperty(start_value);

  UM::MemberMetaDataBase* member_data =
      GetMemberMetaData(&test_obj, "FloatProperty");

  ASSERT_TRUE(member_data);
  std::u16string member_value = member_data->GetValueAsString(&test_obj);
  EXPECT_EQ(member_value, base::NumberToString16(start_value));
}

TEST_F(MetadataTest, TestFloatPropertyChangedCallback) {
  const float start_value = 12.34f;

  MetadataTestClass test_obj;
  base::CallbackListSubscription callback =
      test_obj.AddFloatPropertyChangedCallback(base::BindRepeating(
          &MetadataTest::OnFloatPropertyChanged, base::Unretained(this)));

  UM::MemberMetaDataBase* member_data =
      GetMemberMetaData(&test_obj, "FloatProperty");

  ASSERT_TRUE(member_data);

  member_data->SetValueAsString(&test_obj, base::NumberToString16(start_value));

  EXPECT_TRUE(float_property_changed());

  std::u16string member_value = member_data->GetValueAsString(&test_obj);
  EXPECT_EQ(member_value, base::NumberToString16(start_value));
}

TEST_F(MetadataTest, TestMetaDataParentClassTracking) {
  UM::ClassMetaData* base_class_meta_data = MetadataTestBaseClass::MetaData();
  UM::ClassMetaData* derived_class_meta_data = MetadataTestClass::MetaData();

  EXPECT_EQ(base_class_meta_data,
            derived_class_meta_data->parent_class_meta_data());
}

TEST_F(MetadataTest, TestMetaDataFindParentClassMember) {
  UM::ClassMetaData* derived_class_meta_data = MetadataTestClass::MetaData();

  UM::MemberMetaDataBase* member_data =
      derived_class_meta_data->FindMemberData("IntProperty");

  EXPECT_NE(member_data, nullptr);
}

TEST_F(MetadataTest, TestMetaDataMemberIterator) {
  UM::ClassMetaData* derived_class_meta_data = MetadataTestClass::MetaData();

  std::string derived_class_member_name = "IntProperty";
  bool found_derived_class_member = false;

  std::string base_class_member_name = "IntProperty";
  bool found_base_class_member = false;

  for (UM::MemberMetaDataBase* member_data : *derived_class_meta_data) {
    if (member_data->member_name() == derived_class_member_name)
      found_derived_class_member = true;

    if (member_data->member_name() == base_class_member_name)
      found_base_class_member = true;
  }

  EXPECT_TRUE(found_derived_class_member);
  EXPECT_TRUE(found_base_class_member);
}

TEST_F(MetadataTest, TestTypeCacheContainsTestClass) {
  UM::MetaDataCache* cache = UM::MetaDataCache::GetInstance();
  ASSERT_TRUE(cache != nullptr);

  UM::ClassMetaData* test_class_meta = MetadataTestClass::MetaData();

  const auto& cache_meta = cache->GetCachedTypes();
  EXPECT_TRUE(base::Contains(cache_meta, test_class_meta));
}

TEST_F(MetadataTest, TestMetaDataFile) {
  UM::ClassMetaData* metadata = MetadataTestBaseClass::MetaData();

#if defined(__clang__) && defined(_MSC_VER)
  EXPECT_EQ(metadata->file(), "ui\\base\\metadata\\metadata_unittest.cc");
#else
  EXPECT_EQ(metadata->file(), "ui/base/metadata/metadata_unittest.cc");
#endif
}

TEST_F(MetadataTest, TestClassPropertyMetaData) {
  ClassPropertyMetaDataTestClass test_class;
  gfx::Insets insets1(8), insets2 = insets1;

  std::map<std::string, std::u16string> expected_kv = {
      {"kIntKey", u"-1"},
      {"kOwnedInsetsKey1", u"(not assigned)"},
      {"kOwnedInsetsKey2", u"(not assigned)"},
      {"kInsetsKey1", u"(not assigned)"},
      {"kInsetsKey2", u"(not assigned)"}};

  auto verify = [&]() {
    ui::metadata::ClassMetaData* metadata = test_class.GetClassMetaData();
    for (auto member = metadata->begin(); member != metadata->end(); member++) {
      std::string key = (*member)->member_name();
      if (expected_kv.count(key)) {
        EXPECT_EQ((*member)->GetValueAsString(&test_class), expected_kv[key]);
        expected_kv.erase(key);
      }
    }
    EXPECT_TRUE(expected_kv.empty());
  };

  verify();

  test_class.SetProperty(kIntKey, 1);
  test_class.SetProperty(kOwnedInsetsKey1, insets1);
  test_class.SetProperty(kOwnedInsetsKey2, insets1);
  test_class.SetProperty(kInsetsKey1, &insets1);
  test_class.SetProperty(kInsetsKey2, &insets2);

  expected_kv = {{"kIntKey", u"1"},
                 {"kOwnedInsetsKey1", u"8,8,8,8"},
                 {"kOwnedInsetsKey2", u"(assigned)"},
                 {"kInsetsKey1", u"8,8,8,8"},
                 {"kInsetsKey2", u"(assigned)"}};

  verify();
}

TEST_F(MetadataTest, TestHasMetaData) {
  EXPECT_FALSE(UM::kHasClassMetadata<MetadataTestClassNoMetadata>);
  EXPECT_TRUE(UM::kHasClassMetadata<ClassPropertyMetaDataTestClass>);
  EXPECT_TRUE(UM::kHasClassMetadata<ClassPropertyMetaDataTestClass*>);
  EXPECT_TRUE(UM::kHasClassMetadata<ClassPropertyMetaDataTestClass&>);
  EXPECT_TRUE(UM::kHasClassMetadata<const ClassPropertyMetaDataTestClass>);
  EXPECT_TRUE(UM::kHasClassMetadata<const ClassPropertyMetaDataTestClass*>);
  EXPECT_TRUE(UM::kHasClassMetadata<const ClassPropertyMetaDataTestClass&>);
}
