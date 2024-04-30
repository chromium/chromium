// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_DESIGNER_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_DESIGNER_EXAMPLE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/table_model.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace ui {
class Event;
}

namespace gfx {
class Vector2d;
}

namespace views {

class BoxLayoutView;
class Combobox;
class TableView;

namespace examples {

class DesignerSurface;

class BaseClassRegistration {
 public:
  virtual ~BaseClassRegistration();
  virtual std::unique_ptr<View> CreateView() = 0;
  virtual std::u16string GetViewClassName() = 0;
};

// DesignerExample. Demonstrates a simple visual designer for creating, placing,
// moving and sizing individual views on a surface.
class VIEWS_EXAMPLES_EXPORT DesignerExample : public ExampleBase,
                                              public ui::TableModel,
                                              public ui::ComboboxModel,
                                              public ui::EventHandler {
 public:
  enum GrabHandlePosition : int {
    kTop = 0x01,
    kBottom = 0x02,
    kLeft = 0x04,
    kRight = 0x08,
    kTopLeft = kTop | kLeft,
    kTopRight = kTop | kRight,
    kBottomLeft = kBottom | kLeft,
    kBottomRight = kBottom | kRight,
  };

  static constexpr int kGrabHandleSize = 8;

  class GrabHandles;

  class GrabHandle : public View {
    METADATA_HEADER(GrabHandle, View)

   public:
    GrabHandle(GrabHandles* grab_handles, GrabHandlePosition position);
    GrabHandle(const GrabHandle&) = delete;
    GrabHandle& operator=(const GrabHandle&) = delete;
    ~GrabHandle() override;

    void SetAttachedView(View* view);
    View* attached_view() { return attached_view_; }
    const View* attached_view() const { return attached_view_; }
    GrabHandlePosition position() const { return position_; }
    void UpdatePosition(bool reorder);

   protected:
    // View overrides.
    ui::Cursor GetCursor(const ui::MouseEvent& event) override;
    gfx::Size CalculatePreferredSize(
        const SizeBounds& /*available_size*/) const override;
    void OnPaint(gfx::Canvas* canvas) override;
    bool OnMousePressed(const ui::MouseEvent& event) override;
    bool OnMouseDragged(const ui::MouseEvent& event) override;

   private:
    friend class DesignerExample;
    void PositionOnView();
    void UpdateViewSize();
    static bool IsTop(GrabHandlePosition position);
    static bool IsBottom(GrabHandlePosition position);
    static bool IsLeft(GrabHandlePosition position);
    static bool IsRight(GrabHandlePosition position);

    GrabHandlePosition position_;
    raw_ptr<GrabHandles, DanglingUntriaged> grab_handles_;
    raw_ptr<View> attached_view_ = nullptr;
    gfx::Point mouse_drag_pos_;
  };

  class GrabHandles {
   public:
    GrabHandles();
    ~GrabHandles();

    void Initialize(View* layout_panel);
    void SetAttachedView(View* view);
    bool IsGrabHandle(View* view);

   private:
    std::vector<raw_ptr<GrabHandle, VectorExperimental>> grab_handles_;
  };

  DesignerExample();
  DesignerExample(const DesignerExample&) = delete;
  DesignerExample& operator=(const DesignerExample&) = delete;
  ~DesignerExample() override;

 protected:
  // ExampleBase overrides
  void CreateExampleView(View* container) override;

  // ui::EventHandler overrides
  void OnEvent(ui::Event* event) override;

 private:
  void HandleDesignerMouseEvent(ui::Event* event);
  void SelectView(View* view);
  gfx::Vector2d SnapToGrid(const gfx::Vector2d& distance);
  // Creates the selected view class.
  void CreateView(const ui::Event& event);

  // ui::TableModel overrides
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // ui::ComboboxModel overrides
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

  raw_ptr<BoxLayoutView> designer_container_ = nullptr;
  raw_ptr<DesignerSurface> designer_panel_ = nullptr;
  raw_ptr<View> palette_panel_ = nullptr;

  raw_ptr<Combobox> view_type_ = nullptr;
  raw_ptr<TableView> inspector_ = nullptr;
  raw_ptr<ui::TableModelObserver> model_observer_ = nullptr;

  raw_ptr<View> selected_ = nullptr;
  raw_ptr<View> dragging_ = nullptr;
  gfx::Point last_mouse_pos_;
  std::vector<raw_ptr<ui::metadata::MemberMetaDataBase, VectorExperimental>>
      selected_members_;

  GrabHandles grab_handles_;

  std::vector<std::unique_ptr<BaseClassRegistration>> class_registrations_;

  views::ViewTracker tracker_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_DESIGNER_EXAMPLE_H_
