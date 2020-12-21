// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_
#define UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager_delegate.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

class Profile;

namespace chromeos {

// Represents an engine in component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionEngine {
  ComponentExtensionEngine();
  ComponentExtensionEngine(const ComponentExtensionEngine& other);
  ~ComponentExtensionEngine();
  std::string engine_id;
  std::string display_name;
  std::string indicator;
  std::vector<std::string> language_codes;  // e.g. "en".
  std::string layout;
  GURL options_page_url;
  GURL input_view_url;
};

// Represents a component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionIME {
  ComponentExtensionIME();
  ComponentExtensionIME(const ComponentExtensionIME& other);
  ~ComponentExtensionIME();
  std::string id;  // extension id.
  std::string manifest;  // the contents of manifest.json
  std::string description;  // description of extension.
  GURL options_page_url;
  base::FilePath path;
  std::vector<ComponentExtensionEngine> engines;
};

// This class manages component extension input method.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionIMEManager {
 public:
  ComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate);
  virtual ~ComponentExtensionIMEManager();

  // Loads |input_method_id| component extension IME. This function returns true
  // on success. This function is safe to call multiple times. Returns false if
  // already corresponding component extension is loaded.
  bool LoadComponentExtensionIME(Profile* profile,
                                 const std::string& input_method_id);

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
  // |input_method_id|. This function retruns true if it is found, otherwise
  // returns false. |out_extension| and |out_engine| can be NULL.
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

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManager);
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_
