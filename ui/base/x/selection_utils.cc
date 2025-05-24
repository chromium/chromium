// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include <stdint.h>

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/gfx/x/atom_cache.h"

namespace ui {

std::vector<x11::Atom> GetTextAtomsFrom() {
  return {x11::GetAtom(kMimeTypeLinuxUtf8String),
          x11::GetAtom(kMimeTypeLinuxString), x11::GetAtom(kMimeTypeLinuxText),
          x11::GetAtom(kMimeTypePlainText),
          x11::GetAtom(kMimeTypeUtf8PlainText)};
}

std::vector<x11::Atom> GetURLAtomsFrom() {
  return {x11::GetAtom(kMimeTypeUriList), x11::GetAtom(kMimeTypeMozillaUrl)};
}

std::vector<x11::Atom> GetURIListAtomsFrom() {
  return {x11::GetAtom(kMimeTypeUriList)};
}

void GetAtomIntersection(const std::vector<x11::Atom>& desired,
                         const std::vector<x11::Atom>& offered,
                         std::vector<x11::Atom>* output) {
  for (const auto& desired_atom : desired) {
    if (base::Contains(offered, desired_atom))
      output->push_back(desired_atom);
  }
}

void AddString16ToVector(std::u16string_view str,
                         std::vector<unsigned char>* bytes) {
  auto span = base::as_byte_span(str);
  bytes->insert(bytes->end(), span.begin(), span.end());
}

std::vector<std::string> ParseURIList(const SelectionData& data) {
  // uri-lists are newline separated file lists in URL encoding.
  std::string unparsed;
  data.AssignTo(&unparsed);
  return base::SplitString(unparsed, "\n", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  CHECK(memory.get());

  return std::string(base::as_string_view(*memory));
}

std::u16string RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  CHECK(memory.get());

  auto in_bytes = base::span(*memory);
  std::u16string out;
  out.resize(memory->size() / 2u);
  base::as_writable_byte_span(out).copy_from(in_bytes);
  return out;
}

///////////////////////////////////////////////////////////////////////////////

SelectionFormatMap::SelectionFormatMap() = default;

SelectionFormatMap::SelectionFormatMap(const SelectionFormatMap& other) =
    default;

SelectionFormatMap::~SelectionFormatMap() = default;

void SelectionFormatMap::Insert(
    x11::Atom atom,
    const scoped_refptr<base::RefCountedMemory>& item) {
  data_.erase(atom);
  data_.emplace(atom, item);
}

ui::SelectionData SelectionFormatMap::GetFirstOf(
    const std::vector<x11::Atom>& requested_types) const {
  for (const auto& requested_type : requested_types) {
    auto data_it = data_.find(requested_type);
    if (data_it != data_.end()) {
      return SelectionData(data_it->first, data_it->second);
    }
  }

  return SelectionData();
}

ui::SelectionData SelectionFormatMap::Get(x11::Atom requested_type) const {
  auto data_it = data_.find(requested_type);
  if (data_it != data_.end()) {
    return SelectionData(data_it->first, data_it->second);
  }

  return SelectionData();
}

std::vector<x11::Atom> SelectionFormatMap::GetTypes() const {
  std::vector<x11::Atom> atoms;
  for (const auto& datum : data_)
    atoms.push_back(datum.first);

  return atoms;
}

///////////////////////////////////////////////////////////////////////////////

SelectionData::SelectionData() : type_(x11::Atom::None) {}

SelectionData::SelectionData(
    x11::Atom type,
    const scoped_refptr<base::RefCountedMemory>& memory)
    : type_(type), memory_(memory) {}

SelectionData::SelectionData(const SelectionData& rhs) = default;

SelectionData::~SelectionData() = default;

SelectionData& SelectionData::operator=(const SelectionData& rhs) {
  type_ = rhs.type_;
  memory_ = rhs.memory_;
  // TODO(erg): In some future where we have to support multiple X Displays,
  // the following will also need to deal with the display.
  return *this;
}

bool SelectionData::IsValid() const {
  return type_ != x11::Atom::None;
}

x11::Atom SelectionData::GetType() const {
  return type_;
}

base::span<const unsigned char> SelectionData::GetSpan() const {
  return memory_ ? *memory_ : base::span<const unsigned char>();
}

std::string SelectionData::GetText() const {
  if (type_ == x11::GetAtom(kMimeTypeLinuxUtf8String) ||
      type_ == x11::GetAtom(kMimeTypeLinuxText) ||
      type_ == x11::GetAtom(kMimeTypeUtf8PlainText)) {
    return RefCountedMemoryToString(memory_);
  } else {
    // BTW, I looked at COMPOUND_TEXT, and there's no way we're going to
    // support that. Yuck.
    CHECK(type_ == x11::GetAtom(kMimeTypeLinuxString) ||
          type_ == x11::GetAtom(kMimeTypePlainText));
    std::string result;
    base::ConvertToUtf8AndNormalize(RefCountedMemoryToString(memory_),
                                    base::kCodepageLatin1, &result);
    return result;
  }
}

std::u16string SelectionData::GetHtml() const {
  std::u16string markup;

  CHECK_EQ(type_, x11::GetAtom(kMimeTypeHtml));
  base::span<const unsigned char> span = GetSpan();

  // If the data starts with U+FEFF, i.e., Byte Order Mark, assume it is
  // UTF-16, otherwise assume UTF-8.
  UNSAFE_TODO({
    if (span.size() >= 2 &&
        reinterpret_cast<const char16_t*>(span.data())[0] == u'\uFEFF') {
      markup.assign(reinterpret_cast<const char16_t*>(span.data()) + 1,
                    (span.size() / 2) - 1);
    } else {
      markup = base::UTF8ToUTF16(base::as_string_view(span));
    }
  });

  // If there is a terminating NULL, drop it.
  if (!markup.empty() && markup.back() == '\0') {
    markup.pop_back();
  }

  return markup;
}

void SelectionData::AssignTo(std::string* result) const {
  *result = RefCountedMemoryToString(memory_);
}

void SelectionData::AssignTo(std::u16string* result) const {
  *result = RefCountedMemoryToString16(memory_);
}

scoped_refptr<base::RefCountedBytes> SelectionData::TakeBytes() {
  if (!memory_.get()) {
    return nullptr;
  }
  auto* memory = memory_.release();
  return base::MakeRefCounted<base::RefCountedBytes>(*memory);
}

}  // namespace ui
