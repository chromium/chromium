#include "ui/gfx/x/randr_output_manager.h"

#include <optional>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace {

void* ContextOf(int i) {
  return reinterpret_cast<void*>(i);
}

}  // namespace

namespace x11 {

std::ostream& operator<<(std::ostream& os, const RandRMonitorConfig& layout) {
  if (layout.id().has_value()) {
    os << *layout.id() << ": ";
  } else {
    os << "(no screen id): ";
  }
  return os << layout.rect().ToString() << " @ " << layout.dpi().x() << "x"
            << layout.dpi().y();
}

std::ostream& operator<<(
    std::ostream& os,
    const RandRMonitorConfigWithContext& layout_with_context) {
  return os << "{config=" << layout_with_context.config
            << ", context=" << layout_with_context.context << "}";
}

bool operator==(const RandRMonitorConfigWithContext& a,
                const RandRMonitorConfigWithContext& b) {
  return a.config == b.config && a.context == b.context;
}

TEST(RandrOutputManagerTest, CalculateDisplayLayoutDiff) {
  std::vector<RandRMonitorConfigWithContext> current_displays = {
      {{123, gfx::Rect(0, 0, 1230, 1230), gfx::Vector2d(96, 96)}, ContextOf(1)},
      {{234, gfx::Rect(1230, 0, 2340, 2340), gfx::Vector2d(192, 192)},
       ContextOf(2)},
      {{345, gfx::Rect(0, 1230, 3450, 3450), gfx::Vector2d(96, 96)},
       ContextOf(3)}};
  x11::RandRMonitorLayout new_layout(
      {// Updated.
       {234, gfx::Rect(3450, 1230, 2340, 2000), gfx::Vector2d(100, 96)},
       // Unchanged.
       {345, gfx::Rect(0, 1230, 3450, 3450), gfx::Vector2d(96, 96)},
       // New.
       {{}, gfx::Rect(3450, 3450, 4560, 4560), gfx::Vector2d(192, 192)}});
  auto diff = CalculateDisplayLayoutDiff(current_displays, new_layout);

  x11::RandRMonitorLayout expected_new_displays;
  expected_new_displays.configs.push_back(RandRMonitorConfig(
      {}, gfx::Rect(3450, 3450, 4560, 4560), gfx::Vector2d(192, 192)));
  EXPECT_EQ(diff.new_displays, expected_new_displays);

  std::vector<RandRMonitorConfigWithContext> expected_updated_displays = {
      RandRMonitorConfigWithContext(
          RandRMonitorConfig(234, gfx::Rect(3450, 1230, 2340, 2000),
                             gfx::Vector2d(100, 96)),
          ContextOf(2))};
  EXPECT_EQ(diff.updated_displays, expected_updated_displays);

  std::vector<RandRMonitorConfigWithContext> expected_removed_displays = {
      RandRMonitorConfigWithContext(
          RandRMonitorConfig(123, gfx::Rect(0, 0, 1230, 1230),
                             gfx::Vector2d(96, 96)),
          ContextOf(1))};
  EXPECT_EQ(diff.removed_displays, expected_removed_displays);
}

}  // namespace x11
