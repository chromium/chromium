// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_RAW_PTR_PLUGIN_PLUGINCONFIG_H_
#define TOOLS_CLANG_RAW_PTR_PLUGIN_PLUGINCONFIG_H_

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "llvm/Support/YAMLTraits.h"

namespace raw_ptr_plugin {

// Configuration for enforcing `raw_ptr` within template arguments
// of container types (e.g., `std::vector<T*>`).
struct RawPtrContainerConfig {
  bool enabled = false;
  std::vector<std::string> included_container_types;
  std::vector<std::string> excluded_container_types;
};

// Helper structure for parsing YAML, allowing us to distinguish between
// omitted fields and explicitly set fields (e.g., enabled: false).
struct ParsedRawPtrContainerConfig {
  std::optional<bool> enabled;
  std::optional<bool> override_defaults;
  std::optional<std::vector<std::string>> included_container_types;
  std::optional<std::vector<std::string>> excluded_container_types;
};

struct ParsedRawPtrPluginConfig {
  ParsedRawPtrContainerConfig container_config;
};

// Root configuration for the `raw-ptr-plugin`.
struct RawPtrPluginConfig {
  RawPtrContainerConfig container_config;

  // Default config used when `check-containers` is set, but `config-path`
  // isn't. This enables the check on the relevant standard library containers.
  static RawPtrPluginConfig Default() {
    RawPtrPluginConfig config;
    config.container_config.enabled = true;
    config.container_config.included_container_types = {
        "base::flat_tree",
        "std::deque",
        "std::list",
        "std::set",
        "std::stack",
        "std::tuple",
        "std::unordered_set",
        "std::variant",
        "std::vector",
        "std::queue",
    };
    return config;
  }

  // Merges the parsed configuration into the active configuration.
  // Performs validation on the parsed configuration. If validation fails,
  // returns a non-empty error message. Otherwise, applies the changes
  // (including overriding defaults if requested) and returns an empty string.
  std::string Merge(const ParsedRawPtrPluginConfig& parsed) {
    const auto& p_container = parsed.container_config;

    // 1. Validate mutual exclusivity in override mode
    // In override mode (starting from a blank slate), specifying both
    // `included_types` (positive matching) and `excluded_types` (negative
    // matching) is logically contradictory and ambiguous. We only allow opting
    // into specific containers OR opting out of all containers except a few,
    // not both.
    if (p_container.override_defaults.value_or(false) &&
        p_container.included_container_types.has_value() &&
        p_container.excluded_container_types.has_value() &&
        !p_container.included_container_types->empty() &&
        !p_container.excluded_container_types->empty()) {
      return "Config error: 'included_types' and 'excluded_types' cannot be "
             "defined at the same time when 'override_defaults' is true.";
    }

    if (p_container.enabled.has_value()) {
      container_config.enabled = *p_container.enabled;
    }

    // 2. Wipe default STL containers if user requested an override
    if (p_container.override_defaults.value_or(false)) {
      container_config.included_container_types.clear();
      container_config.excluded_container_types.clear();
    }

    // Reuse range-based merge helper
    auto dedup_merge = [](std::vector<std::string>& dest,
                          const std::optional<std::vector<std::string>>& src) {
      if (!src.has_value()) {
        return;
      }
      llvm::append_range(dest, *src);
      llvm::sort(dest);
      dest.erase(llvm::unique(dest), dest.end());
    };

    // 3. Process normal inclusions and exclusions
    dedup_merge(container_config.included_container_types,
                p_container.included_container_types);
    dedup_merge(container_config.excluded_container_types,
                p_container.excluded_container_types);

    // 4. Exclusions take precedence over inclusions
    // If it's excluded, it shouldn't co-exist as an included type.
    if (!container_config.excluded_container_types.empty()) {
      llvm::erase_if(container_config.included_container_types,
                     [&](const std::string& type) {
                       return llvm::is_contained(
                           container_config.excluded_container_types, type);
                     });
    }

    return "";  // Success
  }
};

}  // namespace raw_ptr_plugin

namespace llvm::yaml {

template <>
struct MappingTraits<raw_ptr_plugin::ParsedRawPtrContainerConfig> {
  static void mapping(IO& io,
                      raw_ptr_plugin::ParsedRawPtrContainerConfig& config) {
    io.mapOptional("enabled", config.enabled);
    io.mapOptional("override_defaults", config.override_defaults);
    io.mapOptional("included_types", config.included_container_types);
    io.mapOptional("excluded_types", config.excluded_container_types);
  }
};

template <>
struct MappingTraits<raw_ptr_plugin::ParsedRawPtrPluginConfig> {
  static void mapping(IO& io,
                      raw_ptr_plugin::ParsedRawPtrPluginConfig& config) {
    io.mapOptional("check_raw_ptr_in_container", config.container_config);
  }
};
}  // end namespace llvm::yaml

#endif  // TOOLS_CLANG_RAW_PTR_PLUGIN_PLUGINCONFIG_H_
