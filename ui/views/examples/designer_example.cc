// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/designer_example.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/event_targeter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"

namespace views::examples {

class DesignerSurface : public View {
  METADATA_HEADER(DesignerSurface, View)

 public:
  explicit DesignerSurface(int grid_size = 8);
  DesignerSurface(const DesignerSurface&) = delete;
  DesignerSurface& operator=(const DesignerSurface&) = delete;
  ~DesignerSurface() override = default;

  int GetGridSize() const { return grid_size_; }
  void SetGridSize(int grid_size);

  // View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  void RebuildGridImage();

  gfx::ImageSkia grid_image_;
  int grid_size_ = 8;
};

DesignerSurface::DesignerSurface(int grid_size) : grid_size_(grid_size) {}

void DesignerSurface::SetGridSize(int grid_size) {
  if (grid_size_ != grid_size) {
    grid_size_ = grid_size;
    if (GetWidget())
      RebuildGridImage();
  }
}

void DesignerSurface::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  canvas->TileImageInt(grid_image_, 0, 0, size().width(), size().height());
}

void DesignerSurface::OnThemeChanged() {
  View::OnThemeChanged();
  RebuildGridImage();
}

void DesignerSurface::RebuildGridImage() {
  const SkColor grid_color =
      GetColorProvider()->GetColor(ExamplesColorIds::kColorDesignerGrid);
  gfx::Size grid_size = grid_size_ <= 8
                            ? gfx::Size(grid_size_ * 8, grid_size_ * 8)
                            : gfx::Size(grid_size_, grid_size_);
  auto grid_canvas = std::make_unique<gfx::Canvas>(grid_size, 1.0, false);
  for (int i = 0; i < grid_size.width(); i += grid_size_) {
    for (int j = 0; j < grid_size.height(); j += grid_size_) {
      grid_canvas->FillRect(gfx::Rect(i, j, 1, 1), grid_color);
    }
  }
  grid_image_ = gfx::ImageSkia::CreateFrom1xBitmap(grid_canvas->GetBitmap());
}

BEGIN_METADATA(DesignerSurface)
ADD_PROPERTY_METADATA(int, GridSize)
END_METADATA

BEGIN_VIEW_BUILDER(/* no export */, DesignerSurface, View)
VIEW_BUILDER_PROPERTY(int, GridSize)
END_VIEW_BUILDER

}  // namespace views::examples

DEFINE_VIEW_BUILDER(/* no export */, views::examples::DesignerSurface)

namespace views::examples {

namespace {

ui::TableColumn MakeColumn(int id, std::u16string title, bool sortable) {
  ui::TableColumn column;
  column.id = id;
  column.title = title;
  column.alignment = ui::TableColumn::LEFT;
  column.width = -1;
  column.percent = 0.5f;
  column.sortable = sortable;
  return column;
}

int SnapToInterval(int value, int interval) {
  return value - (value % interval);
}

template <typename C>
class ClassRegistration : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override { return std::make_unique<C>(); }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(C::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<Combobox> : public BaseClassRegistration,
                                    public ui::ComboboxModel {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<Combobox>()
        .SetModel(this)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_COMBOBOX_NAME))
        .Build();
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(Combobox::MetaData()->type_name());
  }

  // ui::ComboboxModel
  size_t GetItemCount() const override { return 1; }
  std::u16string GetItemAt(size_t index) const override { return u"<empty>"; }
  std::optional<size_t> GetDefaultIndex() const override { return 0; }
};

template <>
class ClassRegistration<MdTextButton> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<MdTextButton>().SetText(u"Button").Build();
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(MdTextButton::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<Textfield> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    const std::u16string text = u"<text field>";
    return Builder<Textfield>()
        .SetText(text)
        .SetDefaultWidthInChars(text.size())
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_TEXTFIELD_NAME))
        .Build();
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(Textfield::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<Checkbox> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return std::make_unique<Checkbox>(u"<Checkbox>");
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(Checkbox::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<RadioButton> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return std::make_unique<RadioButton>(u"<RadioButton>", 0);
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(RadioButton::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<ToggleButton> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<ToggleButton>()
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_IMAGEBUTTON_NAME))
        .Build();
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(ToggleButton::MetaData()->type_name());
  }
};

template <>
class ClassRegistration<ImageButton> : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<ImageButton>()
        .SetImageModel(Button::ButtonState::STATE_NORMAL,
                       ui::ImageModel::FromVectorIcon(kPinIcon, ui::kColorIcon))
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_IMAGEBUTTON_NAME))
        .Build();
  }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(ImageButton::MetaData()->type_name());
  }
};

std::vector<std::unique_ptr<BaseClassRegistration>> GetClassRegistrations() {
  std::vector<std::unique_ptr<BaseClassRegistration>> registrations;
  registrations.push_back(std::make_unique<ClassRegistration<Checkbox>>());
  registrations.push_back(std::make_unique<ClassRegistration<Combobox>>());
  registrations.push_back(std::make_unique<ClassRegistration<MdTextButton>>());
  registrations.push_back(std::make_unique<ClassRegistration<RadioButton>>());
  registrations.push_back(std::make_unique<ClassRegistration<Textfield>>());
  registrations.push_back(std::make_unique<ClassRegistration<ToggleButton>>());
  registrations.push_back(std::make_unique<ClassRegistration<ImageButton>>());
  return registrations;
}

bool IsViewParent(View* parent, View* view) {
  while (view) {
    if (view == parent)
      return true;
    view = view->parent();
  }
  return false;
}

View* GetDesignerChild(View* child_view, View* designer) {
  while (child_view && child_view != designer &&
         child_view->parent() != designer) {
    child_view = child_view->parent();
  }
  return child_view;
}

}  // namespace

BaseClassRegistration::~BaseClassRegistration() = default;

DesignerExample::GrabHandle::GrabHandle(GrabHandles* grab_handles,
                                        GrabHandlePosition position)
    : position_(position), grab_handles_(grab_handles) {}

DesignerExample::GrabHandle::~GrabHandle() = default;

void DesignerExample::GrabHandle::SetAttachedView(View* view) {
  bool was_visible = GetVisible() && attached_view_ != view;
  attached_view_ = view;
  SetVisible(!!attached_view_);
  UpdatePosition(!was_visible);
}

void DesignerExample::GrabHandle::UpdatePosition(bool reorder) {
  if (GetVisible() && attached_view_) {
    PositionOnView();
    if (reorder)
      parent()->ReorderChildView(this, parent()->children().size());
  }
}

ui::Cursor DesignerExample::GrabHandle::GetCursor(const ui::MouseEvent& event) {
  switch (position_) {
    case GrabHandlePosition::kTop:
    case GrabHandlePosition::kBottom:
      return ui::mojom::CursorType::kNorthSouthResize;
    case GrabHandlePosition::kLeft:
    case GrabHandlePosition::kRight:
      return ui::mojom::CursorType::kEastWestResize;
    case GrabHandlePosition::kTopLeft:
    case GrabHandlePosition::kBottomRight:
      return ui::mojom::CursorType::kNorthWestSouthEastResize;
    case GrabHandlePosition::kTopRight:
    case GrabHandlePosition::kBottomLeft:
      return ui::mojom::CursorType::kNorthEastSouthWestResize;
  }
}

gfx::Size DesignerExample::GrabHandle::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return gfx::Size(kGrabHandleSize, kGrabHandleSize);
}

void DesignerExample::GrabHandle::OnPaint(gfx::Canvas* canvas) {
  SkPath path;
  gfx::Point center = GetLocalBounds().CenterPoint();
  path.addCircle(center.x(), center.y(), width() / 2);
  cc::PaintFlags flags;
  flags.setColor(
      GetColorProvider()->GetColor(ExamplesColorIds::kColorDesignerGrabHandle));
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

bool DesignerExample::GrabHandle::OnMousePressed(const ui::MouseEvent& event) {
  mouse_drag_pos_ = event.location();
  return true;
}

bool DesignerExample::GrabHandle::OnMouseDragged(const ui::MouseEvent& event) {
  gfx::Point new_location = event.location();
  if (position_ == GrabHandlePosition::kTop ||
      position_ == GrabHandlePosition::kBottom) {
    new_location.set_x(mouse_drag_pos_.x());
  }
  if (position_ == GrabHandlePosition::kLeft ||
      position_ == GrabHandlePosition::kRight) {
    new_location.set_y(mouse_drag_pos_.y());
  }
  SetPosition(origin() + (new_location - mouse_drag_pos_));
  UpdateViewSize();
  grab_handles_->SetAttachedView(attached_view_);
  return true;
}

void DesignerExample::GrabHandle::PositionOnView() {
  DCHECK(attached_view_);
  gfx::Rect view_bounds = attached_view_->bounds();
  gfx::Point edge_position;
  if (IsTop(position_)) {
    edge_position.set_y(view_bounds.y());
    if (position_ == GrabHandlePosition::kTop)
      edge_position.set_x(view_bounds.top_center().x());
  } else if (IsBottom(position_)) {
    edge_position.set_y(view_bounds.bottom());
    if (position_ == GrabHandlePosition::kBottom)
      edge_position.set_x(view_bounds.bottom_center().x());
  }
  if (IsLeft(position_)) {
    edge_position.set_x(view_bounds.x());
    if (position_ == GrabHandlePosition::kLeft)
      edge_position.set_y(view_bounds.left_center().y());
  } else if (IsRight(position_)) {
    edge_position.set_x(view_bounds.right());
    if (position_ == GrabHandlePosition::kRight)
      edge_position.set_y(view_bounds.right_center().y());
  }
  SetPosition(edge_position - (bounds().CenterPoint() - origin()));
}

void DesignerExample::GrabHandle::UpdateViewSize() {
  DCHECK(attached_view_);
  gfx::Rect view_bounds = attached_view_->bounds();
  gfx::Point view_center = bounds().CenterPoint();
  if (IsTop(position_)) {
    view_bounds.set_height(view_bounds.height() -
                           (view_center.y() - view_bounds.y()));
    view_bounds.set_y(view_center.y());
  }
  if (IsBottom(position_))
    view_bounds.set_height(view_center.y() - view_bounds.y());
  if (IsLeft(position_)) {
    view_bounds.set_width(view_bounds.width() -
                          (view_center.x() - view_bounds.x()));
    view_bounds.set_x(view_center.x());
  }
  if (IsRight(position_))
    view_bounds.set_width(view_center.x() - view_bounds.x());
  attached_view_->SetBoundsRect(view_bounds);
}

// static
bool DesignerExample::GrabHandle::IsTop(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kTop);
}

// static
bool DesignerExample::GrabHandle::IsBottom(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kBottom);
}

// static
bool DesignerExample::GrabHandle::IsLeft(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kLeft);
}

// static
bool DesignerExample::GrabHandle::IsRight(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kRight);
}

BEGIN_METADATA(DesignerExample, GrabHandle)
END_METADATA

DesignerExample::GrabHandles::GrabHandles() = default;

DesignerExample::GrabHandles::~GrabHandles() = default;

void DesignerExample::GrabHandles::Initialize(View* layout_panel) {
  static constexpr GrabHandlePosition positions[] = {
      GrabHandlePosition::kTop,        GrabHandlePosition::kBottom,
      GrabHandlePosition::kLeft,       GrabHandlePosition::kRight,
      GrabHandlePosition::kTopLeft,    GrabHandlePosition::kTopRight,
      GrabHandlePosition::kBottomLeft, GrabHandlePosition::kBottomRight,
  };
  DCHECK(grab_handles_.empty());
  for (GrabHandlePosition position : positions) {
    auto grab_handle = std::make_unique<GrabHandle>(this, position);
    grab_handle->SizeToPreferredSize();
    grab_handle->SetVisible(false);
    grab_handles_.push_back(layout_panel->AddChildView(std::move(grab_handle)));
  }
}

void DesignerExample::GrabHandles::SetAttachedView(View* view) {
  for (GrabHandle* grab_handle : grab_handles_)
    grab_handle->SetAttachedView(view);
}

bool DesignerExample::GrabHandles::IsGrabHandle(View* view) {
  return base::ranges::find(grab_handles_, view) != grab_handles_.end();
}

DesignerExample::DesignerExample() : ExampleBase("Designer") {}

DesignerExample::~DesignerExample() {
  if (tracker_.view())
    inspector_->SetModel(nullptr);
}

void DesignerExample::CreateExampleView(View* container) {
  Builder<View>(container)
      .SetUseDefaultFillLayout(true)
      .AddChildren(
          Builder<BoxLayoutView>()
              .CopyAddressTo(&designer_container_)
              .SetOrientation(BoxLayout::Orientation::kHorizontal)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kEnd)
              .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kStretch)
              .AddChildren(
                  Builder<DesignerSurface>()
                      .CopyAddressTo(&designer_panel_)
                      .CustomConfigure(base::BindOnce(
                          [](DesignerExample* designer_example,
                             DesignerSurface* designer_surface) {
                            designer_surface->AddPreTargetHandler(
                                designer_example);
                          },
                          base::Unretained(this))),
                  Builder<BoxLayoutView>()
                      .CopyAddressTo(&palette_panel_)
                      .SetOrientation(BoxLayout::Orientation::kVertical)
                      .SetInsideBorderInsets(gfx::Insets::TLBR(0, 3, 0, 0))
                      .SetBetweenChildSpacing(3)
                      .AddChildren(
                          Builder<Combobox>()
                              .CopyAddressTo(&view_type_)
                              .SetAccessibleName(u"View Type")
                              .SetModel(this),
                          Builder<MdTextButton>()
                              .SetCallback(base::BindRepeating(
                                  &DesignerExample::CreateView,
                                  base::Unretained(this)))
                              .SetText(u"Add"),
                          TableView::CreateScrollViewBuilderWithTable(
                              Builder<TableView>()
                                  .CopyAddressTo(&inspector_)
                                  .SetColumns({MakeColumn(0, u"Name", true),
                                               MakeColumn(1, u"Value", false)})
                                  .SetModel(this)
                                  .SetTableType(views::TableType::kTextOnly)
                                  .SetSingleSelection(true))
                              .SetPreferredSize(gfx::Size(250, 400)))))
      .BuildChildren();
  grab_handles_.Initialize(designer_panel_);
  designer_container_->SetFlexForView(designer_panel_, 75);
  class_registrations_ = GetClassRegistrations();

  // TODO(crbug.com/40247792): Refactor such that the TableModel is not
  // responsible for managing the lifetimes of views
  tracker_.SetView(inspector_);
}

void DesignerExample::OnEvent(ui::Event* event) {
  if (event->IsMouseEvent() && event->target()) {
    View* view = static_cast<View*>(event->target());
    if (IsViewParent(designer_panel_, view->parent()) ||
        view == designer_panel_) {
      HandleDesignerMouseEvent(event);
      return;
    }
  }
  ui::EventHandler::OnEvent(event);
}

void DesignerExample::HandleDesignerMouseEvent(ui::Event* event) {
  ui::MouseEvent* mouse_event = event->AsMouseEvent();
  switch (mouse_event->type()) {
    case ui::EventType::kMousePressed:
      if (mouse_event->IsOnlyLeftMouseButton()) {
        DCHECK(!dragging_);
        View* event_view = GetDesignerChild(static_cast<View*>(event->target()),
                                            designer_panel_);
        if (grab_handles_.IsGrabHandle(event_view)) {
          dragging_ = event_view;
          return;
        }
        grab_handles_.SetAttachedView(nullptr);
        if (event_view == designer_panel_) {
          SelectView(nullptr);
          return;
        }
        SelectView(event_view);
        dragging_ = selected_;
        last_mouse_pos_ = mouse_event->location();
        event->SetHandled();
        return;
      }
      break;
    case ui::EventType::kMouseDragged:
      if (dragging_) {
        if (grab_handles_.IsGrabHandle(dragging_))
          return;
        gfx::Point new_position =
            selected_->origin() +
            SnapToGrid(mouse_event->location() - last_mouse_pos_);
        new_position.SetToMax(gfx::Point());
        new_position.SetToMin(designer_panel_->GetLocalBounds().bottom_right() -
                              gfx::Vector2d(dragging_->size().width(),
                                            dragging_->size().height()));
        dragging_->SetPosition(new_position);
        event->SetHandled();
        return;
      }
      break;
    case ui::EventType::kMouseReleased:
      grab_handles_.SetAttachedView(selected_);
      if (dragging_) {
        bool dragging_handle = grab_handles_.IsGrabHandle(dragging_);
        dragging_ = nullptr;
        if (!dragging_handle)
          event->SetHandled();
        return;
      }
      break;
    default:
      return;
  }
}

void DesignerExample::SelectView(View* view) {
  if (view != selected_) {
    selected_ = view;
    selected_members_.clear();
    if (selected_) {
      for (auto* member : *selected_->GetClassMetaData())
        selected_members_.push_back(member);
    }
    if (model_observer_)
      model_observer_->OnModelChanged();
  }
}

gfx::Vector2d DesignerExample::SnapToGrid(const gfx::Vector2d& distance) {
  return gfx::Vector2d(
      SnapToInterval(distance.x(), designer_panel_->GetGridSize()),
      SnapToInterval(distance.y(), designer_panel_->GetGridSize()));
}

void DesignerExample::CreateView(const ui::Event& event) {
  std::unique_ptr<View> new_view =
      class_registrations_[view_type_->GetSelectedRow().value()]->CreateView();
  new_view->SizeToPreferredSize();
  gfx::Rect child_rect = designer_panel_->GetContentsBounds();
  child_rect.ClampToCenteredSize(new_view->size());
  child_rect.set_origin(gfx::Point() +
                        SnapToGrid(child_rect.OffsetFromOrigin()));
  new_view->SetBoundsRect(child_rect);
  designer_panel_->AddChildView(std::move(new_view));
}

size_t DesignerExample::RowCount() {
  return selected_ ? selected_members_.size() : 0;
}

std::u16string DesignerExample::GetText(size_t row, int column_id) {
  if (selected_) {
    ui::metadata::MemberMetaDataBase* member = selected_members_[row];
    if (column_id == 0)
      return base::ASCIIToUTF16(member->member_name());
    return member->GetValueAsString(selected_);
  }
  return std::u16string();
}

void DesignerExample::SetObserver(ui::TableModelObserver* observer) {
  model_observer_ = observer;
}

size_t DesignerExample::GetItemCount() const {
  return class_registrations_.size();
}

std::u16string DesignerExample::GetItemAt(size_t index) const {
  return class_registrations_[index]->GetViewClassName();
}

std::optional<size_t> DesignerExample::GetDefaultIndex() const {
  return 0;
}

}  // namespace views::examples
