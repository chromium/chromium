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
#include "ui/base/ime/chromeos/input_method_descriptor.h"

class Profile;

namespace chromeos {

// Represents an engine in component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionEngine {
  ComponentExtensionEngine();
  ComponentExtensionEngine(const ComponentExtensionEngine& other);
  ~ComponentExtensionEngine();
  std::string engine_id;  // The engine id.
  std::string display_name;  // The display name.
  std::string indicator;  // The indicator text.
  std::vector<std::string> language_codes;  // The engine's language(ex. "en").
  std::string description;  // The engine description.
  std::vector<std::string> layouts;  // The list of keyboard layout of engine.
  GURL options_page_url; // an URL to option page.
  GURL input_view_url; // an URL to input view page.
};

// Represents a component extension IME.
struct COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionIME {
  ComponentExtensionIME();
  ComponentExtensionIME(const ComponentExtensionIME& other);
  ~ComponentExtensionIME();
  std::string id;  // extension id.
  std::string manifest;  // the contents of manifest.json
  std::string description;  // description of extension.
  GURL options_page_url; // an URL to option page.
  base::FilePath path;
  std::vector<ComponentExtensionEngine> engines;
};

// Provides an interface to list/load/unload for component extension IME.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    ComponentExtensionIMEManagerDelegate {
 public:
  ComponentExtensionIMEManagerDelegate();
  virtual ~ComponentExtensionIMEManagerDelegate();

  // Lists installed component extension IMEs.
  virtual std::vector<ComponentExtensionIME> ListIME() = 0;

  // Loads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true.
  virtual void Load(Profile* profile,
                    const std::string& extension_id,
                    const std::string& manifest,
                    const base::FilePath& path) = 0;

  // Unloads component extension IME associated with |extension_id|.
  virtual void Unload(Profile* profile,
                      const std::string& extension_id,
                      const base::FilePath& path) = 0;
};

// This class manages component extension input method.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ComponentExtensionIMEManager {
 public:
  ComponentExtensionIMEManager();
  virtual ~ComponentExtensionIMEManager();

  // Initializes component extension manager. This function create internal
  // mapping between input method id and engine components. This function must
  // be called before using any other function.
  void Initialize(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate);

  // Loads |input_method_id| component extension IME. This function returns true
  // on success. This function is safe to call multiple times. Returns false if
  // already corresponding component extension is loaded.
  bool LoadComponentExtensionIME(Profile* profile,
                                 const std::string& input_method_id);

  // Unloads |input_method_id| component extension IME. This function returns
  // true on success. This function is safe to call multiple times. Returns
  // false if already corresponding component extension is unloaded.
  bool UnloadComponentExtensionIME(Profile* profile,
                                   const std::string& input_method_id);

  // Returns true if |input_method_id| is whitelisted component extension input
  // method.
  bool IsWhitelisted(const std::string& input_method_id);

  // Returns true if |extension_id| is whitelisted component extension.
  bool IsWhitelistedExtension(const std::string& extension_id);

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

  bool IsInLoginLayoutWhitelist(const std::vector<std::string>& layouts);

  std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate_;

  // The map of extension_id to ComponentExtensionIME instance.
  // It's filled by Initialize() method and never changed during runtime.
  std::map<std::string, ComponentExtensionIME> component_extension_imes_;

  // For quick check the validity of a given input method id.
  // It's filled by Initialize() method and never changed during runtime.
  std::set<std::string> input_method_id_set_;

  std::set<std::string> login_layout_set_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManager);
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_H_
