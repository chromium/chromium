// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include <stdint.h>

#include <set>

#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

const char kString[] = "STRING";
const char kText[] = "TEXT";
const char kTextPlain[] = "text/plain";
const char kTextPlainUtf8[] = "text/plain;charset=utf-8";
const char kUtf8String[] = "UTF8_STRING";

std::vector<::Atom> GetTextAtomsFrom() {
  std::vector< ::Atom> atoms;
  atoms.push_back(gfx::GetAtom(kUtf8String));
  atoms.push_back(gfx::GetAtom(kString));
  atoms.push_back(gfx::GetAtom(kText));
  atoms.push_back(gfx::GetAtom(kTextPlain));
  atoms.push_back(gfx::GetAtom(kTextPlainUtf8));
  return atoms;
}

std::vector<::Atom> GetURLAtomsFrom() {
  std::vector< ::Atom> atoms;
  atoms.push_back(gfx::GetAtom(kMimeTypeURIList));
  atoms.push_back(gfx::GetAtom(kMimeTypeMozillaURL));
  return atoms;
}

std::vector<::Atom> GetURIListAtomsFrom() {
  std::vector< ::Atom> atoms;
  atoms.push_back(gfx::GetAtom(kMimeTypeURIList));
  return atoms;
}

void GetAtomIntersection(const std::vector< ::Atom>& desired,
                         const std::vector< ::Atom>& offered,
                         std::vector< ::Atom>* output) {
  for (const auto& desired_atom : desired) {
    if (base::Contains(offered, desired_atom))
      output->push_back(desired_atom);
  }
}

void AddString16ToVector(const base::string16& str,
                         std::vector<unsigned char>* bytes) {
  const unsigned char* front =
      reinterpret_cast<const unsigned char*>(str.data());
  bytes->insert(bytes->end(), front, front + (str.size() * 2));
}

std::vector<std::string> ParseURIList(const SelectionData& data) {
  // uri-lists are newline separated file lists in URL encoding.
  std::string unparsed;
  data.AssignTo(&unparsed);
  return base::SplitString(
      unparsed, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return std::string();
  }

  size_t size = memory->size();
  if (!size)
    return std::string();

  const unsigned char* front = memory->front();
  return std::string(reinterpret_cast<const char*>(front), size);
}

base::string16 RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return base::string16();
  }

  size_t size = memory->size();
  if (!size)
    return base::string16();

  const unsigned char* front = memory->front();
  return base::string16(reinterpret_cast<const base::char16*>(front), size / 2);
}

///////////////////////////////////////////////////////////////////////////////

SelectionFormatMap::SelectionFormatMap() {}

SelectionFormatMap::SelectionFormatMap(const SelectionFormatMap& other) =
    default;

SelectionFormatMap::~SelectionFormatMap() {}

void SelectionFormatMap::Insert(
    ::Atom atom,
    const scoped_refptr<base::RefCountedMemory>& item) {
  data_.erase(atom);
  data_.emplace(atom, item);
}

ui::SelectionData SelectionFormatMap::GetFirstOf(
    const std::vector< ::Atom>& requested_types) const {
  for (const auto& requested_type : requested_types) {
    auto data_it = data_.find(requested_type);
    if (data_it != data_.end()) {
      return SelectionData(data_it->first, data_it->second);
    }
  }

  return SelectionData();
}

std::vector< ::Atom> SelectionFormatMap::GetTypes() const {
  std::vector< ::Atom> atoms;
  for (const auto& datum : data_)
    atoms.push_back(datum.first);

  return atoms;
}

///////////////////////////////////////////////////////////////////////////////

SelectionData::SelectionData() : type_(x11::None) {}

SelectionData::SelectionData(
    ::Atom type,
    const scoped_refptr<base::RefCountedMemory>& memory)
    : type_(type), memory_(memory) {}

SelectionData::SelectionData(const SelectionData& rhs)
    : type_(rhs.type_), memory_(rhs.memory_) {}

SelectionData::~SelectionData() {}

SelectionData& SelectionData::operator=(const SelectionData& rhs) {
  type_ = rhs.type_;
  memory_ = rhs.memory_;
  // TODO(erg): In some future where we have to support multiple X Displays,
  // the following will also need to deal with the display.
  return *this;
}

bool SelectionData::IsValid() const {
  return type_ != x11::None;
}

::Atom SelectionData::GetType() const {
  return type_;
}

const unsigned char* SelectionData::GetData() const {
  return memory_.get() ? memory_->front() : nullptr;
}

size_t SelectionData::GetSize() const {
  return memory_.get() ? memory_->size() : 0;
}

std::string SelectionData::GetText() const {
  if (type_ == gfx::GetAtom(kUtf8String) || type_ == gfx::GetAtom(kText) ||
      type_ == gfx::GetAtom(kTextPlainUtf8)) {
    return RefCountedMemoryToString(memory_);
  } else if (type_ == gfx::GetAtom(kString) ||
             type_ == gfx::GetAtom(kTextPlain)) {
    std::string result;
    base::ConvertToUtf8AndNormalize(RefCountedMemoryToString(memory_),
                                    base::kCodepageLatin1,
                                    &result);
    return result;
  } else {
    // BTW, I looked at COMPOUND_TEXT, and there's no way we're going to
    // support that. Yuck.
    NOTREACHED();
    return std::string();
  }
}

base::string16 SelectionData::GetHtml() const {
  base::string16 markup;

  if (type_ == gfx::GetAtom(kMimeTypeHTML)) {
    const unsigned char* data = GetData();
    size_t size = GetSize();

    // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
    // UTF-16, otherwise assume UTF-8.
    if (size >= 2 &&
        reinterpret_cast<const uint16_t*>(data)[0] == 0xFEFF) {
      markup.assign(reinterpret_cast<const uint16_t*>(data) + 1,
                    (size / 2) - 1);
    } else {
      base::UTF8ToUTF16(reinterpret_cast<const char*>(data), size, &markup);
    }

    // If there is a terminating NULL, drop it.
    if (!markup.empty() && markup.at(markup.length() - 1) == '\0')
      markup.resize(markup.length() - 1);

    return markup;
  } else {
    NOTREACHED();
    return markup;
  }
}

void SelectionData::AssignTo(std::string* result) const {
  *result = RefCountedMemoryToString(memory_);
}

void SelectionData::AssignTo(base::string16* result) const {
  *result = RefCountedMemoryToString16(memory_);
}

}  // namespace ui
