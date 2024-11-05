// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/webapps/manifest_builder.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/color_utils.h"

namespace wolvic {

namespace {

std::string GetDisplayMode(blink::mojom::DisplayMode display) {
  switch (display) {
    case blink::mojom::DisplayMode::kBrowser:
      return "browser";
    case blink::mojom::DisplayMode::kMinimalUi:
      return "minimal-ui";
    case blink::mojom::DisplayMode::kStandalone:
      return "standalone";
    case blink::mojom::DisplayMode::kFullscreen:
      return "fullscreen";
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
      return "window-controls-overlay";
    case blink::mojom::DisplayMode::kTabbed:
      return "tabbed";
    case blink::mojom::DisplayMode::kBorderless:
      return "borderless";
    case blink::mojom::DisplayMode::kPictureInPicture:
      return "picture-in-picture";
    default:
      return "browser";
  }

  NOTREACHED();
  return "browser";
}

std::string GetScreenOrientationLockType(
    device::mojom::ScreenOrientationLockType orientation) {
  switch (orientation) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return "portrait-primary";
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return "portrait-secondary";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return "landscape-primary";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return "landscape-secondary";
    case device::mojom::ScreenOrientationLockType::ANY:
      return "any";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return "landscape";
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return "portrait";
    case device::mojom::ScreenOrientationLockType::NATURAL:
      return "natural";
    default:
      return "any";
  }

  NOTREACHED();
  return "any";
}

}  // namespace

// static
std::string ManifestBuilder::FromMojoToJson(
    const blink::mojom::Manifest& manifest) {
  auto json = base::Value::Dict();
  if (manifest.name.has_value()) {
    json.Set("name", *manifest.name);
  }

  if (manifest.short_name.has_value()) {
    json.Set("short_name", *manifest.short_name);
  }

  if (!manifest.id.is_empty()) {
    json.Set("id", manifest.id.spec());
  }

  if (!manifest.scope.is_empty()) {
    json.Set("scope", manifest.scope.spec());
  }

  if (!manifest.start_url.is_empty()) {
    json.Set("start_url", manifest.start_url.spec());
  }

  json.Set("display", GetDisplayMode(manifest.display));
  json.Set("orientation", GetScreenOrientationLockType(manifest.orientation));

  if (manifest.description.has_value()) {
    json.Set("description", *manifest.description);
  }

  base::Value::List icons;
  for (const auto& icon : manifest.icons) {
    auto value = base::Value::Dict().Set("src", icon.src.spec());
    if (!icon.type.empty()) {
      value.Set("type", icon.type);
    }
    std::string sizes;
    for (const auto& size : icon.sizes) {
      if (!sizes.empty()) {
        sizes.append(" ");
      }

      sizes.append(size.IsEmpty() ? std::string("any")
                                  : base::StringPrintf("%dx%d", size.width(),
                                                       size.height()));
    }
    value.Set("sizes", sizes);
    icons.Append(std::move(value));
  }
  if (!icons.empty()) {
    json.Set("icons", std::move(icons));
  }

  if (manifest.share_target.has_value()) {
    auto share_target =
        base::Value::Dict()
            .Set("action", manifest.share_target->action.spec())
            .Set("method",
                 manifest.share_target->method ==
                         blink::mojom::ManifestShareTarget_Method::kPost
                     ? "POST"
                     : "GET")
            .Set("enctype", manifest.share_target->enctype ==
                                    blink::mojom::ManifestShareTarget::Enctype::
                                        kFormUrlEncoded
                                ? "application/x-www-form-urlencoded"
                                : "multipart/form-data");

    base::Value::Dict param;
    if (manifest.share_target->params.title.has_value()) {
      param.Set("title", *manifest.share_target->params.title);
    }
    if (manifest.share_target->params.text.has_value()) {
      param.Set("text", *manifest.share_target->params.text);
    }
    if (manifest.share_target->params.url.has_value()) {
      param.Set("url", *manifest.share_target->params.url);
    }

    base::Value::List files;
    for (const auto& file : manifest.share_target->params.files) {
      if (!file.name.empty()) {
        continue;
      }
      base::Value::List value;
      for (const auto& accept : file.accept) {
        value.Append(accept);
      }
      if (value.empty()) {
        continue;
      }
      files.Append(base::Value::Dict()
                       .Set("name", file.name)
                       .Set("accept", std::move(value)));
    }
    if (!files.empty()) {
      param.Set("files", std::move(files));
    }

    if (!param.empty()) {
      share_target.Set("param", std::move(param));
    }
    json.Set("share_target", std::move(share_target));
  }

  base::Value::List related_applications;
  for (const auto& app : manifest.related_applications) {
    base::Value::Dict related_application;
    if (app.platform.has_value()) {
      related_application.Set("platform", *app.platform);
    }
    if (app.url.is_empty()) {
      related_application.Set("url", app.url.spec());
    }
    if (app.id.has_value()) {
      related_application.Set("id", *app.id);
    }
    if (!related_application.empty()) {
      related_applications.Append(std::move(related_application));
    }
  }

  if (!related_applications.empty()) {
    json.Set("related_applications", std::move(related_applications));
  }
  json.Set("prefer_related_applications",
           manifest.prefer_related_applications ? "true" : "false");

  if (manifest.has_theme_color) {
    json.Set("theme_color",
             color_utils::SkColorToRgbaString(manifest.theme_color));
  }

  if (manifest.has_background_color) {
    json.Set("background_color",
             color_utils::SkColorToRgbaString(manifest.background_color));
  }

  return base::WriteJsonWithOptions(json, base::OPTIONS_PRETTY_PRINT).value();
}

}  // namespace wolvic
