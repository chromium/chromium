// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_menu_model_adapter.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace ui {

TEST(DialogModelMenuModelAdapterTest, ReportsElementIds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kItem1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kItem2Id);
  constexpr char16_t kItem1Text[] = u"Item 1";
  constexpr char16_t kItem2Text[] = u"Item 2";
  constexpr char16_t kItem3Text[] = u"Item 3";

  DialogModelMenuItem::Params item1_params;
  item1_params.SetId(kItem1Id);
  DialogModelMenuItem::Params item2_params;
  item2_params.SetId(kItem2Id);

  auto model = DialogModel::Builder()
                   .AddMenuItem(ImageModel(), kItem1Text, base::DoNothing(),
                                item1_params)
                   .AddMenuItem(ImageModel(), kItem2Text, base::DoNothing(),
                                item2_params)
                   .AddMenuItem(ImageModel(), kItem3Text, base::DoNothing())
                   .Build();
  DialogModelMenuModelAdapter adapter(std::move(model));

  EXPECT_EQ(kItem1Id, adapter.GetElementIdentifierAt(0));
  EXPECT_EQ(kItem2Id, adapter.GetElementIdentifierAt(1));
  EXPECT_EQ(ui::ElementIdentifier(), adapter.GetElementIdentifierAt(2));
}

}  // namespace ui