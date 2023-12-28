// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/bind.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/features_compiler_test.h"

namespace extensions {

namespace {

template <typename T>
void ExpectVectorsEqual(std::vector<T> expected,
                        std::vector<T> actual,
                        const std::string& name) {
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  EXPECT_EQ(expected, actual) << name;
}

template <typename T>
void ExpectOptionalVectorsEqual(const std::optional<std::vector<T>>& expected,
                                const std::optional<std::vector<T>>& actual,
                                const std::string& name) {
  if (expected.has_value() != actual.has_value()) {
    ADD_FAILURE() << "Mismatched optional vectors for " << name << ": "
                  << expected.has_value() << " vs " << actual.has_value();
    return;
  }
  if (expected.has_value())
    ExpectVectorsEqual(*expected, *actual, name);
}

const bool kDefaultAutoGrant = true;
const bool kDefaultInternal = false;
const bool kDefaultRequiresDelegatedAvailabilityCheck = false;

}  // namespace

// A utility object for comparing a feature with its expected value.
struct FeatureComparator {
 public:
  explicit FeatureComparator(const std::string& name);
  ~FeatureComparator();

  void CompareFeature(const SimpleFeature* feature);

  std::string name;
  std::vector<std::string> blocklist;
  std::vector<std::string> allowlist;
  std::vector<std::string> dependencies;
  std::vector<Manifest::Type> extension_types;
  std::optional<std::vector<mojom::ContextType>> contexts;
  std::vector<Feature::Platform> platforms;

  URLPatternSet matches;

  std::optional<SimpleFeature::Location> location;
  std::optional<int> min_manifest_version;
  std::optional<int> max_manifest_version;
  std::optional<std::string> command_line_switch;
  std::optional<version_info::Channel> channel;

  std::string alias;
  std::string source;

  bool component_extensions_auto_granted;
  bool internal;
  bool requires_delegated_availability_check;
};

FeatureComparator::FeatureComparator(const std::string& name)
    : name(name),
      component_extensions_auto_granted(kDefaultAutoGrant),
      internal(kDefaultInternal),
      requires_delegated_availability_check(
          kDefaultRequiresDelegatedAvailabilityCheck) {}

FeatureComparator::~FeatureComparator() = default;

void FeatureComparator::CompareFeature(const SimpleFeature* feature) {
  ASSERT_TRUE(feature);
  EXPECT_EQ(name, feature->name());
  ExpectVectorsEqual(blocklist, feature->blocklist(), name);
  ExpectVectorsEqual(allowlist, feature->allowlist(), name);
  ExpectVectorsEqual(dependencies, feature->dependencies(), name);
  ExpectVectorsEqual(extension_types, feature->extension_types(), name);
  ExpectOptionalVectorsEqual(contexts, feature->contexts(), name);
  ExpectVectorsEqual(platforms, feature->platforms(), name);
  EXPECT_EQ(matches, feature->matches()) << name;
  EXPECT_EQ(location, feature->location()) << name;
  EXPECT_EQ(min_manifest_version, feature->min_manifest_version()) << name;
  EXPECT_EQ(max_manifest_version, feature->max_manifest_version()) << name;
  EXPECT_EQ(component_extensions_auto_granted,
            feature->component_extensions_auto_granted())
      << name;
  EXPECT_EQ(command_line_switch, feature->command_line_switch()) << name;
  EXPECT_EQ(channel, feature->channel()) << name;
  EXPECT_EQ(internal, feature->IsInternal()) << name;
  EXPECT_EQ(alias, feature->alias()) << name;
  EXPECT_EQ(source, feature->source()) << name;
  EXPECT_EQ(requires_delegated_availability_check,
            feature->RequiresDelegatedAvailabilityCheck())
      << name;
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
    comparator.extension_types = {Manifest::TYPE_EXTENSION,
                                  Manifest::TYPE_PLATFORM_APP};
    comparator.location = SimpleFeature::COMPONENT_LOCATION;
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
    comparator.extension_types = {Manifest::TYPE_EXTENSION};
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
    comparator.CompareFeature(feature);
  }
  {
    const ComplexFeature* complex_feature =
        GetAsComplexFeature("gamma.complex_unparented");
    FeatureComparator comparator("gamma.complex_unparented");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kUnprivilegedExtension});
    comparator.channel = version_info::Channel::STABLE;
    // We cheat and have both children exactly the same for ease of comparing;
    // complex features are tested more thoroughly below.
    for (const auto& feature : complex_feature->features_)
      comparator.CompareFeature(static_cast<SimpleFeature*>(feature.get()));
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("delta");
    FeatureComparator comparator("delta");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension, mojom::ContextType::kWebUi});
    comparator.channel = version_info::Channel::DEV;
    comparator.matches.AddPattern(
        URLPattern(URLPattern::SCHEME_ALL, "*://example.com/*"));
    comparator.min_manifest_version = 2;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("pi");
    FeatureComparator comparator("pi");
    comparator.contexts =
        std::vector<mojom::ContextType>({mojom::ContextType::kUntrustedWebUi});
    comparator.channel = version_info::Channel::STABLE;
    comparator.matches.AddPattern(
        URLPattern(URLPattern::SCHEME_ALL, "chrome-untrusted://foo/*"));
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("allEnum");
    FeatureComparator comparator("allEnum");
    comparator.contexts = std::vector<mojom::ContextType>(
        {mojom::ContextType::kPrivilegedExtension,
         mojom::ContextType::kPrivilegedWebPage,
         mojom::ContextType::kContentScript,
         mojom::ContextType::kLockscreenExtension,
         mojom::ContextType::kOffscreenExtension,
         mojom::ContextType::kUserScript, mojom::ContextType::kWebPage,
         mojom::ContextType::kWebUi, mojom::ContextType::kUntrustedWebUi,
         mojom::ContextType::kUnprivilegedExtension});
    comparator.extension_types = {Manifest::TYPE_EXTENSION,
                                  Manifest::TYPE_HOSTED_APP,
                                  Manifest::TYPE_LEGACY_PACKAGED_APP,
                                  Manifest::TYPE_PLATFORM_APP,
                                  Manifest::TYPE_SHARED_MODULE,
                                  Manifest::TYPE_THEME,
                                  Manifest::TYPE_LOGIN_SCREEN_EXTENSION,
                                  Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION};
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
    EXPECT_EQ(2u, feature->features_.size());
    // Find the default parent. This is a little tedious because it might not
    // be guaranteed that the default_parent is in a specific index, but it
    // specifies channel as 'stable'.
    const SimpleFeature* default_parent = nullptr;
    const SimpleFeature* other_parent = nullptr;
    {
      const SimpleFeature* parent1 =
          static_cast<SimpleFeature*>(feature->features_[0].get());
      const SimpleFeature* parent2 =
          static_cast<SimpleFeature*>(feature->features_[1].get());
      if (parent1->channel() == version_info::Channel::STABLE) {
        default_parent = parent1;
        other_parent = parent2;
      } else {
        other_parent = parent1;
        default_parent = parent2;
      }
    }
    {
      // Check the default parent.
      FeatureComparator comparator("complex");
      comparator.channel = version_info::Channel::STABLE;
      comparator.contexts = std::vector<mojom::ContextType>(
          {mojom::ContextType::kPrivilegedExtension});
      comparator.extension_types = {Manifest::TYPE_EXTENSION};
      comparator.CompareFeature(default_parent);
      // Check the child of the complex feature. It should inherit its
      // properties from the default parent.
      const SimpleFeature* child_feature = GetAsSimpleFeature("complex.child");
      comparator.name = "complex.child";
      comparator.platforms = {Feature::WIN_PLATFORM};
      comparator.dependencies = {"permission:complex.child"};
      comparator.CompareFeature(child_feature);
    }
    {
      // Finally, check the branch of the complex feature.
      FeatureComparator comparator("complex");
      comparator.channel = version_info::Channel::BETA;
      comparator.contexts = std::vector<mojom::ContextType>(
          {mojom::ContextType::kPrivilegedExtension});
      comparator.extension_types = {Manifest::TYPE_EXTENSION};
      comparator.allowlist = {"0123456789ABCDEF0123456789ABCDEF01234567"};
      comparator.CompareFeature(other_parent);
    }
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
