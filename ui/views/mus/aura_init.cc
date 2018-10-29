// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/aura_init.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/catalog/public/cpp/resource_loader.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_views_delegate.h"

namespace views {

AuraInit::InitParams::InitParams() : resource_file("views_mus_resources.pak") {}

AuraInit::InitParams::~InitParams() = default;

AuraInit::AuraInit() {
  if (!ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<MusViewsDelegate>();
}

AuraInit::~AuraInit() = default;

// static
std::unique_ptr<AuraInit> AuraInit::Create(const InitParams& params) {
  // Using 'new' to access a non-public constructor. go/totw/134
  std::unique_ptr<AuraInit> aura_init = base::WrapUnique(new AuraInit());
  if (!aura_init->Init(params))
    aura_init.reset();
  return aura_init;
}

bool AuraInit::Init(const InitParams& params) {
  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);

  MusClient::InitParams mus_params;
  mus_params.connector = params.connector;
  mus_params.identity = params.identity;
  mus_params.io_task_runner = params.io_task_runner;
  mus_params.create_wm_state = true;
  mus_params.use_accessibility_host = params.use_accessibility_host;
  mus_client_ = std::make_unique<MusClient>(mus_params);
  ui::MaterialDesignController::Initialize();
  if (!InitializeResources(params))
    return false;

  ui::InitializeInputMethodForTesting();
  return true;
}

bool AuraInit::InitializeResources(const InitParams& params) {
  // Resources may have already been initialized (e.g. when chrome with mash is
  // used to launch the current app).
  if (ui::ResourceBundle::HasSharedInstance())
    return true;

  std::set<std::string> resource_paths({params.resource_file});
  if (!params.resource_file_200.empty())
    resource_paths.insert(params.resource_file_200);

  catalog::ResourceLoader loader;
  filesystem::mojom::DirectoryPtr directory;
  params.connector->BindInterface(catalog::mojom::kServiceName, &directory);
  // TODO(jonross): if this proves useful in resolving the crash of
  // mash_unittests then switch AuraInit to have an Init method, returning a
  // bool for success. Then update all callsites to use this to determine the
  // shutdown of their ServiceContext.
  // One cause of failure is that the peer has closed, but we have not been
  // notified yet. It is not possible to complete initialization, so exit now.
  // Calling services will shutdown ServiceContext as appropriate.
  if (!loader.OpenFiles(std::move(directory), resource_paths))
    return false;
  if (params.register_path_provider)
    ui::RegisterPathProvider();
  base::File pak_file = loader.TakeFile(params.resource_file);
  base::File pak_file_2 = pak_file.Duplicate();
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      std::move(pak_file), base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      std::move(pak_file_2), ui::SCALE_FACTOR_100P);
  if (!params.resource_file_200.empty())
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
        loader.TakeFile(params.resource_file_200), ui::SCALE_FACTOR_200P);
  return true;
}

}  // namespace views
