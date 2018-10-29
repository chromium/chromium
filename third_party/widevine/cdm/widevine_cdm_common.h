// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
#define WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_

// This file defines constants common to all Widevine CDM versions.

// "alpha" is a temporary name until a convention is defined.
const char kWidevineKeySystem[] = "com.widevine.alpha";

const char kWidevineCdmGuid[] = "AD87877A-0213-49A8-8849-9E93B075E477";

// Widevine CDM files are in a directory with this name. This path is also
// hardcoded in some build files and changing it requires changing the build
// files as well.
const char kWidevineCdmBaseDirectory[] = "WidevineCdm";

// This name is used by UMA. Do not change it!
const char kWidevineKeySystemNameForUMA[] = "Widevine";

const char kWidevineCdmDisplayName[] = "Widevine Content Decryption Module";

// Identifier used by the PluginPrivateFileSystem to identify the files stored
// for the Widevine CDM. This is used to store persistent files. As the files
// were initially used by the CDM running as a pepper plugin, this ID is based
// on the pepper plugin MIME type. Changing this will result in any existing
// saved files becoming inaccesssible.
const char kWidevineCdmFileSystemId[] = "application_x-ppapi-widevine-cdm";

// Name of the CDM library.
const char kWidevineCdmLibraryName[] = "widevinecdm";

#endif  // WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
