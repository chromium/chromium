// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/badge_example.h"

#include <memory>
#include <set>
#include <utility>

#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/badge.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

BadgeExample::BadgeExample()
    : ExampleBase(GetStringUTF8(IDS_BADGE_SELECT_LABEL).c_str()) {}

BadgeExample::~BadgeExample() = default;

void BadgeExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);

  auto show_menu = [](BadgeExample* example) {
    // Create a menu item view.
    auto menu_item_view =
        std::make_unique<views::MenuItemView>(&example->menu_delegate_);

    // Add items to the context menu.
    menu_item_view->AppendMenuItem(1, GetStringUTF16(IDS_BADGE_MENU_ITEM_1));
    menu_item_view->AppendMenuItem(2, GetStringUTF16(IDS_BADGE_MENU_ITEM_2));
    // Enable the "New" Badge.
    menu_item_view->AppendMenuItem(3, GetStringUTF16(IDS_BADGE_MENU_ITEM_3))
        ->set_is_new(true);

    example->menu_runner_ =
        std::make_unique<MenuRunner>(std::move(menu_item_view), 0);

    View* menu_button = example->menu_button_;
    gfx::Point screen_loc;
    views::View::ConvertPointToScreen(menu_button, &screen_loc);
    gfx::Rect bounds(screen_loc, menu_button->size());

    example->menu_runner_->RunMenuAt(menu_button->GetWidget(), nullptr, bounds,
                                     MenuAnchorPosition::kTopLeft,
                                     ui::MENU_SOURCE_NONE);
  };

  auto view =
      Builder<BoxLayoutView>()
          .SetOrientation(BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets(10))
          .SetBetweenChildSpacing(10)
          .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kStart)
          .AddChildren(
              Builder<BoxLayoutView>().SetBetweenChildSpacing(10).AddChildren(
                  Builder<Link>().SetText(GetStringUTF16(IDS_BADGE_LINK_TEXT)),
                  Builder<Badge>().SetText(
                      GetStringUTF16(IDS_BADGE_BADGE_TEXT))),
              Builder<MdTextButton>()
                  .CopyAddressTo(&menu_button_)
                  .SetText(GetStringUTF16(IDS_BADGE_MENU_BUTTON))
                  .SetCallback(
                      base::BindRepeating(show_menu, base::Unretained(this))))
          .Build();

  container->AddChildView(std::move(view));
}

}  // namespace views::examples
