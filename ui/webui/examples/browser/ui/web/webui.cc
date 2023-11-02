// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/webui.h"

#include "chrome/grit/webui_gallery_resources.h"
#include "chrome/grit/webui_gallery_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/examples/grit/webui_examples_resources.h"

namespace webui_examples {

namespace {

void SetJSModuleDefaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");
  // TODO(crbug.com/1098690): Trusted Type Polymer
  source->DisableTrustedTypesCSP();
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'self';");
}

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  SetJSModuleDefaults(source);
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

}  // namespace

WebUI::WebUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(kHost);
  SetupWebUIDataSource(
      source,
      base::make_span(kWebuiGalleryResources, kWebuiGalleryResourcesSize),
      IDR_WEBUI_GALLERY_WEBUI_GALLERY_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

WebUI::~WebUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebUI)

}  // namespace webui_examples
