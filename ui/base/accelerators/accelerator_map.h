// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MAP_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MAP_H_

#include <map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#endif

namespace ui {

// This is a wrapper around an internal std::map of type
// |std::map<Accelerator, V>| where |V| is the mapped value.
//
// Accelerators in Chrome on all platforms are specified with the |key_code|,
// aka VKEY, however certain keys (eg. brackets, comma, period, plus, minus),
// are in different places based on the keyboard layout. In some cases the
// VKEYs don't exist, in some cases they now conflict with other shortcuts.
//
// Chrome OS uses a positional mapping for this subset of keys. Shortcuts
// based on these keys are determined by the position of the key on a US
// keyboard. This was already the case for all non-latin alphabet keyboards
// (Chinese, Japanese, Arabic, Russian, etc.).
//
// To achieve this on Chrome OS for the remaining layouts, an additional
// remapping may happen to the accelerator used for lookup based on the state
// of |use_positional_lookup_|. When false no remapping occurs. When true,
// the |code| aka DomCode (which is by definition fixed position), is used to
// find the US layout VKEY, and that VKEY is used to lookup in the map.
//
// Other non-positional keys, eg. alphanumeric, F-keys, and special keys are
// all not remapped. Alphanumeric keys always continue to follow the
// |code|/VKEY defined by the current layout as is existing behavior.
template <typename V>
class COMPONENT_EXPORT(UI_BASE) AcceleratorMap {
 public:
  AcceleratorMap() = default;
  ~AcceleratorMap() = default;

  using iterator = typename std::map<Accelerator, V>::iterator;
  using const_iterator = typename std::map<Accelerator, V>::const_iterator;

  // Lookup an accelerator and return a pointer to the value. If the
  // accelerator is not in the map, this returns nullptr.
  const V* Find(const Accelerator& accelerator) const {
    auto iter = FindImpl(accelerator);
    return iter == map_.end() ? nullptr : &iter->second;
  }

  V* Find(const Accelerator& accelerator) {
    // Call the const implementation to avoid duplicating code.
    return const_cast<V*>(
        const_cast<const AcceleratorMap*>(this)->Find(accelerator));
  }

  // Lookup an accelerator and return a reference to the value. If the
  // accelerator is not present this function will DCHECK.
  const V& Get(const Accelerator& accelerator) const {
    auto iter = FindImpl(accelerator);
    DCHECK(iter != map_.end());
    return iter->second;
  }

  V& Get(const Accelerator& accelerator) {
    // Call the const implementation to avoid duplicating code.
    return const_cast<V&>(
        const_cast<const AcceleratorMap*>(this)->Get(accelerator));
  }

  V& GetOrInsertDefault(const Accelerator& accelerator) {
#if BUILDFLAG(IS_CHROMEOS)
    // Ensure the DomCode is NONE before registering. The DomCode is only
    // used during lookup to select the correct VKEY.
    if (accelerator.code() != DomCode::NONE) {
      Accelerator accelerator_copy = accelerator;
      accelerator_copy.reset_code();
      return map_[accelerator_copy];
    }
#endif
    return map_[accelerator];
  }

  // Erase an accelerator from the map.
  bool Erase(const Accelerator& accelerator) {
    return map_.erase(accelerator) > 0;
  }

  void Clear() { map_.clear(); }

  // Inserts a new accelerator and value into the map. DCHECKs if the
  // accelerator was already in the map.
  void InsertNew(const std::pair<const Accelerator, V>& value) {
#if BUILDFLAG(IS_CHROMEOS)
    // Ensure the DomCode is NONE before registering. The DomCode is only
    // used during lookup to select the correct VKEY.
    if (value.first.code() != DomCode::NONE) {
      Accelerator accelerator_copy = value.first;
      accelerator_copy.reset_code();
      auto value_copy = std::make_pair(accelerator_copy, value.second);
      auto result = map_.insert(value_copy);
      DCHECK(result.second);
      return;
    }
#endif
    auto result = map_.insert(value);
    DCHECK(result.second);
  }

  // Iterators for the internal map.
  iterator begin() { return map_.begin(); }
  iterator end() { return map_.end(); }

  // Returns the number of items in the map.
  size_t size() const { return map_.size(); }

  // Returns true if the map is empty.
  bool empty() const { return map_.empty(); }

#if BUILDFLAG(IS_CHROMEOS)
  // When true, lookup operators on the map will remap the |key_code| of
  // position-based keys based on the |code|.
  void set_use_positional_lookup(bool use_positional_lookup) {
    use_positional_lookup_ = use_positional_lookup;
  }
#endif

 private:
  std::map<Accelerator, V> map_;

#if BUILDFLAG(IS_CHROMEOS)
  bool use_positional_lookup_ = false;

  // For the shortcuts that use positional mapping, the lookup is done based
  // on the |key_code| in the US layout. The supplied |code| (DomCode) is used
  // to remap the layout specific |key_code| with the US layout equivalent,
  // which effectively pins the location of these keys in place regardless of
  // the actual key mapping. Note that this only happens for a subset of keys
  // and has no overlap with alphanumeric characters or the language equivalent
  // keys that map to the alphanumeric set of VKEYS.
  //
  // One such example is Alt ']'. In the US layout ']' is VKEY_OEM_6, in the
  // DE layout it is VKEY_OEM_PLUS. However the key in that position is always
  // DomCode::BRACKET_RIGHT regardless of what the key generates when pressed.
  // This function uses the DomCode to remap the VKEY back to it's US layout
  // equivalent.
  //
  // See the design doc in crbug.com/1174326 for more information.
  Accelerator RemapAcceleratorForLookup(const Accelerator& accelerator) const {
    KeyboardCode lookup_key_code = accelerator.key_code();
    if (use_positional_lookup_) {
      // If there's a valid remapping, use that |KeyboardCode| instead.
      KeyboardCode remapped_key_code =
          KeycodeConverter::MapPositionalDomCodeToUSShortcutKey(
              accelerator.code(), accelerator.key_code());
      lookup_key_code = remapped_key_code != VKEY_UNKNOWN ? remapped_key_code
                                                          : lookup_key_code;
    }

    return Accelerator(lookup_key_code, DomCode::NONE, accelerator.modifiers(),
                       accelerator.key_state());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  const_iterator FindImpl(const Accelerator& accelerator) const {
#if BUILDFLAG(IS_CHROMEOS)
    auto iter = map_.find(RemapAcceleratorForLookup(accelerator));
    // Sanity check that a DomCode was never inserted into the map.
    DCHECK(iter == map_.end() || iter->first.code() == DomCode::NONE);
    return iter;
#endif  // BUILDFLAG(IS_CHROMEOS)

    return map_.find(accelerator);
  }

  iterator FindImpl(const Accelerator& accelerator) {
    return const_cast<V*>(
        const_cast<const AcceleratorMap*>(this)->FindImpl(accelerator));
  }
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MAP_H_
