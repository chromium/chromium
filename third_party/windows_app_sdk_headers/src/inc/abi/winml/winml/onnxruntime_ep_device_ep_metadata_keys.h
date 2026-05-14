// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

// This file contains well-known keys for OrtEpDevice and OrtHardwareDevice metadata entries.
// It does NOT specify all available metadata keys.

// Key for the execution provider version string. This should be available for all plugin EPs.
static const char* const kOrtEpDevice_EpMetadataKey_Version = "version";

// Key for the execution provider OS driver version.
static const char* const kOrtEpDevice_EpMetadataKey_OSDriverVersion = "os_driver_version";

// Prefix for execution provider compatibility information stored in model metadata.
// Used when generating EP context models to store compatibility strings for each EP.
// Full key format: "ep_compatibility_info.<EP_TYPE>"
static const char* const kOrtModelMetadata_EpCompatibilityInfoPrefix = "ep_compatibility_info.";

// Key for the execution provider library path (for dynamically loaded EPs)
static const char* const kOrtEpDevice_EpMetadataKey_LibraryPath = "library_path";

// Optional metadata key to determine if a OrtHardwareDevice represents a virtual (non-hardware) device.
// Possible values:
//  - "0": OrtHardwareDevice is not virtual (i.e., actual hardware device). This is the assumed default value
//         if this metadata key is not present.
//  - "1": OrtHardwareDevice is virtual.
static const char* const kOrtHardwareDevice_MetadataKey_IsVirtual = "is_virtual";
