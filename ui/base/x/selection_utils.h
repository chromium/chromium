// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_UTILS_H_
#define UI_BASE_X_SELECTION_UTILS_H_

#include <stddef.h>
#include <map>

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/x/xproto.h"

namespace ui {
class SelectionData;

// Returns a list of all text atoms that we handle.
COMPONENT_EXPORT(UI_BASE_X) std::vector<x11::Atom> GetTextAtomsFrom();

COMPONENT_EXPORT(UI_BASE_X) std::vector<x11::Atom> GetURLAtomsFrom();

COMPONENT_EXPORT(UI_BASE_X) std::vector<x11::Atom> GetURIListAtomsFrom();

// Places the intersection of |desired| and |offered| into |output|.
COMPONENT_EXPORT(UI_BASE_X)
void GetAtomIntersection(const std::vector<x11::Atom>& desired,
                         const std::vector<x11::Atom>& offered,
                         std::vector<x11::Atom>* output);

// Takes the raw bytes of the std::u16string and copies them into |bytes|.
COMPONENT_EXPORT(UI_BASE_X)
void AddString16ToVector(const std::u16string& str,
                         std::vector<unsigned char>* bytes);

// Tokenizes and parses the Selection Data as if it is a URI List.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<std::string> ParseURIList(const SelectionData& data);

COMPONENT_EXPORT(UI_BASE_X)
std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory);

COMPONENT_EXPORT(UI_BASE_X)
std::u16string RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory);

///////////////////////////////////////////////////////////////////////////////

// Represents the selection in different data formats. Binary data passed in is
// assumed to be allocated with new char[], and is owned by SelectionFormatMap.
class COMPONENT_EXPORT(UI_BASE_X) SelectionFormatMap {
 public:
  // Our internal data store, which we only expose through iterators.
  using InternalMap =
      std::map<x11::Atom, scoped_refptr<base::RefCountedMemory>>;
  using const_iterator = InternalMap::const_iterator;

  SelectionFormatMap();
  SelectionFormatMap(const SelectionFormatMap& other);
  ~SelectionFormatMap();
  // Copy and assignment deliberately open.

  // Adds the selection in the format |atom|. Ownership of |data| is passed to
  // us.
  void Insert(x11::Atom atom,
              const scoped_refptr<base::RefCountedMemory>& item);

  // Returns the first of the |requested_types| or NULL if missing.
  ui::SelectionData GetFirstOf(
      const std::vector<x11::Atom>& requested_types) const;

  // Returns the |SelectionData| of the |requested_type| or NULL if missing.
  ui::SelectionData Get(x11::Atom requested_type) const;

  // Returns all the selected types.
  std::vector<x11::Atom> GetTypes() const;

  // Pass through to STL map. Only allow non-mutation access.
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }
  const_iterator find(x11::Atom atom) const { return data_.find(atom); }
  size_t size() const { return data_.size(); }

 private:
  InternalMap data_;
};

///////////////////////////////////////////////////////////////////////////////

// A holder for data with optional X11 deletion semantics.
class COMPONENT_EXPORT(UI_BASE_X) SelectionData {
 public:
  // |atom_cache| is still owned by caller.
  SelectionData();
  SelectionData(x11::Atom type,
                const scoped_refptr<base::RefCountedMemory>& memory);
  SelectionData(const SelectionData& rhs);
  ~SelectionData();
  SelectionData& operator=(const SelectionData& rhs);

  bool IsValid() const;
  x11::Atom GetType() const;
  const unsigned char* GetData() const;
  size_t GetSize() const;

  // If |type_| is a string type, convert the data to UTF8 and return it.
  std::string GetText() const;

  // If |type_| is the HTML type, returns the data as a string16. This detects
  // guesses the character encoding of the source.
  std::u16string GetHtml() const;

  // Assigns the raw data to the string.
  void AssignTo(std::string* result) const;
  void AssignTo(std::u16string* result) const;

  // Transfers ownership of |memory_| to the caller.
  scoped_refptr<base::RefCountedBytes> TakeBytes();

 private:
  x11::Atom type_;
  scoped_refptr<base::RefCountedMemory> memory_;
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_UTILS_H_
