// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/table_model.h"

#include "base/check.h"
#include "base/i18n/string_compare.h"
#include "base/notreached.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace ui {

// TableColumn -----------------------------------------------------------------

TableColumn::TableColumn()
    : id(0),
      width(-1),
      percent(),
      min_visible_width(0),
      alignment(LEFT),
      sortable(false),
      initial_sort_is_ascending(true),
      elide_behavior(gfx::NO_ELIDE) {}

TableColumn::TableColumn(int id, Alignment alignment, int width, float percent)
    : title(l10n_util::GetStringUTF16(id)),
      id(id),
      width(width),
      percent(percent),
      min_visible_width(0),
      alignment(alignment),
      sortable(false),
      initial_sort_is_ascending(true),
      elide_behavior(gfx::NO_ELIDE) {}

TableColumn::TableColumn(const TableColumn& other) = default;

TableColumn& TableColumn::operator=(const TableColumn& other) = default;

// TableModel -----------------------------------------------------------------

// Used for sorting.
namespace {
icu::Collator* g_collator = nullptr;
}  // namespace

ui::ImageModel TableModel::GetIcon(size_t row) const {
  return ui::ImageModel();
}

std::u16string TableModel::GetTooltip(size_t row) const {
  return std::u16string();
}

std::u16string TableModel::GetAXNameForHeader(
    const std::vector<std::u16string>& visible_column_titles,
    const std::vector<std::u16string>& visible_column_sortable) const {
  return std::u16string();
}

std::u16string TableModel::GetAXNameForHeaderCell(
    const std::u16string& visible_column_title,
    const std::u16string& visible_column_sortable) const {
  return visible_column_title;
}

std::u16string TableModel::GetAXNameForRow(
    size_t row,
    const std::vector<int>& visible_column_ids) const {
  DCHECK_LT(row, RowCount());
  DCHECK(!visible_column_ids.empty());
  return GetText(row, visible_column_ids[0]);
}

int TableModel::CompareValues(size_t row1, size_t row2, int column_id) const {
  DCHECK_LT(row1, RowCount());
  DCHECK_LT(row2, RowCount());
  DCHECK(g_collator);

  std::u16string value1 = GetText(row1, column_id);
  std::u16string value2 = GetText(row2, column_id);
  return base::i18n::CompareString16WithCollator(*g_collator, value1, value2);
}

TableModel::~TableModel() = default;

// static
void TableModel::CreateCollator(base::PassKey<views::TableView>) {
  CHECK(!g_collator);
  UErrorCode create_status = U_ZERO_ERROR;
  g_collator = icu::Collator::createInstance(create_status);
  CHECK(U_SUCCESS(create_status));
}

// static
void TableModel::ClearCollator(base::PassKey<views::TableView>) {
  CHECK(g_collator);
  delete g_collator;
  g_collator = nullptr;
}

}  // namespace ui
