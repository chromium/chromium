// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/io_surface_context.h"

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace ui {

namespace {

// The global map from window number and window ordering to context data.
using TypeMap = std::map<IOSurfaceContext::Type, IOSurfaceContext*>;

TypeMap* GetTypeMap() {
  static auto* type_map = new TypeMap();
  return type_map;
}

}  // namespace

// static
scoped_refptr<IOSurfaceContext>
IOSurfaceContext::Get(Type type) {
  TRACE_EVENT0("browser", "IOSurfaceContext::Get");

  // Return the context for this type, if it exists.
  auto* type_map = GetTypeMap();
  TypeMap::iterator found = type_map->find(type);
  if (found != type_map->end()) {
    DCHECK(!found->second->poisoned_);
    return found->second;
  }

  base::ScopedTypeRef<CGLContextObj> cgl_context;
  CGLError error = kCGLNoError;

  // Create the pixel format object for the context.
  std::vector<CGLPixelFormatAttribute> attribs;
  attribs.push_back(kCGLPFADepthSize);
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  if (gl::GLContext::SwitchableGPUsSupported())
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  GLint number_virtual_screens = 0;
  base::ScopedTypeRef<CGLPixelFormatObj> pixel_format;
  error = CGLChoosePixelFormat(&attribs.front(),
                               pixel_format.InitializeInto(),
                               &number_virtual_screens);
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create pixel format object.";
    return nullptr;
  }

  // Create all contexts in the same share group so that the textures don't
  // need to be recreated when transitioning contexts.
  CGLContextObj share_context = nullptr;
  if (!type_map->empty())
    share_context = type_map->begin()->second->cgl_context();
  error = CGLCreateContext(
      pixel_format, share_context, cgl_context.InitializeInto());
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create context object.";
    return nullptr;
  }

  return new IOSurfaceContext(type, cgl_context);
}

void IOSurfaceContext::PoisonContextAndSharegroup() {
  if (poisoned_)
    return;

  auto* type_map = GetTypeMap();
  for (TypeMap::iterator it = type_map->begin(); it != type_map->end(); ++it) {
    it->second->poisoned_ = true;
  }
  type_map->clear();
}

IOSurfaceContext::IOSurfaceContext(
    Type type, base::ScopedTypeRef<CGLContextObj> cgl_context)
    : type_(type), cgl_context_(cgl_context), poisoned_(false) {
  auto* type_map = GetTypeMap();
  DCHECK(type_map->find(type_) == type_map->end());
  type_map->insert(std::make_pair(type_, this));

  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

IOSurfaceContext::~IOSurfaceContext() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);

  auto* type_map = GetTypeMap();
  if (!poisoned_) {
    DCHECK(type_map->find(type_) != type_map->end());
    DCHECK(type_map->find(type_)->second == this);
    type_map->erase(type_);
  } else {
    TypeMap::const_iterator found = type_map->find(type_);
    if (found != type_map->end())
      DCHECK(found->second != this);
  }
}

void IOSurfaceContext::OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  // Recreate all browser-side GL contexts whenever the GPU switches. If this
  // is not done, performance will suffer.
  // http://crbug.com/361493
  PoisonContextAndSharegroup();
}

}  // namespace ui
