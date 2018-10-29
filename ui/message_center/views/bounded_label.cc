// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_label.h"

#include <stddef.h>

#include <limits>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/label.h"

namespace {

const size_t kPreferredLinesCacheSize = 10;

}  // namespace

namespace message_center {

// InnerBoundedLabel ///////////////////////////////////////////////////////////

// InnerBoundedLabel is a views::Label subclass that does all of the work for
// BoundedLabel. It is kept private to prevent outside code from calling a
// number of views::Label methods like SetFontList() that break BoundedLabel's
// caching but can't be overridden.
//
// TODO(dharcourt): Move the line limiting functionality to views::Label to make
// this unnecessary.

class InnerBoundedLabel : public views::Label {
 public:
  InnerBoundedLabel(const BoundedLabel& owner);
  ~InnerBoundedLabel() override;

  void SetNativeTheme(const ui::NativeTheme* theme);

  // Pass in a -1 width to use the preferred width, a -1 limit to skip limits.
  int GetLinesForWidthAndLimit(int width, int limit);
  gfx::Size GetSizeForWidthAndLines(int width, int lines);
  std::vector<base::string16> GetWrappedText(int width, int lines);

  // Overridden from views::Label.
  void SetText(const base::string16& new_text) override;
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  // Overridden from views::Label.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  int GetTextFlags();

  void ClearCaches();
  int GetCachedLines(int width);
  void SetCachedLines(int width, int lines);
  gfx::Size GetCachedSize(const std::pair<int, int>& width_and_lines);
  void SetCachedSize(std::pair<int, int> width_and_lines, gfx::Size size);

  const BoundedLabel* owner_;  // Weak reference.
  base::string16 wrapped_text_;
  int wrapped_text_width_;
  int wrapped_text_lines_;
  std::map<int, int> lines_cache_;
  std::list<int> lines_widths_;  // Most recently used in front.
  std::map<std::pair<int, int>, gfx::Size> size_cache_;
  std::list<std::pair<int, int> > size_widths_and_lines_;  // Recent in front.

  DISALLOW_COPY_AND_ASSIGN(InnerBoundedLabel);
};

InnerBoundedLabel::InnerBoundedLabel(const BoundedLabel& owner)
    : owner_(&owner),
      wrapped_text_width_(0),
      wrapped_text_lines_(0) {
  SetMultiLine(true);
  SetAllowCharacterBreak(true);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  set_collapse_when_hidden(true);
}

InnerBoundedLabel::~InnerBoundedLabel() {
}

void InnerBoundedLabel::SetNativeTheme(const ui::NativeTheme* theme) {
  ClearCaches();
  OnNativeThemeChanged(theme);
}

int InnerBoundedLabel::GetLinesForWidthAndLimit(int width, int limit) {
  if (width == 0 || limit == 0)
    return 0;
  int lines = GetCachedLines(width);
  if (lines == std::numeric_limits<int>::max()) {
    int text_width = std::max(width - owner_->GetInsets().width(), 0);
    lines = GetWrappedText(text_width, lines).size();
    SetCachedLines(width, lines);
  }
  return (limit < 0 || lines <= limit) ? lines : limit;
}

gfx::Size InnerBoundedLabel::GetSizeForWidthAndLines(int width, int lines) {
  if (width == 0 || lines == 0)
    return gfx::Size();
  std::pair<int, int> key(width, lines);
  gfx::Size size = GetCachedSize(key);
  if (size.height() == std::numeric_limits<int>::max()) {
    gfx::Insets insets = owner_->GetInsets();
    int text_width = (width < 0) ? std::numeric_limits<int>::max() :
                                   std::max(width - insets.width(), 0);
    int text_height = std::numeric_limits<int>::max();
    std::vector<base::string16> wrapped = GetWrappedText(text_width, lines);
    gfx::Canvas::SizeStringInt(
        base::JoinString(wrapped, base::ASCIIToUTF16("\n")),
        font_list(), &text_width, &text_height, owner_->GetLineHeight(),
        GetTextFlags());
    size.set_width(text_width + insets.width());
    size.set_height(text_height + insets.height());
    SetCachedSize(key, size);
  }
  return size;
}

std::vector<base::string16> InnerBoundedLabel::GetWrappedText(int width,
                                                              int lines) {
  // Short circuit simple case.
  if (width == 0 || lines == 0)
    return std::vector<base::string16>();

  // Restrict line limit to ensure (lines + 1) * line_height <= INT_MAX and
  // use it to calculate a reasonable text height.
  int height = std::numeric_limits<int>::max();
  if (lines > 0) {
    int line_height = std::max(font_list().GetHeight(),
                               2);  // At least 2 pixels.
    int max_lines = std::numeric_limits<int>::max() / line_height - 1;
    lines = std::min(lines, max_lines);
    height = (lines + 1) * line_height;
  }

  // Wrap, using INT_MAX for -1 widths that indicate no wrapping.
  std::vector<base::string16> wrapped;
  gfx::ElideRectangleText(text(), font_list(),
                          (width < 0) ? std::numeric_limits<int>::max() : width,
                          height, gfx::WRAP_LONG_WORDS, &wrapped);

  // Elide if necessary.
  if (lines > 0 && wrapped.size() > static_cast<unsigned int>(lines)) {
    // Add an ellipsis to the last line. If this ellipsis makes the last line
    // too wide, that line will be further elided by the gfx::ElideText below,
    // so for example "ABC" could become "ABC..." and then "AB...".
    base::string16 last =
        wrapped[lines - 1] + base::UTF8ToUTF16(gfx::kEllipsis);
    if (width > 0 && gfx::GetStringWidth(last, font_list()) > width)
      last = gfx::ElideText(last, font_list(), width, gfx::ELIDE_TAIL);
    wrapped.resize(lines - 1);
    wrapped.push_back(last);
  }

  return wrapped;
}

void InnerBoundedLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ClearCaches();
  views::Label::OnBoundsChanged(previous_bounds);
}

void InnerBoundedLabel::OnPaint(gfx::Canvas* canvas) {
  views::Label::OnPaintBackground(canvas);
  views::Label::OnPaintBorder(canvas);
  int lines = owner_->GetLineLimit();
  int height = GetSizeForWidthAndLines(width(), lines).height();
  if (height > 0) {
    gfx::Rect bounds(width(), height);
    bounds.Inset(owner_->GetInsets());
    if (bounds.width() != wrapped_text_width_ || lines != wrapped_text_lines_) {
      wrapped_text_ = base::JoinString(GetWrappedText(bounds.width(), lines),
                                       base::ASCIIToUTF16("\n"));
      wrapped_text_width_ = bounds.width();
      wrapped_text_lines_ = lines;
    }
    bounds.set_x(GetMirroredXForRect(bounds));
    canvas->DrawStringRectWithFlags(
        wrapped_text_, font_list(), enabled_color(), bounds, GetTextFlags());
  }
}

void InnerBoundedLabel::SetText(const base::string16& new_text) {
  if (text() == new_text)
    return;
  views::Label::SetText(new_text);
  ClearCaches();
}

int InnerBoundedLabel::GetTextFlags() {
  int flags = gfx::Canvas::MULTI_LINE | gfx::Canvas::CHARACTER_BREAKABLE;

  // We can't use subpixel rendering if the background is non-opaque.
  if (SkColorGetA(background_color()) != 0xFF)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;

  return flags | gfx::Canvas::TEXT_ALIGN_TO_HEAD;
}

void InnerBoundedLabel::ClearCaches() {
  wrapped_text_width_ = 0;
  wrapped_text_lines_ = 0;
  lines_cache_.clear();
  lines_widths_.clear();
  size_cache_.clear();
  size_widths_and_lines_.clear();
}

int InnerBoundedLabel::GetCachedLines(int width) {
  int lines = std::numeric_limits<int>::max();
  std::map<int, int>::const_iterator found;
  if ((found = lines_cache_.find(width)) != lines_cache_.end()) {
    lines = found->second;
    lines_widths_.remove(width);
    lines_widths_.push_front(width);
  }
  return lines;
}

void InnerBoundedLabel::SetCachedLines(int width, int lines) {
  if (lines_cache_.size() >= kPreferredLinesCacheSize) {
    lines_cache_.erase(lines_widths_.back());
    lines_widths_.pop_back();
  }
  lines_cache_[width] = lines;
  lines_widths_.push_front(width);
}

gfx::Size InnerBoundedLabel::GetCachedSize(
    const std::pair<int, int>& width_and_lines) {
  gfx::Size size(width_and_lines.first, std::numeric_limits<int>::max());
  std::map<std::pair<int, int>, gfx::Size>::const_iterator found;
  if ((found = size_cache_.find(width_and_lines)) != size_cache_.end()) {
    size = found->second;
    size_widths_and_lines_.remove(width_and_lines);
    size_widths_and_lines_.push_front(width_and_lines);
  }
  return size;
}

void InnerBoundedLabel::SetCachedSize(std::pair<int, int> width_and_lines,
                                      gfx::Size size) {
  if (size_cache_.size() >= kPreferredLinesCacheSize) {
    size_cache_.erase(size_widths_and_lines_.back());
    size_widths_and_lines_.pop_back();
  }
  size_cache_[width_and_lines] = size;
  size_widths_and_lines_.push_front(width_and_lines);
}

// BoundedLabel ///////////////////////////////////////////////////////////

BoundedLabel::BoundedLabel(const base::string16& text,
                           const gfx::FontList& font_list)
    : line_limit_(-1), fixed_width_(0) {
  label_.reset(new InnerBoundedLabel(*this));
  label_->SetFontList(font_list);
  label_->SetText(text);
}

BoundedLabel::BoundedLabel(const base::string16& text)
    : line_limit_(-1), fixed_width_(0) {
  label_.reset(new InnerBoundedLabel(*this));
  label_->SetText(text);
}

BoundedLabel::~BoundedLabel() {
}

void BoundedLabel::SetColor(SkColor text_color) {
  label_->SetEnabledColor(text_color);
  label_->SetAutoColorReadabilityEnabled(false);
}

void BoundedLabel::SetLineHeight(int height) {
  label_->SetLineHeight(height);
}

void BoundedLabel::SetLineLimit(int lines) {
  line_limit_ = std::max(lines, -1);
}

void BoundedLabel::SetText(const base::string16& text) {
  label_->SetText(text);
}

int BoundedLabel::GetLineHeight() const {
  return label_->line_height();
}

int BoundedLabel::GetLineLimit() const {
  return line_limit_;
}

int BoundedLabel::GetLinesForWidthAndLimit(int width, int limit) {
  return visible() ? label_->GetLinesForWidthAndLimit(width, limit) : 0;
}

gfx::Size BoundedLabel::GetSizeForWidthAndLines(int width, int lines) {
  return visible() ?
         label_->GetSizeForWidthAndLines(width, lines) : gfx::Size();
}

void BoundedLabel::SizeToFit(int fixed_width) {
  fixed_width_ = fixed_width;
  label_->SizeToFit(fixed_width);
}

int BoundedLabel::GetBaseline() const {
  return label_->GetBaseline();
}

gfx::Size BoundedLabel::CalculatePreferredSize() const {
  if (!visible())
    return gfx::Size();
  return fixed_width_ != 0
             ? label_->GetSizeForWidthAndLines(fixed_width_, line_limit_)
             : label_->GetSizeForWidthAndLines(-1, -1);
}

int BoundedLabel::GetHeightForWidth(int width) const {
  return visible() ?
         label_->GetSizeForWidthAndLines(width, line_limit_).height() : 0;
}

void BoundedLabel::OnPaint(gfx::Canvas* canvas) {
  label_->OnPaint(canvas);
}

bool BoundedLabel::CanProcessEventsWithinSubtree() const {
  return label_->CanProcessEventsWithinSubtree();
}

void BoundedLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  label_->GetAccessibleNodeData(node_data);
}

views::View* BoundedLabel::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (GetSizeForWidthAndLines(width(), -1).height() <=
      GetHeightForWidth(width())) {
    return nullptr;
  }
  return HitTestPoint(point) ? this : nullptr;
}

bool BoundedLabel::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  return label_->GetTooltipText(p, tooltip);
}

void BoundedLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  label_->SetBoundsRect(bounds());
  views::View::OnBoundsChanged(previous_bounds);
}

void BoundedLabel::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  label_->SetNativeTheme(theme);
}

base::string16 BoundedLabel::GetWrappedTextForTest(int width, int lines) {
  return base::JoinString(label_->GetWrappedText(width, lines),
                          base::ASCIIToUTF16("\n"));
}

}  // namespace message_center
