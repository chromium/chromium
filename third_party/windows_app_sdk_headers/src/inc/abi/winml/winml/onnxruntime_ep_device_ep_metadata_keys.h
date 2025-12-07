// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

// This file contains well-known keys for OrtEpDevice EP metadata entries.
// It does NOT specify all available metadata keys.

// Key for the execution provider version string. This should be available for all plugin EPs.
static const char* const kOrtEpDevice_EpMetadataKey_Version = "version";

// Prefix for execution provider compatibility information stored in model metadata.
// Used when generating EP context models to store compatibility strings for each EP.
// Full key format: "ep_compatibility_info.<EP_TYPE>"
static const char* const kOrtModelMetadata_EpCompatibilityInfoPrefix = "ep_compatibility_info.";

// Key for the execution provider library path (for dynamically loaded EPs)
static const char* const kOrtEpDevice_EpMetadataKey_LibraryPath = "library_path";
