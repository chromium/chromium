// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_content_browser_overlay_manifest.h"
#include "content/public/app/content_browser_manifest.h"
#include "services/service_manager/public/cpp/manifest.h"

base::ListValue GetInterfacesForCapability(
    const service_manager::Manifest::ExposedCapabilityMap& exposed_capabilities,
    const std::string& capability) {
  auto it = exposed_capabilities.find(capability);
  if (it == exposed_capabilities.end()) {
    return base::ListValue();
  }

  base::ListValue interface_names;
  for (const auto& interface_name : it->second) {
    interface_names.Append(base::Value(interface_name));
  }

  return interface_names;
}

void PrintAccessibleInterfaces(const service_manager::Manifest& manifest,
                               const std::string& capability) {
  base::DictionaryValue interfaces;
  base::ListValue exposed_capabilities =
      GetInterfacesForCapability(manifest.exposed_capabilities, capability);
  interfaces.SetKey("exposed_capabilities", std::move(exposed_capabilities));

  for (const auto& entry : manifest.exposed_interface_filter_capabilities) {
    base::ListValue interface_names =
        GetInterfacesForCapability(entry.second, capability);
    interfaces.SetPath({"exposed_interface_filter_capabilities", entry.first},
                       std::move(interface_names));
  }

  std::string output;
  base::JSONWriter::WriteWithOptions(
      interfaces, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output);
  std::cout << output;
}

int main(int argc, const char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  service_manager::Manifest manifest = content::GetContentBrowserManifest();
  manifest.Amend(GetChromeContentBrowserOverlayManifest());

  auto args = base::CommandLine::ForCurrentProcess()->GetArgs();
  for (const base::CommandLine::StringType& arg : args) {
#if defined(OS_WIN)
    PrintAccessibleInterfaces(manifest, base::UTF16ToASCII(arg));
#else
    PrintAccessibleInterfaces(manifest, arg);
#endif
  }

  return 0;
}
