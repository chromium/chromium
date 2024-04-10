// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_COMPONENT_EXTENSION_IME_MANAGER_H_
#define UI_BASE_IME_ASH_COMPONENT_EXTENSION_IME_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "ui/base/ime/ash/component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

class Profile;

namespace ash {

// Represents an engine in component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_ASH) ComponentExtensionEngine {
  ComponentExtensionEngine();
  ComponentExtensionEngine(const ComponentExtensionEngine& other);
  ~ComponentExtensionEngine();
  std::string engine_id;
  std::string display_name;
  std::string indicator;
  std::vector<std::string> language_codes;  // e.g. "en".
  std::string layout;
  std::optional<std::string> handwriting_language;
  GURL options_page_url;
  GURL input_view_url;
};

// Represents a component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_ASH) ComponentExtensionIME {
  ComponentExtensionIME();
  ComponentExtensionIME(const ComponentExtensionIME& other);
  ~ComponentExtensionIME();
  std::string id;           // extension id.
  std::string manifest;     // the contents of manifest.json
  std::string description;  // description of extension.
  GURL options_page_url;
  base::FilePath path;
  std::vector<ComponentExtensionEngine> engines;
};

// This class manages component extension input method.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) ComponentExtensionIMEManager {
 public:
  explicit ComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate);

  ComponentExtensionIMEManager(const ComponentExtensionIMEManager&) = delete;
  ComponentExtensionIMEManager& operator=(const ComponentExtensionIMEManager&) =
      delete;

  virtual ~ComponentExtensionIMEManager();

  // Loads the IME component extension for |input_method_id| if the extension Id
  // is not in the |extension_loaded|. This function returns true once an
  // corresponding IME extension will be loaded. This function is safe to call
  // multiple times. Returns false if the corresponding component extension is
  // already loaded or there is not any IME extension found for the
  // |input_method_id|.
  bool LoadComponentExtensionIME(
      Profile* profile,
      const std::string& input_method_id,
      std::set<std::string>* extension_loaded = nullptr);

  // Returns true if |input_method_id| is allowlisted component extension input
  // method.
  bool IsAllowlisted(const std::string& input_method_id);

  // Returns true if |extension_id| is allowlisted component extension.
  bool IsAllowlistedExtension(const std::string& extension_id);

  // Returns all IME as InputMethodDescriptors.
  input_method::InputMethodDescriptors GetAllIMEAsInputMethodDescriptor();

  // Returns all XKB keyboard IME as InputMethodDescriptors.
  virtual input_method::InputMethodDescriptors
  GetXkbIMEAsInputMethodDescriptor();

 private:
  // Finds ComponentExtensionIME and EngineDescription associated with
  // |input_method_id|. This function returns true if it is found, otherwise
  // returns false. |out_extension| and |out_engine| can be nullptr.
  bool FindEngineEntry(const std::string& input_method_id,
                       ComponentExtensionIME* out_extension);

  bool IsInLoginLayoutAllowlist(const std::string& layout);

  std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate_;

  // The map of extension_id to ComponentExtensionIME instance.
  // It's filled by ctor and never changed during runtime.
  std::map<std::string, ComponentExtensionIME> component_extension_imes_;

  // For quick check the validity of a given input method id.
  // It's filled by ctor and never changed during runtime.
  std::set<std::string> input_method_id_set_;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_COMPONENT_EXTENSION_IME_MANAGER_H_
