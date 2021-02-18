// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/metadata_types.h"
#include "ui/views/view.h"

namespace VM = views::metadata;

class MetadataTest : public PlatformTest {
 public:
  MetadataTest() = default;
  ~MetadataTest() override = default;

  bool float_property_changed() const { return float_property_changed_; }
  void OnFloatPropertyChanged() { float_property_changed_ = true; }

 protected:
  template <typename T>
  VM::MemberMetaDataBase* GetMemberMetaData(T* obj,
                                            const std::string& member_name) {
    VM::ClassMetaData* meta_data = obj->GetClassMetaData();
    if (meta_data == nullptr)
      return nullptr;

    VM::MemberMetaDataBase* member_data =
        meta_data->FindMemberData(member_name);
    return member_data;
  }

 private:
  bool float_property_changed_ = false;
};

// Base view in which a simple hierarchy is created for testing metadata
// iteration across class types.
class MetadataTestBaseView : public views::View {
 public:
  MetadataTestBaseView() = default;
  ~MetadataTestBaseView() override = default;

  METADATA_HEADER(MetadataTestBaseView);

  void SetIntProperty(int new_value) {
    if (new_value == int_property_)
      return;
    int_property_ = new_value;
    OnPropertyChanged(&int_property_, views::kPropertyEffectsNone);
  }
  int GetIntProperty() const { return int_property_; }
  base::CallbackListSubscription AddIntPropertyChangedCallback(
      views::PropertyChangedCallback callback) WARN_UNUSED_RESULT {
    return AddPropertyChangedCallback(&int_property_, std::move(callback));
  }

 private:
  int int_property_ = 0;
};

BEGIN_METADATA(MetadataTestBaseView, views::View)
ADD_PROPERTY_METADATA(int, IntProperty)
END_METADATA

// Descendent view in the simple hierarchy. The inherited properties are visible
// within the metadata.
class MetadataTestView : public MetadataTestBaseView {
 public:
  MetadataTestView() = default;
  ~MetadataTestView() override = default;

  METADATA_HEADER(MetadataTestView);

  void SetFloatProperty(float new_value) {
    if (float_property_ == new_value)
      return;
    float_property_ = new_value;
    OnPropertyChanged(&float_property_, views::kPropertyEffectsNone);
  }
  float GetFloatProperty() const { return float_property_; }
  base::CallbackListSubscription AddFloatPropertyChangedCallback(
      views::PropertyChangedCallback callback) WARN_UNUSED_RESULT {
    return AddPropertyChangedCallback(&float_property_, std::move(callback));
  }

 private:
  float float_property_ = 0.f;
};

BEGIN_METADATA(MetadataTestView, MetadataTestBaseView)
ADD_PROPERTY_METADATA(float, FloatProperty)
END_METADATA

// Test view to which class properties are attached.
class ClassPropertyMetaDataTestView : public views::View {
 public:
  ClassPropertyMetaDataTestView() = default;
  ~ClassPropertyMetaDataTestView() override = default;

  METADATA_HEADER(ClassPropertyMetaDataTestView);
};

DEFINE_UI_CLASS_PROPERTY_KEY(int, kIntKey, -1)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kOwnedInsetsKey1, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kOwnedInsetsKey2, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(gfx::Insets*, kInsetsKey1, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(gfx::Insets*, kInsetsKey2, nullptr)

BEGIN_METADATA(ClassPropertyMetaDataTestView, views::View)
ADD_CLASS_PROPERTY_METADATA(int, kIntKey)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets, kOwnedInsetsKey1)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets*, kOwnedInsetsKey2)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets, kInsetsKey1)
ADD_CLASS_PROPERTY_METADATA(gfx::Insets*, kInsetsKey2)
END_METADATA

TEST_F(MetadataTest, TestFloatMetadataPropertyAccess) {
  const float start_value = 12.34f;

  MetadataTestView test_obj;
  test_obj.SetFloatProperty(start_value);

  VM::MemberMetaDataBase* member_data =
      GetMemberMetaData(&test_obj, "FloatProperty");

  ASSERT_TRUE(member_data);
  base::string16 member_value = member_data->GetValueAsString(&test_obj);
  CHECK_EQ(member_value, base::NumberToString16(start_value));
}

TEST_F(MetadataTest, TestFloatPropertyChangedCallback) {
  const float start_value = 12.34f;

  MetadataTestView test_obj;
  base::CallbackListSubscription callback =
      test_obj.AddFloatPropertyChangedCallback(base::BindRepeating(
          &MetadataTest::OnFloatPropertyChanged, base::Unretained(this)));

  VM::MemberMetaDataBase* member_data =
      GetMemberMetaData(&test_obj, "FloatProperty");

  ASSERT_TRUE(member_data);

  member_data->SetValueAsString(&test_obj, base::NumberToString16(start_value));

  CHECK(float_property_changed());

  base::string16 member_value = member_data->GetValueAsString(&test_obj);
  CHECK_EQ(member_value, base::NumberToString16(start_value));
}

TEST_F(MetadataTest, TestMetaDataParentClassTracking) {
  VM::ClassMetaData* base_class_meta_data = MetadataTestBaseView::MetaData();
  VM::ClassMetaData* derived_class_meta_data = MetadataTestView::MetaData();

  CHECK_EQ(base_class_meta_data,
           derived_class_meta_data->parent_class_meta_data());
}

TEST_F(MetadataTest, TestMetaDataFindParentClassMember) {
  VM::ClassMetaData* derived_class_meta_data = MetadataTestView::MetaData();

  VM::MemberMetaDataBase* member_data =
      derived_class_meta_data->FindMemberData("IntProperty");

  CHECK_NE(member_data, nullptr);
}

TEST_F(MetadataTest, TestMetaDataMemberIterator) {
  VM::ClassMetaData* derived_class_meta_data = MetadataTestView::MetaData();

  std::string derived_class_member_name = "IntProperty";
  bool found_derived_class_member = false;

  std::string base_class_member_name = "IntProperty";
  bool found_base_class_member = false;

  for (VM::MemberMetaDataBase* member_data : *derived_class_meta_data) {
    if (member_data->member_name() == derived_class_member_name)
      found_derived_class_member = true;

    if (member_data->member_name() == base_class_member_name)
      found_base_class_member = true;
  }

  CHECK(found_derived_class_member);
  CHECK(found_base_class_member);
}

TEST_F(MetadataTest, TestTypeCacheContainsTestClass) {
  VM::MetaDataCache* cache = VM::MetaDataCache::GetInstance();
  ASSERT_TRUE(cache != nullptr);

  VM::ClassMetaData* test_class_meta = MetadataTestView::MetaData();

  const auto& cache_meta = cache->GetCachedTypes();
  CHECK(std::find(cache_meta.begin(), cache_meta.end(), test_class_meta) !=
        cache_meta.end());
}

TEST_F(MetadataTest, TestMetaDataFile) {
  VM::ClassMetaData* metadata = MetadataTestBaseView::MetaData();

  CHECK_EQ(metadata->file(), "ui/views/metadata/metadata_unittest.cc");
}

TEST_F(MetadataTest, TestClassPropertyMetaData) {
  ClassPropertyMetaDataTestView view;
  gfx::Insets insets1(8, 8, 8, 8), insets2 = insets1;

  std::map<std::string, base::string16> expected_kv = {
      {"kIntKey", base::ASCIIToUTF16("-1")},
      {"kOwnedInsetsKey1", base::ASCIIToUTF16("(not assigned)")},
      {"kOwnedInsetsKey2", base::ASCIIToUTF16("(not assigned)")},
      {"kInsetsKey1", base::ASCIIToUTF16("(not assigned)")},
      {"kInsetsKey2", base::ASCIIToUTF16("(not assigned)")}};

  auto verify = [&]() {
    views::metadata::ClassMetaData* metadata = view.GetClassMetaData();
    for (auto member = metadata->begin(); member != metadata->end(); member++) {
      std::string key = (*member)->member_name();
      if (expected_kv.count(key)) {
        EXPECT_EQ((*member)->GetValueAsString(&view), expected_kv[key]);
        expected_kv.erase(key);
      }
    }
    EXPECT_EQ(expected_kv.empty(), true);
  };

  verify();

  view.SetProperty(kIntKey, 1);
  view.SetProperty(kOwnedInsetsKey1, insets1);
  view.SetProperty(kOwnedInsetsKey2, insets1);
  view.SetProperty(kInsetsKey1, &insets1);
  view.SetProperty(kInsetsKey2, &insets2);

  expected_kv = {{"kIntKey", base::ASCIIToUTF16("1")},
                 {"kOwnedInsetsKey1", base::ASCIIToUTF16("8,8,8,8")},
                 {"kOwnedInsetsKey2", base::ASCIIToUTF16("(assigned)")},
                 {"kInsetsKey1", base::ASCIIToUTF16("8,8,8,8")},
                 {"kInsetsKey2", base::ASCIIToUTF16("(assigned)")}};

  verify();
}
