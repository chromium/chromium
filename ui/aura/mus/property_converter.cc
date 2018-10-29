// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_converter.h"

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace aura {

namespace {

// Get the WindowProperty's value as a byte array. Only supports aura properties
// that point to types with a matching std::vector<uint8_t> mojo::TypeConverter.
template <typename T>
std::unique_ptr<std::vector<uint8_t>> GetArray(Window* window,
                                               const WindowProperty<T>* key) {
  const T value = window->GetProperty(key);
  if (!value)
    return std::make_unique<std::vector<uint8_t>>();
  return std::make_unique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(*value));
}

// A validator that always returns true regardless of its input.
bool AlwaysTrue(int64_t value) {
  return true;
}

bool ValidateResizeBehaviour(int64_t value) {
  // Resize behaviour is a 3 bitfield.
  return value >= 0 && value <= (ws::mojom::kResizeBehaviorCanMaximize |
                                 ws::mojom::kResizeBehaviorCanMinimize |
                                 ws::mojom::kResizeBehaviorCanResize);
}

bool ValidateShowState(int64_t value) {
  return value == int64_t(ws::mojom::ShowState::DEFAULT) ||
         value == int64_t(ws::mojom::ShowState::NORMAL) ||
         value == int64_t(ws::mojom::ShowState::MINIMIZED) ||
         value == int64_t(ws::mojom::ShowState::MAXIMIZED) ||
         value == int64_t(ws::mojom::ShowState::INACTIVE) ||
         value == int64_t(ws::mojom::ShowState::FULLSCREEN);
}

bool ValidateWindowCornerRadius(int64_t value) {
  return value >= -1;
}

}  // namespace

PropertyConverter::PrimitiveProperty::PrimitiveProperty() {}

PropertyConverter::PrimitiveProperty::PrimitiveProperty(
    const PrimitiveProperty& property) = default;

PropertyConverter::PrimitiveProperty::~PrimitiveProperty() {}

// static
base::RepeatingCallback<bool(int64_t)>
PropertyConverter::CreateAcceptAnyValueCallback() {
  return base::Bind(&AlwaysTrue);
}

PropertyConverter::PropertyConverter() {
  // Add known aura properties with associated mus properties.
  RegisterImageSkiaProperty(client::kAppIconKey,
                            ws::mojom::WindowManager::kAppIcon_Property);
  RegisterImageSkiaProperty(client::kWindowIconKey,
                            ws::mojom::WindowManager::kWindowIcon_Property);
  RegisterPrimitiveProperty(client::kAlwaysOnTopKey,
                            ws::mojom::WindowManager::kAlwaysOnTop_Property,
                            CreateAcceptAnyValueCallback());
  RegisterPrimitiveProperty(client::kDrawAttentionKey,
                            ws::mojom::WindowManager::kDrawAttention_Property,
                            CreateAcceptAnyValueCallback());
  RegisterPrimitiveProperty(
      client::kImmersiveFullscreenKey,
      ws::mojom::WindowManager::kImmersiveFullscreen_Property,
      CreateAcceptAnyValueCallback());
  RegisterPrimitiveProperty(client::kResizeBehaviorKey,
                            ws::mojom::WindowManager::kResizeBehavior_Property,
                            base::Bind(&ValidateResizeBehaviour));
  RegisterPrimitiveProperty(client::kShowStateKey,
                            ws::mojom::WindowManager::kShowState_Property,
                            base::Bind(&ValidateShowState));
  RegisterRectProperty(client::kRestoreBoundsKey,
                       ws::mojom::WindowManager::kRestoreBounds_Property);
  RegisterSizeProperty(client::kPreferredSize,
                       ws::mojom::WindowManager::kPreferredSize_Property);
  RegisterSizeProperty(client::kMinimumSize,
                       ws::mojom::WindowManager::kMinimumSize_Property);
  RegisterStringProperty(client::kNameKey,
                         ws::mojom::WindowManager::kName_Property);
  RegisterString16Property(client::kTitleKey,
                           ws::mojom::WindowManager::kWindowTitle_Property);
  RegisterPrimitiveProperty(
      client::kWindowCornerRadiusKey,
      ws::mojom::WindowManager::kWindowCornerRadius_Property,
      base::BindRepeating(&ValidateWindowCornerRadius));
  RegisterPrimitiveProperty(
      client::kAnimationsDisabledKey,
      ws::mojom::WindowManager::kAnimationsDisabled_Property,
      CreateAcceptAnyValueCallback());
  RegisterWindowPtrProperty(
      client::kChildModalParentKey,
      ws::mojom::WindowManager::kChildModalParent_Property);
}

PropertyConverter::~PropertyConverter() {}

const void* PropertyConverter::GetPropertyKeyFromTransportName(
    const std::string& transport_name) {
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.transport_name == transport_name)
      return primitive_property.first;
  }

  for (const auto& image_property : image_properties_) {
    if (image_property.second == transport_name)
      return image_property.first->name;
  }

  for (const auto& rect_property : rect_properties_) {
    if (rect_property.second == transport_name)
      return rect_property.first->name;
  }

  for (const auto& size_property : size_properties_) {
    if (size_property.second == transport_name)
      return size_property.first->name;
  }

  for (const auto& string_property : string_properties_) {
    if (string_property.second == transport_name)
      return string_property.first->name;
  }

  for (const auto& string16_property : string16_properties_) {
    if (string16_property.second == transport_name)
      return string16_property.first->name;
  }

  for (const auto& unguessable_token_property : unguessable_token_properties_) {
    if (unguessable_token_property.second == transport_name)
      return unguessable_token_property.first->name;
  }

  for (const auto& window_ptr_property : window_ptr_properties_) {
    if (window_ptr_property.second == transport_name)
      return window_ptr_property.first->name;
  }

  return nullptr;
}

bool PropertyConverter::IsTransportNameRegistered(
    const std::string& name) const {
  return transport_names_.count(name) > 0;
}

bool PropertyConverter::ConvertPropertyForTransport(
    Window* window,
    const void* key,
    std::string* transport_name,
    std::unique_ptr<std::vector<uint8_t>>* transport_value) {
  *transport_name = GetTransportNameForPropertyKey(key);
  if (transport_name->empty())
    return false;

  auto* image_key = static_cast<const WindowProperty<gfx::ImageSkia*>*>(key);
  if (image_properties_.count(image_key) > 0) {
    const gfx::ImageSkia* value = window->GetProperty(image_key);
    if (value) {
      // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
      SkBitmap bitmap = value->GetRepresentation(1.f).GetBitmap();
      *transport_value = std::make_unique<std::vector<uint8_t>>(
          mojo::ConvertTo<std::vector<uint8_t>>(bitmap));
    } else {
      *transport_value = std::make_unique<std::vector<uint8_t>>();
    }
    return true;
  }

  auto* rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0) {
    *transport_value = GetArray(window, rect_key);
    return true;
  }

  auto* size_key = static_cast<const WindowProperty<gfx::Size*>*>(key);
  if (size_properties_.count(size_key) > 0) {
    *transport_value = GetArray(window, size_key);
    return true;
  }

  auto* string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0) {
    *transport_value = GetArray(window, string_key);
    return true;
  }

  auto* string16_key = static_cast<const WindowProperty<base::string16*>*>(key);
  if (string16_properties_.count(string16_key) > 0) {
    *transport_value = GetArray(window, string16_key);
    return true;
  }

  auto* unguessable_token_key =
      static_cast<const WindowProperty<base::UnguessableToken*>*>(key);
  if (unguessable_token_properties_.count(unguessable_token_key) > 0) {
    *transport_value = GetArray(window, unguessable_token_key);
    return true;
  }

  // window_ptr_properties_ aren't processed here since Window* values aren't
  // transferrable. A post processing step in WindowTree and WindowTreeClient
  // takes care of the conversion.
  if (IsWindowPtrPropertyRegistered(
          static_cast<const WindowProperty<Window*>*>(key))) {
    return true;
  }

  // Handle primitive property types generically.
  DCHECK_GT(primitive_properties_.count(key), 0u);
  PrimitiveType default_value = primitive_properties_[key].default_value;
  // TODO(msw): Using the int64_t accessor is wasteful for smaller types.
  const PrimitiveType value = window->GetPropertyInternal(key, default_value);
  *transport_value = std::make_unique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(value));
  return true;
}

std::string PropertyConverter::GetTransportNameForPropertyKey(const void* key) {
  if (primitive_properties_.count(key) > 0)
    return primitive_properties_[key].transport_name;

  auto* image_key = static_cast<const WindowProperty<gfx::ImageSkia*>*>(key);
  if (image_properties_.count(image_key) > 0)
    return image_properties_[image_key];

  auto* rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0)
    return rect_properties_[rect_key];

  auto* size_key = static_cast<const WindowProperty<gfx::Size*>*>(key);
  if (size_properties_.count(size_key) > 0)
    return size_properties_[size_key];

  auto* string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0)
    return string_properties_[string_key];

  auto* string16_key = static_cast<const WindowProperty<base::string16*>*>(key);
  if (string16_properties_.count(string16_key) > 0)
    return string16_properties_[string16_key];

  auto* unguessable_token_key =
      static_cast<const WindowProperty<base::UnguessableToken*>*>(key);
  if (unguessable_token_properties_.count(unguessable_token_key) > 0)
    return unguessable_token_properties_[unguessable_token_key];

  auto* window_ptr_key = static_cast<const WindowProperty<Window*>*>(key);
  if (window_ptr_properties_.count(window_ptr_key) > 0)
    return window_ptr_properties_[window_ptr_key];

  return std::string();
}

void PropertyConverter::SetPropertyFromTransportValue(
    Window* window,
    const std::string& transport_name,
    const std::vector<uint8_t>* data) {
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.transport_name == transport_name) {
      // aura::Window only supports property types that fit in PrimitiveType.
      if (data->size() != 8u) {
        DVLOG(2) << "Property size mismatch (PrimitiveType): "
                 << transport_name;
        return;
      }
      const PrimitiveType value = mojo::ConvertTo<PrimitiveType>(*data);
      if (!primitive_property.second.validator.Run(value)) {
        DVLOG(2) << "Property value rejected (PrimitiveType): "
                 << transport_name;
        return;
      }
      // TODO(msw): Should aura::Window just store all properties by name?
      window->SetPropertyInternal(
          primitive_property.first, primitive_property.second.property_name,
          nullptr, value, primitive_property.second.default_value);
      return;
    }
  }

  for (const auto& image_property : image_properties_) {
    if (image_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
      const SkBitmap bitmap = mojo::ConvertTo<SkBitmap>(*data);
      const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      window->SetProperty(image_property.first, new gfx::ImageSkia(image));
      return;
    }
  }

  for (const auto& rect_property : rect_properties_) {
    if (rect_property.second == transport_name) {
      if (data->size() != 16u) {
        DVLOG(2) << "Property size mismatch (gfx::Rect): " << transport_name;
        return;
      }
      const gfx::Rect value = mojo::ConvertTo<gfx::Rect>(*data);
      window->SetProperty(rect_property.first, new gfx::Rect(value));
      return;
    }
  }

  for (const auto& size_property : size_properties_) {
    if (size_property.second == transport_name) {
      if (data->size() != 8u) {
        DVLOG(2) << "Property size mismatch (gfx::Size): " << transport_name;
        return;
      }
      const gfx::Size value = mojo::ConvertTo<gfx::Size>(*data);
      window->SetProperty(size_property.first, new gfx::Size(value));
      return;
    }
  }

  for (const auto& string_property : string_properties_) {
    if (string_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      const std::string value = mojo::ConvertTo<std::string>(*data);
      window->SetProperty(string_property.first, new std::string(value));
      return;
    }
  }

  for (const auto& string16_property : string16_properties_) {
    if (string16_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      const base::string16 value = mojo::ConvertTo<base::string16>(*data);
      window->SetProperty(string16_property.first, new base::string16(value));
      return;
    }
  }

  for (const auto& unguessable_token_property : unguessable_token_properties_) {
    if (unguessable_token_property.second == transport_name) {
      base::UnguessableToken token =
          mojo::ConvertTo<base::UnguessableToken>(*data);
      if (token.is_empty()) {
        window->ClearProperty(unguessable_token_property.first);
      } else {
        // |window| takes ownership of the newly allocated token.
        window->SetProperty(unguessable_token_property.first,
                            new base::UnguessableToken(token));
      }
      return;
    }
  }

  // window_ptr_properties_ aren't processed here since Window* values aren't
  // transferrable. A post processing step in WindowTree and WindowTreeClient
  // takes care of the conversion.
  for (const auto& window_ptr_property : window_ptr_properties_) {
    if (window_ptr_property.second == transport_name) {
      LOG(ERROR) << transport_name << " is a registered window property but "
                 << "should not be processed here.";
      return;
    }
  }

  // WARNING: Adding a new map, be sure and update
  // GetPropertyKeyFromTransportName() as well.

  DVLOG(2) << "Unknown mus property name: " << transport_name;
}

bool PropertyConverter::GetPropertyValueFromTransportValue(
    const std::string& transport_name,
    const std::vector<uint8_t>& transport_data,
    PrimitiveType* value) {
  // aura::Window only supports property types that fit in PrimitiveType.
  if (transport_data.size() != 8u) {
    DVLOG(2) << "Property size mismatch (PrimitiveType): " << transport_name;
    return false;
  }
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.transport_name == transport_name) {
      PrimitiveType v = mojo::ConvertTo<PrimitiveType>(transport_data);
      if (!primitive_property.second.validator.Run(v)) {
        DVLOG(2) << "Property value rejected (PrimitiveType): "
                 << transport_name;
        return false;
      }
      *value = v;
      return true;
    }
  }
  return false;
}

void PropertyConverter::RegisterImageSkiaProperty(
    const WindowProperty<gfx::ImageSkia*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  image_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterRectProperty(
    const WindowProperty<gfx::Rect*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  rect_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterSizeProperty(
    const WindowProperty<gfx::Size*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  size_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterStringProperty(
    const WindowProperty<std::string*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  string_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterString16Property(
    const WindowProperty<base::string16*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  string16_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterTimeDeltaProperty(
    const WindowProperty<base::TimeDelta>* property,
    const char* transport_name) {
  // TimeDelta is internally handled (by class_property) as a primitive
  // value (int64_t) . See ClassPropertyCaster<base::TimeDelta> for details.
  RegisterPrimitiveProperty(property, transport_name,
                            CreateAcceptAnyValueCallback());
}

void PropertyConverter::RegisterUnguessableTokenProperty(
    const WindowProperty<base::UnguessableToken*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  unguessable_token_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

void PropertyConverter::RegisterWindowPtrProperty(
    const WindowProperty<Window*>* property,
    const char* transport_name) {
  DCHECK(!IsTransportNameRegistered(transport_name))
      << "Property already registered: " << transport_name;
  window_ptr_properties_[property] = transport_name;
  transport_names_.insert(transport_name);
}

const WindowProperty<Window*>* PropertyConverter::GetWindowPtrProperty(
    const std::string& transport_name) const {
  for (const auto& iter : window_ptr_properties_) {
    if (transport_name == iter.second)
      return iter.first;
  }
  return nullptr;
}

bool PropertyConverter::IsWindowPtrPropertyRegistered(
    const WindowProperty<Window*>* property) const {
  return window_ptr_properties_.find(property) != window_ptr_properties_.end();
}

base::flat_map<std::string, std::vector<uint8_t>>
PropertyConverter::GetTransportProperties(Window* window) {
  base::flat_map<std::string, std::vector<uint8_t>> properties;
  std::string name;
  std::unique_ptr<std::vector<uint8_t>> value;
  for (const void* key : window->GetAllPropertyKeys()) {
    if (ConvertPropertyForTransport(window, key, &name, &value))
      properties[name] = value ? std::move(*value) : std::vector<uint8_t>();
  }
  return properties;
}

}  // namespace aura
