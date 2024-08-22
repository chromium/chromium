// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TABLE_MODEL_H_
#define UI_BASE_MODELS_TABLE_MODEL_H_

#include <string>

#include "base/component_export.h"
#include "third_party/icu/source/common/unicode/uversion.h"

// third_party/icu/source/common/unicode/uversion.h will set namespace icu.
namespace U_ICU_NAMESPACE {
class Collator;
}

namespace ui {

class ImageModel;
class TableModelObserver;

// The model driving the TableView.
class COMPONENT_EXPORT(UI_BASE) TableModel {
 public:
  // Size of the table row icon, if used.
  static constexpr int kIconSize = 16;

  // Number of rows in the model.
  virtual size_t RowCount() = 0;

  // Returns the value at a particular location in text.
  virtual std::u16string GetText(size_t row, int column_id) = 0;

  // Returns the small icon (|kIconSize| x |kIconSize|) that should be displayed
  // in the first column before the text. This is only used when the TableView
  // was created with the ICON_AND_TEXT table type. An empty ImageModel if there
  // is no image.
  virtual ui::ImageModel GetIcon(size_t row);

  // Returns the tooltip, if any, to show for a particular row.  If there are
  // multiple columns in the row, this will only be shown when hovering over
  // column zero.
  virtual std::u16string GetTooltip(size_t row);

  // Sets the observer for the model. The TableView should NOT take ownership
  // of the observer.
  virtual void SetObserver(TableModelObserver* observer) = 0;

  // Compares the values in the column with id |column_id| for the two rows.
  // Returns a value < 0, == 0 or > 0 as to whether the first value is
  // <, == or > the second value.
  //
  // This implementation does a case insensitive locale specific string
  // comparison.
  virtual int CompareValues(size_t row1, size_t row2, int column_id);

  // Reset the collator.
  void ClearCollator();

 protected:
  virtual ~TableModel();

  // Returns the collator used by CompareValues.
  icu::Collator* GetCollator();
};

// TableColumn specifies the title, alignment and size of a particular column.
struct COMPONENT_EXPORT(UI_BASE) TableColumn {
  enum Alignment : uint8_t { LEFT, RIGHT, CENTER };

  TableColumn();
  TableColumn(int id, Alignment alignment, int width, float percent);
  TableColumn(const TableColumn& other);
  TableColumn& operator=(const TableColumn& other);

  // Note: Please be mindful of ordering when adding, modifying, or removing
  //       fields. The struct should be as tightly packed together as possible.

  // The title for the column.
  std::u16string title;

  // A unique identifier for the column.
  int id;

  // The size of a column may be specified in two ways:
  // 1. A fixed width. Set the width field to a positive number and the
  //    column will be given that width, in pixels.
  // 2. As a percentage of the available width. If width is -1, and percent is
  //    > 0, the column is given a width of
  //    available_width * percent / total_percent.
  // 3. If the width == -1 and percent == 0, the column is autosized based on
  //    the width of the column header text.
  //
  // Sizing is done in four passes. Fixed width columns are given
  // their width, percentages are applied, autosized columns are autosized,
  // and finally percentages are applied again taking into account the widths
  // of autosized columns.
  int width;
  float percent;

  // The minimum width required for all items in this column
  // (including the header) to be visible.
  int min_visible_width;

  // Alignment for the content.
  Alignment alignment;

  // Is this column sortable? Default is false.
  bool sortable;

  // Determines what sort order to apply initially. Default is true.
  bool initial_sort_is_ascending;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TABLE_MODEL_H_
