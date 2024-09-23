// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_list.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace display {
namespace {

class DisplayObserverImpl : public DisplayObserver {
 public:
  DisplayObserverImpl() {}

  DisplayObserverImpl(const DisplayObserverImpl&) = delete;
  DisplayObserverImpl& operator=(const DisplayObserverImpl&) = delete;

  ~DisplayObserverImpl() override {}

  std::string GetAndClearChanges() {
    std::string changes;
    std::swap(changes, changes_);
    return changes;
  }

 private:
  static void AddPartChange(uint32_t changed,
                            uint32_t part,
                            const std::string& description,
                            std::string* changed_string) {
    if ((changed & part) != part)
      return;

    *changed_string += " ";
    *changed_string += description;
  }

  void AddChange(const std::string& change) {
    if (!changes_.empty())
      changes_ += "\n";
    changes_ += change;
  }

  void OnDisplayAdded(const Display& new_display) override {
    AddChange("Added id=" + base::NumberToString(new_display.id()));
  }
  void OnDisplaysRemoved(const Displays& removed_displays) override {
    for (const auto& display : removed_displays) {
      AddChange("Removed id=" + base::NumberToString(display.id()));
    }
  }
  void OnDisplayMetricsChanged(const Display& display,
                               uint32_t changed_metrics) override {
    std::string parts;
    AddPartChange(changed_metrics, DISPLAY_METRIC_BOUNDS, "bounds", &parts);
    AddPartChange(changed_metrics, DISPLAY_METRIC_WORK_AREA, "work_area",
                  &parts);
    AddPartChange(changed_metrics, DISPLAY_METRIC_DEVICE_SCALE_FACTOR,
                  "scale_factor", &parts);
    AddPartChange(changed_metrics, DISPLAY_METRIC_ROTATION, "rotation", &parts);
    AddPartChange(changed_metrics, DISPLAY_METRIC_PRIMARY, "primary", &parts);
    AddPartChange(changed_metrics, DISPLAY_METRIC_LABEL, "label", &parts);

    AddChange("Changed id=" + base::NumberToString(display.id()) + parts);
  }

  std::string changes_;
};

TEST(DisplayListTest, AddUpdateRemove) {
  DisplayList display_list;
  DisplayObserverImpl observer;
  display_list.AddObserver(&observer);
  display_list.AddDisplay(Display(2, gfx::Rect(0, 0, 801, 802)),
                          DisplayList::Type::PRIMARY);
  EXPECT_EQ("Added id=2", observer.GetAndClearChanges());

  // Update the bounds.
  {
    Display updated_display = *(display_list.displays().begin());
    updated_display.set_bounds(gfx::Rect(0, 0, 803, 802));
    display_list.UpdateDisplay(updated_display, DisplayList::Type::PRIMARY);
    EXPECT_EQ("Changed id=2 bounds", observer.GetAndClearChanges());
  }

  // Update the label.
  {
    Display updated_display = *(display_list.displays().begin());
    updated_display.set_label("new_label");
    display_list.UpdateDisplay(updated_display, DisplayList::Type::PRIMARY);
    EXPECT_EQ("Changed id=2 label", observer.GetAndClearChanges());
    EXPECT_EQ("new_label", display_list.FindDisplayById(2)->label());
  }

  // Add another.
  display_list.AddDisplay(Display(3, gfx::Rect(0, 0, 809, 802)),
                          DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ("Added id=3", observer.GetAndClearChanges());
  ASSERT_EQ(2u, display_list.displays().size());
  EXPECT_EQ(2, display_list.displays()[0].id());
  EXPECT_EQ(3, display_list.displays()[1].id());
  EXPECT_EQ(2, display_list.GetPrimaryDisplayIterator()->id());

  // Make the second the primary.
  display_list.UpdateDisplay(display_list.displays()[1],
                             DisplayList::Type::PRIMARY);
  EXPECT_EQ("Changed id=3 primary", observer.GetAndClearChanges());
  EXPECT_EQ(3, display_list.GetPrimaryDisplayIterator()->id());

  // Delete the first.
  display_list.RemoveDisplay(2);
  ASSERT_EQ(1u, display_list.displays().size());
  EXPECT_EQ("Removed id=2", observer.GetAndClearChanges());
  EXPECT_EQ(3, display_list.GetPrimaryDisplayIterator()->id());
}

TEST(DisplayListTest, EmptyIsValid) {
  DisplayList display_list;
  EXPECT_TRUE(display_list.IsValid());
}

TEST(DisplayListTest, FirstDisplayAddedMustBePrimary) {
  DisplayList display_list;
  EXPECT_DCHECK_DEATH(
      display_list.AddDisplay(Display(1), DisplayList::Type::NOT_PRIMARY));
}

TEST(DisplayListTest, DisplaysIdsMustBeUnique) {
  DisplayList display_list;
  display_list.AddDisplay(Display(1), DisplayList::Type::PRIMARY);
  EXPECT_DCHECK_DEATH(
      display_list.AddDisplay(Display(1), DisplayList::Type::PRIMARY));
}

TEST(DisplayListTest, GetPrimaryDisplayEmpty) {
  DisplayList display_list;
  EXPECT_EQ(display_list.displays().end(),
            display_list.GetPrimaryDisplayIterator());
}

TEST(DisplayListTest, GetPrimaryDisplayOk) {
  DisplayList display_list;
  display_list.AddDisplay(Display(1), DisplayList::Type::PRIMARY);
  EXPECT_NE(display_list.displays().end(),
            display_list.GetPrimaryDisplayIterator());
  EXPECT_EQ(1, display_list.GetPrimaryDisplayIterator()->id());
}

}  // namespace
}  // namespace display
