// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
#define WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_

#include "build/build_config.h"
#include "media/cdm/cdm_type.h"  // nogncheck

// Default constants common to all Widevine CDMs.

// "alpha" is a temporary name until a convention is defined.
const char kWidevineKeySystem[] = "com.widevine.alpha";

#if BUILDFLAG(IS_WIN)
// An sub key system of `kWidevineKeySystem` only used in experiments.
const char kWidevineExperimentKeySystem[] = "com.widevine.alpha.experiment";
#endif  // BUILDFLAG(IS_WIN)

// Widevine CDM files are in a directory with this name. This path is also
// hardcoded in some build files and changing it requires changing the build
// files as well.
const char kWidevineCdmBaseDirectory[] = "WidevineCdm";

// Media Foundation Widevine CDM files are in a directory with this name.
const char kMediaFoundationWidevineCdmBaseDirection[] =
    "MediaFoundationWidevineCdm";

// This name is used by UMA. Do not change it!
const char kWidevineKeySystemNameForUMA[] = "Widevine";

// Name of the CDM library.
const char kWidevineCdmLibraryName[] = "widevinecdm";

const char kWidevineCdmDisplayName[] = "Widevine Content Decryption Module";

// TODO(crbug.com/1231162): Remove the string identifier once we've migrated off
// of the PluginPrivateFileSystem.
// Identifier used for both CDM process site isolation and by the
// PluginPrivateFileSystem to identify the files stored for the Widevine CDM.
// This is used to store persistent files. As the files were initially used by
// the CDM running as a pepper plugin, this ID is based on the pepper plugin
// MIME type. Changing this will result in any existing saved files becoming
// inaccessible.
const media::CdmType kWidevineCdmType{0x05d908e5dcca9960ull,
                                      0xcd92d30eac98157aull};

// Constants specific to Windows MediaFoundation-based Widevine CDM library.
#if BUILDFLAG(IS_WIN)
const char kMediaFoundationWidevineCdmLibraryName[] = "Google.Widevine.CDM";
const char kMediaFoundationWidevineCdmDisplayName[] =
    "Google Widevine Windows CDM";
// TODO(crbug.com/1231162): Remove the string identifier once we've migrated off
// of the PluginPrivateFileSystem.
const media::CdmType kMediaFoundationWidevineCdmType{0x8e73dec793bf5adcull,
                                                     0x27e572c9a1fd930eull};
#endif  // BUILDFLAG(IS_WIN)

#endif  // WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
