// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/bind.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/features_compiler_test.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

template <typename ExpectedT, typename ActualT>
void ExpectSpanEqual(base::span<ExpectedT> expected,
                     base::span<const ActualT> actual,
                     std::string_view name) {
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected)) << name;
}

template <typename T>
void ExpectOptionalSpanEqual(const std::optional<std::vector<T>>& expected,
                             const std::optional<base::span<const T>>& actual,
                             std::string_view name) {
  if (expected.has_value() != actual.has_value()) {
    ADD_FAILURE() << "Mismatched optional lists for " << name << ": "
                  << expected.has_value() << " vs " << actual.has_value();
    return;
  }
  if (expected) {
    ExpectSpanEqual(base::span(*expected), *actual, name);
  }
}

constexpr bool kDefaultAutoGrant = true;
constexpr bool kDefaultInternal = false;
constexpr bool kDefaultRequiresDelegatedAvailabilityCheck = false;

constexpr auto kDescriptorBlocklist =
    std::to_array<std::string_view>({"blocked"});
constexpr auto kDescriptorAllowlist =
    std::to_array<std::string_view>({"allowed"});
constexpr auto kDescriptorDependencies =
    std::to_array<std::string_view>({"dependency"});
constexpr auto kDescriptorExtensionTypes =
    std::to_array<Manifest::Type>({Manifest::Type::kExtension});
constexpr auto kDescriptorSessionTypes =
    std::to_array<mojom::FeatureSessionType>(
        {mojom::FeatureSessionType::kRegular});
constexpr auto kDescriptorContexts = std::to_array<mojom::ContextType>(
    {mojom::ContextType::kPrivilegedExtension});
constexpr auto kDescriptorPlatforms =
    std::to_array<Feature::Platform>({Feature::LINUX_PLATFORM});
constexpr auto kDescriptorMatches =
    std::to_array<std::string_view>({"https://example.com/*"});
constexpr SimpleFeatureData kDescriptor = {
    .feature =
        {
            .name = "descriptor",
            .alias = StaticCString("descriptorAlias"),
            .source = StaticCString("descriptorSource"),
            .no_parent = true,
        },
    .config =
        {
            .blocklist = StaticSpan(kDescriptorBlocklist),
            .allowlist = StaticSpan(kDescriptorAllowlist),
            .dependencies = StaticSpan(kDescriptorDependencies),
            .extension_types = StaticSpan(kDescriptorExtensionTypes),
            .session_types = StaticSpan(kDescriptorSessionTypes),
            .contexts = StaticSpan(kDescriptorContexts),
            .platforms = StaticSpan(kDescriptorPlatforms),
            .match_patterns = StaticSpan(kDescriptorMatches),
            .location = SimpleFeature::Location::kUnpacked,
            .min_manifest_version = 2,
            .max_manifest_version = 3,
            .command_line_switch = StaticCString("descriptor-switch"),
            .feature_flag = StaticCString("DescriptorFeature"),
            .channel = version_info::Channel::BETA,
            .component_extensions_auto_granted = false,
            .is_internal = true,
            .requires_delegated_availability_check = true,
            .developer_mode_only = true,
            .disallow_for_service_workers = true,
        },
};

}  // namespace

// A utility object for comparing a feature with its expected value.
struct FeatureComparator {
 public:
  explicit FeatureComparator(std::string_view name);
  ~FeatureComparator();

  void CompareFeature(const SimpleFeature* feature);

  std::string name;
  std::vector<std::string> blocklist;
  std::vector<std::string> allowlist;
  std::vector<std::string> dependencies;
  std::vector<Manifest::Type> extension_types;
  std::vector<mojom::FeatureSessionType> session_types;
  std::optional<std::vector<mojom::ContextType>> contexts;
  std::vector<Feature::Platform> platforms;

  std::vector<std::string> match_patterns;

  std::optional<SimpleFeature::Location> location;
  std::optional<int> min_manifest_version;
  std::optional<int> max_manifest_version;
  std::optional<std::string_view> command_line_switch;
  std::optional<std::string_view> feature_flag;
  std::optional<version_info::Channel> channel;

  std::string alias;
  std::string source;

  bool component_extensions_auto_granted;
  bool internal;
  bool requires_delegated_availability_check;
  bool developer_mode_only = false;
  bool disallow_for_service_workers = false;
  bool no_parent = false;
};

FeatureComparator::FeatureComparator(std::string_view name)
    : name(name),
      component_extensions_auto_granted(kDefaultAutoGrant),
      internal(kDefaultInternal),
      requires_delegated_availability_check(
          kDefaultRequiresDelegatedAvailabilityCheck) {}

FeatureComparator::~FeatureComparator() = default;

void FeatureComparator::CompareFeature(const SimpleFeature* feature) {
  ASSERT_TRUE(feature);
  EXPECT_EQ(name, feature->name());
  ExpectSpanEqual(base::span(blocklist), feature->blocklist(), name);
  ExpectSpanEqual(base::span(allowlist), feature->allowlist(), name);
  ExpectSpanEqual(base::span(dependencies), feature->dependencies(), name);
  ExpectSpanEqual(base::span(extension_types), feature->extension_types(),
                  name);
  ExpectSpanEqual(base::span(session_types), feature->session_types(), name);
  ExpectOptionalSpanEqual(contexts, feature->contexts(), name);
  ExpectSpanEqual(base::span(platforms), feature->platforms(), name);
  ExpectSpanEqual(base::span(match_patterns), feature->match_patterns(), name);
  EXPECT_EQ(location, feature->location()) << name;
  EXPECT_EQ(min_manifest_version, feature->min_manifest_version()) << name;
  EXPECT_EQ(max_manifest_version, feature->max_manifest_version()) << name;
  EXPECT_EQ(component_extensions_auto_granted,
            feature->component_extensions_auto_granted())
      << name;
  EXPECT_EQ(command_line_switch, feature->command_line_switch()) << name;
  const StaticCString actual_feature_flag = feature->feature_flag();
  EXPECT_EQ(feature_flag.has_value(), actual_feature_flag.has_value()) << name;
  if (feature_flag) {
    EXPECT_EQ(*feature_flag, actual_feature_flag.string_view()) << name;
  }
  EXPECT_EQ(channel, feature->channel()) << name;
  EXPECT_EQ(internal, feature->IsInternal()) << name;
  EXPECT_EQ(alias, feature->alias()) << name;
  EXPECT_EQ(source, feature->source()) << name;
  EXPECT_EQ(requires_delegated_availability_check,
            feature->RequiresDelegatedAvailabilityCheck())
      << name;
  EXPECT_EQ(developer_mode_only, feature->developer_mode_only()) << name;
  EXPECT_EQ(disallow_for_service_workers,
            feature->disallow_for_service_workers())
      << name;
  EXPECT_EQ(no_parent, feature->no_parent()) << name;
}

TEST(FeaturesGenerationTest, DescriptorBackedFeature) {
  SimpleFeature feature{StaticFeatureData(kDescriptor)};
  FeatureComparator comparator("descriptor");
  comparator.blocklist = {"blocked"};
  comparator.allowlist = {"allowed"};
  comparator.dependencies = {"dependency"};
  comparator.extension_types = {Manifest::Type::kExtension};
  comparator.session_types = {mojom::FeatureSessionType::kRegular};
  comparator.contexts = {
      mojom::ContextType::kPrivilegedExtension,
  };
  comparator.platforms = {Feature::LINUX_PLATFORM};
  comparator.match_patterns = {"https://example.com/*"};
  comparator.location = SimpleFeature::Location::kUnpacked;
  comparator.min_manifest_version = 2;
  comparator.max_manifest_version = 3;
  comparator.command_line_switch = "descriptor-switch";
  comparator.feature_flag = "DescriptorFeature";
  comparator.channel = version_info::Channel::BETA;
  comparator.component_extensions_auto_granted = false;
  comparator.internal = true;
  comparator.requires_delegated_availability_check = true;
  comparator.developer_mode_only = true;
  comparator.disallow_for_service_workers = true;
  comparator.no_parent = true;
  comparator.alias = "descriptorAlias";
  comparator.source = "descriptorSource";
  comparator.CompareFeature(&feature);
}

TEST(FeaturesGenerationTest, FeaturesTest) {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  map.emplace("requires_delegated_availability_check",
              base::BindLambdaForTesting(
                  [&](const std::string& api_full_name,
                      const Extension* extension, mojom::ContextType context,
                      const GURL& url, Feature::Platform platform,
                      int context_id, bool check_developer_mode,
                      const ContextData& context_data) { return false; }));
  ExtensionsClient::Get()->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(map));
  FeatureProvider provider;
  CompilerTestAddFeaturesMethod(&provider);

  auto GetAsSimpleFeature = [&provider](const std::string& name) {
    const Feature* feature = provider.GetFeature(name);
    // Shame we can't test this more safely, but if our feature is declared as
    // the wrong class, things should blow up in a spectacular fashion.
    return static_cast<const SimpleFeature*>(feature);
  };

  auto GetAsComplexFeature = [&provider](const std::string& name) {
    const Feature* feature = provider.GetFeature(name);
    // Shame we can't test this more safely, but if our feature is declared as
    // the wrong class, things should blow up in a spectacular fashion.
    return static_cast<const ComplexFeature*>(feature);
  };

  // Check some simple features for accuracy.
  {
    const SimpleFeature* feature = GetAsSimpleFeature("alpha");
    FeatureComparator comparator("alpha");
    comparator.dependencies = {"permission:alpha"};
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.channel = version_info::Channel::STABLE;
    comparator.max_manifest_version = 1;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("beta");
    FeatureComparator comparator("beta");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.channel = version_info::Channel::DEV;
    comparator.extension_types = {Manifest::Type::kExtension,
                                  Manifest::Type::kPlatformApp};
    comparator.location = SimpleFeature::Location::kComponent;
    comparator.allowlist = {"ABCDEF0123456789ABCDEF0123456789ABCDEF01",
                            "10FEDCBA9876543210FEDCBA9876543210FEDCBA"};
    comparator.blocklist = {"0123456789ABCDEF0123456789ABCDEF01234567",
                            "76543210FEDCBA9876543210FEDCBA9876543210"};
    comparator.component_extensions_auto_granted = false;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("gamma");
    FeatureComparator comparator("gamma");
    comparator.channel = version_info::Channel::BETA;
    comparator.platforms = {Feature::WIN_PLATFORM, Feature::MACOSX_PLATFORM};
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.dependencies = {"permission:gamma"};
    comparator.extension_types = {Manifest::Type::kExtension};
    comparator.internal = true;
    comparator.CompareFeature(feature);

    // A child feature should inherit all fields from its parent, except in the
    // case that it specifies its own value. Thus, we reuse |comparator|.
    feature = GetAsSimpleFeature("gamma.child");
    comparator.name = "gamma.child";
    comparator.allowlist = {"0123456789ABCDEF0123456789ABCDEF01234567"};
    comparator.platforms = {Feature::LINUX_PLATFORM};
    comparator.dependencies.clear();
    comparator.CompareFeature(feature);
  }
  {
    // Features that specify 'noparent' should not inherit features from any
    // other feature.
    const SimpleFeature* feature = GetAsSimpleFeature("gamma.unparented");
    FeatureComparator comparator("gamma.unparented");
    comparator.blocklist = {"0123456789ABCDEF0123456789ABCDEF01234567"};
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kUnprivilegedExtension});
    comparator.channel = version_info::Channel::DEV;
    comparator.no_parent = true;
    comparator.CompareFeature(feature);
  }
  {
    const ComplexFeature* complex_feature =
        GetAsComplexFeature("gamma.complex_unparented");
    FeatureComparator comparator("gamma.complex_unparented");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kUnprivilegedExtension});
    comparator.channel = version_info::Channel::STABLE;
    comparator.match_patterns = {"*://complex.example/*"};
    comparator.no_parent = true;
    // We cheat and have both children exactly the same for ease of comparing;
    // complex features are tested more thoroughly below.
    complex_feature->VisitFeatures([&](const Feature& feature) {
      comparator.CompareFeature(static_cast<const SimpleFeature*>(&feature));
      return true;
    });
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("delta");
    FeatureComparator comparator("delta");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension, mojom::ContextType::kWebUi});
    comparator.channel = version_info::Channel::DEV;
    comparator.match_patterns = {"*://example.com/*"};
    comparator.min_manifest_version = 2;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("pi");
    FeatureComparator comparator("pi");
    comparator.contexts =
        std::vector<mojom::ContextType>({mojom::ContextType::kUntrustedWebUi});
    comparator.channel = version_info::Channel::STABLE;
    comparator.match_patterns = {"chrome-untrusted://foo/*"};
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("allEnum");
    FeatureComparator comparator("allEnum");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension,
         mojom::ContextType::kPrivilegedWebPage,
         mojom::ContextType::kContentScript,
         mojom::ContextType::kOffscreenExtension,
         mojom::ContextType::kUserScript, mojom::ContextType::kWebPage,
         mojom::ContextType::kWebUi, mojom::ContextType::kUntrustedWebUi,
         mojom::ContextType::kUnprivilegedExtension});
    comparator.extension_types = {Manifest::Type::kExtension,
                                  Manifest::Type::kHostedApp,
                                  Manifest::Type::kLegacyPackagedApp,
                                  Manifest::Type::kPlatformApp,
                                  Manifest::Type::kSharedModule,
                                  Manifest::Type::kTheme,
                                  Manifest::Type::kLoginScreenExtension,
                                  Manifest::Type::kChromeOSSystemExtension};
    comparator.channel = version_info::Channel::BETA;
    comparator.CompareFeature(feature);
  }
  {
    // Omega is imported from a second .json file.
    const SimpleFeature* feature = GetAsSimpleFeature("omega");
    FeatureComparator comparator("omega");
    comparator.contexts =
        std::vector<mojom::ContextType>({mojom::ContextType::kWebPage});
    comparator.channel = version_info::Channel::DEV;
    comparator.min_manifest_version = 2;
    comparator.CompareFeature(feature);
  }
  {
    // Features specifying 'nocompile' should not be generated at all.
    const SimpleFeature* feature = GetAsSimpleFeature("uncompiled");
    EXPECT_FALSE(feature);
  }

  // Test complex features.
  {
    const ComplexFeature* feature = GetAsComplexFeature("complex");
    ASSERT_TRUE(feature);
    size_t feature_count = 0u;
    bool saw_stable = false;
    bool saw_beta = false;
    feature->VisitFeatures([&](const Feature& child) {
      ++feature_count;
      const auto* simple_child = static_cast<const SimpleFeature*>(&child);
      FeatureComparator comparator("complex");
      comparator.contexts = std::vector<mojom::ContextType>(
          {mojom::ContextType::kPrivilegedExtension});
      comparator.extension_types = {Manifest::Type::kExtension};
      if (simple_child->channel() == version_info::Channel::BETA) {
        saw_beta = true;
        comparator.channel = version_info::Channel::BETA;
        comparator.allowlist = {"0123456789ABCDEF0123456789ABCDEF01234567"};
      } else {
        saw_stable = true;
        comparator.channel = version_info::Channel::STABLE;
      }
      comparator.CompareFeature(simple_child);
      return true;
    });
    EXPECT_EQ(2u, feature_count);
    EXPECT_TRUE(saw_stable);
    EXPECT_TRUE(saw_beta);

    // The child inherits its properties from the stable default parent.
    const SimpleFeature* child_feature = GetAsSimpleFeature("complex.child");
    FeatureComparator comparator("complex.child");
    comparator.channel = version_info::Channel::STABLE;
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.extension_types = {Manifest::Type::kExtension};
    comparator.platforms = {Feature::WIN_PLATFORM};
    comparator.dependencies = {"permission:complex.child"};
    comparator.CompareFeature(child_feature);
  }

  // Test API aliases.
  {
    const SimpleFeature* feature = GetAsSimpleFeature("alias");
    FeatureComparator comparator("alias");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.channel = version_info::Channel::STABLE;
    comparator.source = "alias_source";
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("alias_source");
    FeatureComparator comparator("alias_source");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension});
    comparator.channel = version_info::Channel::STABLE;
    comparator.alias = "alias";
    comparator.CompareFeature(feature);
  }
  {
    const Feature* feature = provider.GetFeature("complex_alias");
    ASSERT_EQ("", feature->alias());
    ASSERT_EQ("complex_alias_source", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("complex_alias_source");
    ASSERT_EQ("complex_alias", feature->alias());
    ASSERT_EQ("", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("parent_source");
    ASSERT_EQ("parent_source_alias", feature->alias());
    ASSERT_EQ("", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("parent_source.child");
    ASSERT_EQ("parent_source_alias", feature->alias());
    ASSERT_EQ("", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("parent_source.child_source");
    ASSERT_EQ("parent_source_child_alias", feature->alias());
    ASSERT_EQ("", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("alias_parent");
    ASSERT_EQ("", feature->alias());
    ASSERT_EQ("", feature->source());
  }
  {
    const Feature* feature = provider.GetFeature("alias_parent.child");
    ASSERT_EQ("", feature->alias());
    ASSERT_EQ("child_source", feature->source());
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("empty_contexts");
    FeatureComparator comparator("empty_contexts");
    comparator.channel = version_info::Channel::BETA;
    comparator.contexts = std::vector<mojom::ContextType>();
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature =
        GetAsSimpleFeature("requires_delegated_availability_check");
    FeatureComparator comparator("requires_delegated_availability_check");
    comparator.channel = version_info::Channel::BETA;
    comparator.contexts =
        std::vector<mojom::ContextType>{mojom::ContextType::kWebPage};
    comparator.requires_delegated_availability_check = true;
    comparator.CompareFeature(feature);
  }
}

}  // namespace extensions
