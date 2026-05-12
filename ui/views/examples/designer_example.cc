// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/designer_example.h"

#include <ranges>
#include <utility>

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/event_targeter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
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

DesignerSurface::DesignerSurface(int grid_size) : grid_size_(grid_size) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  GetViewAccessibility().SetName(u"Designer Surface");
}

void DesignerSurface::SetGridSize(int grid_size) {
  if (grid_size_ != grid_size) {
    grid_size_ = grid_size;
    if (GetWidget()) {
      RebuildGridImage();
    }
  }
}

void DesignerSurface::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  canvas->TileImageInt(grid_image_, 0, 0, size().width(), size().height());
}

void DesignerSurface::OnThemeChanged() {
  View::OnThemeChanged();
  RebuildGridImage();
}

void DesignerSurface::RebuildGridImage() {
  const SkColor grid_color =
      GetColorProvider()->GetColor(ExamplesColorIds::kColorDesignerGrid);
  auto grid_canvas = std::make_unique<gfx::Canvas>(
      gfx::Size(grid_size_, grid_size_), 1.0f, false);
  grid_canvas->FillRect(gfx::Rect(0, 0, 1, 1), grid_color);
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

void RemoveViewAndChildrenFromTracking(View* view, std::set<View*>& tracking) {
  for (View* child : view->children()) {
    RemoveViewAndChildrenFromTracking(child, tracking);
  }
  tracking.erase(view);
}

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

template <typename C, bool is_container = false>
class ClassRegistration : public BaseClassRegistration {
 public:
  ClassRegistration() = default;
  ~ClassRegistration() override = default;
  std::unique_ptr<View> CreateView() override { return std::make_unique<C>(); }
  std::u16string GetViewClassName() override {
    return base::ASCIIToUTF16(C::kViewClassName);
  }
  bool IsContainer() const override { return is_container; }
};

class ClassRegistrationCombobox : public ClassRegistration<Combobox>,
                                  public ui::ComboboxModel {
 public:
  ClassRegistrationCombobox() = default;
  ~ClassRegistrationCombobox() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<Combobox>()
        .SetModel(this)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_COMBOBOX_NAME))
        .Build();
  }

  // ui::ComboboxModel
  size_t GetItemCount() const override { return 1; }
  std::u16string GetItemAt(size_t index) const override { return u"<empty>"; }
  std::optional<size_t> GetDefaultIndex() const override { return 0; }
};

class ClassRegistrationMdTextButton : public ClassRegistration<MdTextButton> {
 public:
  ClassRegistrationMdTextButton() = default;
  ~ClassRegistrationMdTextButton() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<MdTextButton>().SetText(u"Button").Build();
  }
};

class ClassRegistrationTextfield : public ClassRegistration<Textfield> {
 public:
  ClassRegistrationTextfield() = default;
  ~ClassRegistrationTextfield() override = default;
  std::unique_ptr<View> CreateView() override {
    const std::u16string text = u"<text field>";
    return Builder<Textfield>()
        .SetText(text)
        .SetDefaultWidthInChars(text.size())
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_TEXTFIELD_NAME))
        .Build();
  }
};

class ClassRegistrationCheckbox : public ClassRegistration<Checkbox> {
 public:
  ClassRegistrationCheckbox() = default;
  ~ClassRegistrationCheckbox() override = default;
  std::unique_ptr<View> CreateView() override {
    return std::make_unique<Checkbox>(u"<Checkbox>");
  }
};

class ClassRegistrationRadioButton : public ClassRegistration<RadioButton> {
 public:
  ClassRegistrationRadioButton() = default;
  ~ClassRegistrationRadioButton() override = default;
  std::unique_ptr<View> CreateView() override {
    return std::make_unique<RadioButton>(u"<RadioButton>", 0);
  }
};

class ClassRegistrationToggleButton : public ClassRegistration<ToggleButton> {
 public:
  ClassRegistrationToggleButton() = default;
  ~ClassRegistrationToggleButton() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<ToggleButton>()
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_IMAGEBUTTON_NAME))
        .Build();
  }
};

class ClassRegistrationImageButton : public ClassRegistration<ImageButton> {
 public:
  ClassRegistrationImageButton() = default;
  ~ClassRegistrationImageButton() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<ImageButton>()
        .SetImageModel(
            Button::ButtonState::STATE_NORMAL,
            ui::ImageModel::FromVectorIcon(kPinOldIcon, ui::kColorIcon))
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_DESIGNER_IMAGEBUTTON_NAME))
        .Build();
  }
};

class ClassRegistrationBoxLayoutView
    : public ClassRegistration<BoxLayoutView, true> {
 public:
  ClassRegistrationBoxLayoutView() = default;
  ~ClassRegistrationBoxLayoutView() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<BoxLayoutView>()
        .SetBackground(CreateSolidBackground(SkColorSetA(SK_ColorGRAY, 128)))
        .SetPreferredSize(gfx::Size(100, 100))
        .Build();
  }
};

class ClassRegistrationFlexLayoutView
    : public ClassRegistration<FlexLayoutView, true> {
 public:
  ClassRegistrationFlexLayoutView() = default;
  ~ClassRegistrationFlexLayoutView() override = default;
  std::unique_ptr<View> CreateView() override {
    return Builder<FlexLayoutView>()
        .SetBackground(CreateSolidBackground(SkColorSetA(SK_ColorGRAY, 128)))
        .SetPreferredSize(gfx::Size(100, 100))
        .Build();
  }
};

std::vector<std::unique_ptr<BaseClassRegistration>> GetClassRegistrations() {
  std::vector<std::unique_ptr<BaseClassRegistration>> registrations;
  registrations.push_back(std::make_unique<ClassRegistrationBoxLayoutView>());
  registrations.push_back(std::make_unique<ClassRegistrationCheckbox>());
  registrations.push_back(std::make_unique<ClassRegistrationCombobox>());
  registrations.push_back(std::make_unique<ClassRegistrationFlexLayoutView>());
  registrations.push_back(std::make_unique<ClassRegistrationMdTextButton>());
  registrations.push_back(std::make_unique<ClassRegistrationRadioButton>());
  registrations.push_back(std::make_unique<ClassRegistrationTextfield>());
  registrations.push_back(std::make_unique<ClassRegistrationToggleButton>());
  registrations.push_back(std::make_unique<ClassRegistrationImageButton>());
  return registrations;
}

bool IsViewParent(View* parent, View* view) {
  while (view) {
    if (view == parent) {
      return true;
    }
    view = view->parent();
  }
  return false;
}

View* GetDesignerChild(View* child_view,
                       View* designer,
                       const std::set<View*>& designer_views) {
  while (child_view && child_view != designer &&
         !designer_views.count(child_view)) {
    child_view = child_view->parent();
  }
  return child_view;
}

}  // namespace

// InPlaceEditor provides a base for editing properties within the TableView.
class InPlaceEditor : public View, public TextfieldController {
  METADATA_HEADER(InPlaceEditor, View)

 public:
  explicit InPlaceEditor(DesignerPropertyEditor* editor,
                         base::OnceClosure on_commit)
      : editor_(editor), on_commit_(std::move(on_commit)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
    GetViewAccessibility().SetName(editor->GetPropertyName());
  }

  ~InPlaceEditor() override = default;

  virtual void Commit() {
    if (!on_commit_.is_null()) {
      std::move(on_commit_).Run();
    }
  }

 protected:
  raw_ptr<DesignerPropertyEditor> editor_;
  base::OnceClosure on_commit_;
};

BEGIN_METADATA(InPlaceEditor)
END_METADATA

class TextInPlaceEditor : public InPlaceEditor {
  METADATA_HEADER(TextInPlaceEditor, InPlaceEditor)

 public:
  TextInPlaceEditor(DesignerPropertyEditor* editor, base::OnceClosure on_commit)
      : InPlaceEditor(editor, std::move(on_commit)) {
    Builder<View>(this)
        .SetUseDefaultFillLayout(true)
        .AddChildren(Builder<Textfield>()
                         .CopyAddressTo(&textfield_)
                         .SetAccessibleName(editor->GetPropertyName())
                         .SetText(editor->GetValueAsString())
                         .SetController(this)
                         .SetReadOnly(editor->IsReadOnly()))
        .BuildChildren();
  }

  void RequestFocus() override {
    if (textfield_->GetEnabled()) {
      textfield_->RequestFocus();
    } else {
      InPlaceEditor::RequestFocus();
    }
  }

  void Commit() override {
    if (!on_commit_.is_null() && !editor_->IsReadOnly()) {
      editor_->SetValueFromString(std::u16string(textfield_->GetText()));
    }
    InPlaceEditor::Commit();
  }

  // TextfieldController overrides:
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::EventType::kKeyPressed) {
      if (key_event.key_code() == ui::VKEY_RETURN) {
        Commit();
        return true;
      }
      if (key_event.key_code() == ui::VKEY_ESCAPE) {
        // Cancel: run callback without saving.
        if (!on_commit_.is_null()) {
          std::move(on_commit_).Run();
        }
        return true;
      }
    }
    return false;
  }

 private:
  raw_ptr<Textfield> textfield_;
};

BEGIN_METADATA(TextInPlaceEditor)
END_METADATA

class ComboInPlaceEditor : public InPlaceEditor {
  METADATA_HEADER(ComboInPlaceEditor, InPlaceEditor)

 public:
  ComboInPlaceEditor(DesignerPropertyEditor* editor,
                     base::OnceClosure on_commit)
      : InPlaceEditor(editor, std::move(on_commit)) {
    SetUseDefaultFillLayout(true);
    std::vector<ui::SimpleComboboxModel::Item> items;
    for (const auto& val : editor->GetComboboxValues()) {
      items.emplace_back(val);
    }
    auto model = std::make_unique<ui::SimpleComboboxModel>(std::move(items));
    combobox_ = AddChildView(std::make_unique<EditableCombobox>(
        std::move(model), /*filter_on_edit=*/false));
    combobox_->GetViewAccessibility().SetName(editor->GetPropertyName());
    combobox_->SetText(editor->GetValueAsString());
    combobox_->SetCallback(base::BindRepeating(
        &ComboInPlaceEditor::OnContentsChanged, base::Unretained(this)));
    last_text_length_ = combobox_->GetText().length();
    if (editor->IsReadOnly()) {
      combobox_->SetEnabled(false);
    }
  }

  void RequestFocus() override {
    if (combobox_->GetEnabled()) {
      static_cast<View*>(combobox_)->RequestFocus();
    } else {
      InPlaceEditor::RequestFocus();
    }
  }

  void Commit() override {
    if (!on_commit_.is_null() && !editor_->IsReadOnly()) {
      editor_->SetValueFromString(std::u16string(combobox_->GetText()));
    }
    InPlaceEditor::Commit();
  }

  // View overrides:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() == ui::EventType::kKeyPressed) {
      if (event->key_code() == ui::VKEY_RETURN) {
        Commit();
        event->SetHandled();
      } else if (event->key_code() == ui::VKEY_ESCAPE) {
        if (!on_commit_.is_null()) {
          std::move(on_commit_).Run();
        }
        event->SetHandled();
      }
    }
  }

  void OnContentsChanged() {
    std::u16string current_text = std::u16string(combobox_->GetText());

    // If it's a full match, save it immediately.
    auto values = editor_->GetComboboxValues();
    if (std::ranges::find(values, current_text) != values.end()) {
      if (!editor_->IsReadOnly()) {
        editor_->SetValueFromString(current_text);
      }
    }

    if (current_text.empty() || current_text.length() <= last_text_length_) {
      last_text_length_ = current_text.length();
      return;
    }

    last_text_length_ = current_text.length();

    // Autocomplete logic: find first match that starts with current text.
    for (const auto& val : values) {
      if (base::StartsWith(val, current_text,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        if (val.length() > current_text.length()) {
          combobox_->SetText(val);
          combobox_->SelectRange(
              gfx::Range(current_text.length(), val.length()));
          last_text_length_ = val.length();
        }
        break;
      }
    }
  }

 private:
  raw_ptr<EditableCombobox> combobox_;
  size_t last_text_length_ = 0;
};

BEGIN_METADATA(ComboInPlaceEditor)
END_METADATA

class CheckboxInPlaceEditor : public InPlaceEditor {
  METADATA_HEADER(CheckboxInPlaceEditor, InPlaceEditor)

 public:
  CheckboxInPlaceEditor(DesignerPropertyEditor* editor,
                        base::OnceClosure on_commit)
      : InPlaceEditor(editor, std::move(on_commit)) {
    Builder<View>(this)
        .SetLayoutManager(std::make_unique<BoxLayout>(
            BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0))
        .CustomConfigure(base::BindOnce([](View* editor_view) {
          auto* layout =
              static_cast<BoxLayout*>(editor_view->GetLayoutManager());
          layout->set_main_axis_alignment(
              BoxLayout::MainAxisAlignment::kCenter);
          layout->set_cross_axis_alignment(
              BoxLayout::CrossAxisAlignment::kCenter);
        }))
        .AddChildren(
            Builder<Checkbox>()
                .CopyAddressTo(&checkbox_)
                .SetAccessibleName(editor->GetPropertyName())
                .SetChecked(editor->GetValueAsString() == u"true")
                .SetCallback(base::BindRepeating(
                    &CheckboxInPlaceEditor::OnToggled, base::Unretained(this)))
                .SetEnabled(!editor->IsReadOnly()))
        .BuildChildren();
  }

  void RequestFocus() override {
    if (checkbox_->GetEnabled()) {
      checkbox_->RequestFocus();
    } else {
      InPlaceEditor::RequestFocus();
    }
  }

  void Commit() override {
    if (!on_commit_.is_null() && !editor_->IsReadOnly()) {
      editor_->SetValueFromString(checkbox_->GetChecked() ? u"true" : u"false");
    }
    InPlaceEditor::Commit();
  }

 private:
  void OnToggled() {
    if (!editor_->IsReadOnly()) {
      editor_->SetValueFromString(checkbox_->GetChecked() ? u"true" : u"false");
    }
  }

  raw_ptr<Checkbox> checkbox_;
};

BEGIN_METADATA(CheckboxInPlaceEditor)
END_METADATA

BaseClassRegistration::~BaseClassRegistration() = default;

DesignerExample::GrabHandle::GrabHandle(GrabHandles* grab_handles,
                                        GrabHandlePosition position)
    : position_(position), grab_handles_(grab_handles) {}

DesignerExample::GrabHandle::~GrabHandle() = default;

void DesignerExample::GrabHandle::SetAttachedView(View* view) {
  attached_view_ = view;
  if (attached_view_) {
    PositionOnView();
  }
  SetVisible(!!attached_view_);
}

void DesignerExample::GrabHandle::UpdatePosition(bool reorder) {
  if (attached_view_) {
    PositionOnView();
    if (reorder) {
      parent()->ReorderChildView(this, -1);
    }
  }
}

ui::Cursor DesignerExample::GrabHandle::GetCursor(const ui::MouseEvent& event) {
  if (!attached_view_ ||
      attached_view_->parent() != grab_handles_->layout_panel()) {
    return ui::mojom::CursorType::kNull;
  }
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
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetColorProvider()->GetColor(ui::kColorAccent));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(GetLocalBounds().CenterPoint()),
                     kGrabHandleSize / 2.0f, flags);
}

bool DesignerExample::GrabHandle::OnMousePressed(const ui::MouseEvent& event) {
  mouse_drag_pos_ = event.location();
  views::View::ConvertPointToTarget(this, parent(), &mouse_drag_pos_);
  return true;
}

bool DesignerExample::GrabHandle::OnMouseDragged(const ui::MouseEvent& event) {
  if (!attached_view_ ||
      attached_view_->parent() != grab_handles_->layout_panel()) {
    return true;
  }
  gfx::Point mouse_in_parent = event.location();
  views::View::ConvertPointToTarget(this, parent(), &mouse_in_parent);

  gfx::Vector2d delta = mouse_in_parent - mouse_drag_pos_;
  if (delta.IsZero()) {
    return true;
  }

  gfx::Rect view_bounds = attached_view_->bounds();
  if (IsTop(position_)) {
    view_bounds.set_y(view_bounds.y() + delta.y());
    view_bounds.set_height(view_bounds.height() - delta.y());
  }
  if (IsBottom(position_)) {
    view_bounds.set_height(view_bounds.height() + delta.y());
  }
  if (IsLeft(position_)) {
    view_bounds.set_x(view_bounds.x() + delta.x());
    view_bounds.set_width(view_bounds.width() - delta.x());
  }
  if (IsRight(position_)) {
    view_bounds.set_width(view_bounds.width() + delta.x());
  }
  attached_view_->SetBoundsRect(view_bounds);
  grab_handles_->UpdatePositions();
  mouse_drag_pos_ = mouse_in_parent;
  return true;
}

void DesignerExample::GrabHandle::PositionOnView() {
  DCHECK(attached_view_);
  gfx::RectF view_bounds_f(attached_view_->GetLocalBounds());
  views::View::ConvertRectToTarget(attached_view_, parent(), &view_bounds_f);
  gfx::Rect view_bounds = gfx::ToEnclosingRect(view_bounds_f);
  gfx::Point edge_position;
  if (IsTop(position_)) {
    edge_position.set_y(view_bounds.y());
    if (position_ == GrabHandlePosition::kTop) {
      edge_position.set_x(view_bounds.top_center().x());
    }
  }
  if (IsBottom(position_)) {
    edge_position.set_y(view_bounds.bottom());
    if (position_ == GrabHandlePosition::kBottom) {
      edge_position.set_x(view_bounds.bottom_center().x());
    }
  }
  if (IsLeft(position_)) {
    edge_position.set_x(view_bounds.x());
    if (position_ == GrabHandlePosition::kLeft) {
      edge_position.set_y(view_bounds.left_center().y());
    }
  }
  if (IsRight(position_)) {
    edge_position.set_x(view_bounds.right());
    if (position_ == GrabHandlePosition::kRight) {
      edge_position.set_y(view_bounds.right_center().y());
    }
  }
  if (position_ == GrabHandlePosition::kTopLeft) {
    edge_position = view_bounds.origin();
  }
  if (position_ == GrabHandlePosition::kTopRight) {
    edge_position = view_bounds.top_right();
  }
  if (position_ == GrabHandlePosition::kBottomLeft) {
    edge_position = view_bounds.bottom_left();
  }
  if (position_ == GrabHandlePosition::kBottomRight) {
    edge_position = view_bounds.bottom_right();
  }
  gfx::Rect handle_bounds(edge_position, GetPreferredSize({}));
  handle_bounds.Offset(-kGrabHandleSize / 2, -kGrabHandleSize / 2);
  SetBoundsRect(handle_bounds);
}

void DesignerExample::GrabHandle::UpdateViewSize() {
  gfx::Rect view_bounds = attached_view_->bounds();
  gfx::Point view_center = bounds().CenterPoint();
  if (IsTop(position_)) {
    view_bounds.set_height(view_bounds.height() -
                           (view_center.y() - view_bounds.y()));
    view_bounds.set_y(view_center.y());
  }
  if (IsBottom(position_)) {
    view_bounds.set_height(view_center.y() - view_bounds.y());
  }
  if (IsLeft(position_)) {
    view_bounds.set_width(view_bounds.width() -
                          (view_center.x() - view_bounds.x()));
    view_bounds.set_x(view_center.x());
  }
  if (IsRight(position_)) {
    view_bounds.set_width(view_center.x() - view_bounds.x());
  }
  attached_view_->SetBoundsRect(view_bounds);
  PositionOnView();
}

// static
bool DesignerExample::GrabHandle::IsTop(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kTop) != 0;
}

// static
bool DesignerExample::GrabHandle::IsBottom(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kBottom) != 0;
}

// static
bool DesignerExample::GrabHandle::IsLeft(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kLeft) != 0;
}

// static
bool DesignerExample::GrabHandle::IsRight(GrabHandlePosition position) {
  return (position & GrabHandlePosition::kRight) != 0;
}

BEGIN_METADATA(DesignerExample, GrabHandle)
END_METADATA

DesignerExample::GrabHandles::GrabHandles() = default;

DesignerExample::GrabHandles::~GrabHandles() = default;

void DesignerExample::GrabHandles::Initialize(View* layout_panel) {
  layout_panel_ = layout_panel;
  static constexpr GrabHandlePosition positions[] = {
      GrabHandlePosition::kTop,        GrabHandlePosition::kBottom,
      GrabHandlePosition::kLeft,       GrabHandlePosition::kRight,
      GrabHandlePosition::kTopLeft,    GrabHandlePosition::kTopRight,
      GrabHandlePosition::kBottomLeft, GrabHandlePosition::kBottomRight,
  };
  for (auto position : positions) {
    grab_handles_.push_back(layout_panel->AddChildView(
        std::make_unique<GrabHandle>(this, position)));
  }
}

void DesignerExample::GrabHandles::SetAttachedView(View* view) {
  // Suppress handles for the top-level container.
  if (view == layout_panel_) {
    view = nullptr;
  }
  for (auto handle : grab_handles_) {
    handle->SetAttachedView(view);
  }
}

void DesignerExample::GrabHandles::UpdatePositions() {
  for (auto handle : grab_handles_) {
    handle->UpdatePosition(false);
  }
}

bool DesignerExample::GrabHandles::IsGrabHandle(View* view) {
  return std::ranges::find(grab_handles_, view) != grab_handles_.end();
}

DesignerExample::DesignerExample() : ExampleBase("Designer") {}

DesignerExample::~DesignerExample() {
  inspector_->SetModel(nullptr);
}

void DesignerExample::CreateExampleView(View* container) {
  Builder<View>(container)
      .SetUseDefaultFillLayout(true)
      .AddChildren(
          Builder<BoxLayoutView>()
              .CopyAddressTo(&designer_container_)
              .SetOrientation(BoxLayout::Orientation::kHorizontal)
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
                      .CustomConfigure(base::BindOnce(
                          [](DesignerExample* designer_example,
                             BoxLayoutView* palette_panel) {
                            palette_panel->AddPreTargetHandler(
                                designer_example);
                          },
                          base::Unretained(this)))
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
                                  .SetSingleSelection(true)
                                  .SetObserver(this))
                              .CopyAddressTo(&inspector_scroller_)
                              .SetPreferredSize(gfx::Size(250, 400)))))
      .BuildChildren();
  grab_handles_.Initialize(designer_panel_);
  designer_container_->SetFlexForView(designer_panel_, 75);
  palette_panel_->SetFlexForView(inspector_scroller_, 1);
  class_registrations_ = GetClassRegistrations();

  // TODO(crbug.com/40247792): Refactor such that the TableModel is not
  // responsible for managing the lifetimes of views
  tracker_.SetView(inspector_);
}

void DesignerExample::OnEvent(ui::Event* event) {
  if (event->IsKeyEvent()) {
    ui::KeyEvent* key_event = event->AsKeyEvent();
    if (key_event->type() == ui::EventType::kKeyPressed &&
        key_event->key_code() == ui::VKEY_ESCAPE && selected_) {
      if (selected_ == designer_panel_) {
        SelectView(nullptr);
      } else {
        View* parent_view = GetDesignerChild(selected_->parent(),
                                             designer_panel_, designer_views_);
        SelectView(parent_view);
      }
      grab_handles_.SetAttachedView(selected_);
      event->SetHandled();
      return;
    }
    if (key_event->type() == ui::EventType::kKeyPressed &&
        key_event->key_code() == ui::VKEY_DELETE && selected_ &&
        selected_ != designer_panel_) {
      if (active_inplace_editor_) {
        active_inplace_editor_->Commit();
      }

      View* to_delete = selected_;
      View* parent = to_delete->parent();

      // Determine next selection.
      View* next_selection = nullptr;
      auto children = parent->children();
      auto it = std::ranges::find(children, to_delete);
      if (it != children.end()) {
        if (std::next(it) != children.end()) {
          next_selection = *std::next(it);
        } else if (it != children.begin()) {
          next_selection = *std::prev(it);
        }
      }

      if (!next_selection || grab_handles_.IsGrabHandle(next_selection)) {
        next_selection = parent;
      }

      if (dragging_ == to_delete) {
        dragging_ = nullptr;
      }

      RemoveViewAndChildrenFromTracking(to_delete, designer_views_);
      parent->RemoveChildViewT(to_delete);

      SelectView(next_selection);
      grab_handles_.SetAttachedView(selected_);

      event->SetHandled();
      return;
    }
    if (key_event->type() == ui::EventType::kKeyPressed &&
        (key_event->key_code() == ui::VKEY_UP ||
         key_event->key_code() == ui::VKEY_DOWN) &&
        active_inplace_editor_) {
      if (key_event->key_code() == ui::VKEY_DOWN && key_event->IsAltDown()) {
        // Let it pass through to open the combobox.
      } else {
        std::optional<size_t> row = inspector_->GetFirstSelectedRow();
        if (row.has_value()) {
          int delta = (key_event->key_code() == ui::VKEY_UP) ? -1 : 1;
          int next_row = static_cast<int>(row.value()) + delta;
          if (next_row >= 0 &&
              next_row < static_cast<int>(inspector_->GetRowCount())) {
            active_inplace_editor_->Commit();
            inspector_->Select(static_cast<size_t>(next_row));
            event->SetHandled();
            return;
          }
        }
      }
    }
    if (key_event->type() == ui::EventType::kKeyPressed &&
        (key_event->key_code() == ui::VKEY_LEFT ||
         key_event->key_code() == ui::VKEY_RIGHT)) {
      std::optional<size_t> row = inspector_->GetFirstSelectedRow();
      if (row.has_value()) {
        auto editor = display_list_[row.value()];
        if (editor->IsExpandable()) {
          bool expand = key_event->key_code() == ui::VKEY_RIGHT;
          if (editor->IsExpanded() != expand) {
            editor->SetExpanded(expand);
            RefreshDisplayList();
            if (model_observer_) {
              model_observer_->OnModelChanged();
            }
            inspector_->Select(row.value());
            event->SetHandled();
            return;
          }
        }
      }
    }
  }
  if (event->IsMouseEvent() && event->target()) {
    View* view = static_cast<View*>(event->target());
    if (view == inspector_) {
      ui::MouseEvent* mouse_event = event->AsMouseEvent();
      if (mouse_event->type() == ui::EventType::kMousePressed &&
          mouse_event->IsOnlyLeftMouseButton()) {
        gfx::Point point = mouse_event->location();
        int row = point.y() / inspector_->GetRowHeight();
        if (row >= 0 && row < static_cast<int>(display_list_.size())) {
          // Check if clicking in the first column (Name)
          const auto& visible_columns = inspector_->visible_columns();
          if (point.x() < visible_columns[0].width) {
            auto editor = display_list_[row];
            if (editor->IsExpandable()) {
              editor->SetExpanded(!editor->IsExpanded());
              RefreshDisplayList();
              if (model_observer_) {
                model_observer_->OnModelChanged();
              }
              inspector_->Select(row);
              event->SetHandled();
              return;
            }
          }
        }
      }
    }
    if (IsViewParent(designer_panel_, view->parent()) ||
        view == designer_panel_) {
      HandleDesignerMouseEvent(event);
    }
  }
}

void DesignerExample::HandleDesignerMouseEvent(ui::Event* event) {
  ui::MouseEvent* mouse_event = event->AsMouseEvent();
  switch (mouse_event->type()) {
    case ui::EventType::kMousePressed:
      if (mouse_event->IsOnlyLeftMouseButton()) {
        designer_panel_->RequestFocus();
        DCHECK(!dragging_);
        View* target_view = static_cast<View*>(event->target());
        if (grab_handles_.IsGrabHandle(target_view)) {
          dragging_ = target_view;
          return;
        }
        View* event_view =
            GetDesignerChild(target_view, designer_panel_, designer_views_);
        if (mouse_event->IsAltDown() && event_view != designer_panel_) {
          if (selected_ && (event_view == selected_ ||
                            IsViewParent(selected_, event_view))) {
            event_view = GetDesignerChild(selected_->parent(), designer_panel_,
                                          designer_views_);
          } else {
            event_view = GetDesignerChild(event_view->parent(), designer_panel_,
                                          designer_views_);
          }
        }
        grab_handles_.SetAttachedView(nullptr);
        SelectView(event_view);
        if (selected_ && selected_ != designer_panel_) {
          dragging_ = selected_;
          last_mouse_pos_ = mouse_event->location();
        }
        event->SetHandled();
        return;
      }
      break;
    case ui::EventType::kMouseDragged:
      if (dragging_) {
        if (grab_handles_.IsGrabHandle(dragging_)) {
          return;
        }
        if (dragging_->parent() != designer_panel_) {
          return;
        }
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
        if (model_observer_) {
          model_observer_->OnModelChanged();
        }
        bool dragging_handle = grab_handles_.IsGrabHandle(dragging_);
        dragging_ = nullptr;
        if (!dragging_handle) {
          event->SetHandled();
        }
        return;
      }
      break;
    default:
      return;
  }
}

void DesignerExample::SelectView(View* view) {
  if (view != selected_) {
    if (active_inplace_editor_) {
      active_inplace_editor_->Commit();
    }
    selected_ = view;
    active_editors_.clear();
    if (selected_) {
      for (auto* member : *selected_->GetClassMetaData()) {
        active_editors_.push_back(CreatePropertyEditor(selected_, member));
      }
    }
    RefreshDisplayList();
    if (model_observer_) {
      model_observer_->OnModelChanged();
    }
  }
}

void DesignerExample::RefreshDisplayList() {
  display_list_.clear();
  auto add_to_list = [this](auto& self,
                            DesignerPropertyEditor* editor) -> void {
    display_list_.push_back(editor);
    if (editor->IsExpandable() && editor->IsExpanded()) {
      for (auto* sub : editor->GetSubEditors()) {
        self(self, sub);
      }
    }
  };

  for (const auto& editor : active_editors_) {
    add_to_list(add_to_list, editor.get());
  }
}

void DesignerExample::OnSelectionChanged() {
  std::optional<size_t> row = inspector_->GetFirstSelectedRow();

  if (active_inplace_editor_) {
    // Commit and remove previous editor.
    active_inplace_editor_->Commit();
  }

  if (!row.has_value() || !selected_) {
    return;
  }

  auto editor = display_list_[row.value()];
  std::unique_ptr<InPlaceEditor> inplace_editor;

  size_t current_row = row.value();
  base::OnceClosure on_commit = base::BindOnce(
      [](DesignerExample* example, size_t row_index) {
        if (example->active_inplace_editor_) {
          example->inspector_->RemoveChildViewT(
              example->active_inplace_editor_.get());
          example->active_inplace_editor_ = nullptr;
          if (example->selected_) {
            example->selected_->InvalidateLayout();
            example->selected_->SchedulePaint();
            example->designer_panel_->SchedulePaint();
          }
          if (example->model_observer_) {
            example->model_observer_->OnItemsChanged(row_index, 1);
          }
        }
      },
      base::Unretained(this), current_row);

  if (editor->GetEditorType() ==
      DesignerPropertyEditor::EditorType::kCombobox) {
    inplace_editor =
        std::make_unique<ComboInPlaceEditor>(editor, std::move(on_commit));
  } else if (editor->GetEditorType() ==
             DesignerPropertyEditor::EditorType::kCheckbox) {
    inplace_editor =
        std::make_unique<CheckboxInPlaceEditor>(editor, std::move(on_commit));
  } else {
    inplace_editor =
        std::make_unique<TextInPlaceEditor>(editor, std::move(on_commit));
  }

  size_t view_row = inspector_->ModelToView(row.value());
  const auto& visible_columns = inspector_->visible_columns();
  const auto& vis_col = visible_columns[1];  // Value column
  gfx::Rect cell_bounds(vis_col.x,
                        static_cast<int>(view_row) * inspector_->GetRowHeight(),
                        vis_col.width, inspector_->GetRowHeight());

  active_inplace_editor_ = inspector_->AddChildView(std::move(inplace_editor));
  active_inplace_editor_->SetBoundsRect(cell_bounds);
  active_inplace_editor_->RequestFocus();
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
  designer_views_.insert(new_view.get());

  View* parent = designer_panel_;
  if (selected_) {
    for (const auto& reg : class_registrations_) {
      if (reg->GetViewClassName() ==
          base::ASCIIToUTF16(selected_->GetClassName())) {
        if (reg->IsContainer()) {
          parent = selected_;
        }
        break;
      }
    }
  }

  if (parent == designer_panel_) {
    gfx::Rect child_rect = designer_panel_->GetContentsBounds();
    child_rect.ClampToCenteredSize(new_view->size());
    child_rect.set_origin(gfx::Point() +
                          SnapToGrid(child_rect.OffsetFromOrigin()));
    new_view->SetBoundsRect(child_rect);
  }
  parent->AddChildView(std::move(new_view));
}

size_t DesignerExample::RowCount() {
  return selected_ ? display_list_.size() : 0;
}

std::u16string DesignerExample::GetText(size_t row, int column_id) {
  if (selected_) {
    const auto editor = display_list_[row];
    if (column_id == 0) {
      std::u16string name = editor->GetPropertyName();
      if (editor->IsExpandable()) {
        name = (editor->IsExpanded() ? u"[-] " : u"[+] ") + name;
      }
      return std::u16string(editor->GetLevel() * 2, ' ') + name;
    }
    return editor->GetValueAsString();
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
