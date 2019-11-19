// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
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

const bool kDefaultAutoGrant = true;
const bool kDefaultInternal = false;

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
  std::vector<Feature::Context> contexts;
  std::vector<Feature::Platform> platforms;

  URLPatternSet matches;

  base::Optional<SimpleFeature::Location> location;
  base::Optional<int> min_manifest_version;
  base::Optional<int> max_manifest_version;
  base::Optional<std::string> command_line_switch;
  base::Optional<version_info::Channel> channel;

  std::string alias;
  std::string source;

  bool component_extensions_auto_granted;
  bool internal;
};

FeatureComparator::FeatureComparator(const std::string& name)
    : name(name),
      component_extensions_auto_granted(kDefaultAutoGrant),
      internal(kDefaultInternal) {}

FeatureComparator::~FeatureComparator() = default;

void FeatureComparator::CompareFeature(const SimpleFeature* feature) {
  ASSERT_TRUE(feature);
  EXPECT_EQ(name, feature->name());
  ExpectVectorsEqual(blocklist, feature->blocklist(), name);
  ExpectVectorsEqual(allowlist, feature->allowlist(), name);
  ExpectVectorsEqual(dependencies, feature->dependencies(), name);
  ExpectVectorsEqual(extension_types, feature->extension_types(), name);
  ExpectVectorsEqual(contexts, feature->contexts(), name);
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
}

TEST(FeaturesGenerationTest, FeaturesTest) {
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
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
    comparator.channel = version_info::Channel::STABLE;
    comparator.max_manifest_version = 1;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("beta");
    FeatureComparator comparator("beta");
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
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
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
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
    comparator.contexts = {Feature::UNBLESSED_EXTENSION_CONTEXT};
    comparator.channel = version_info::Channel::DEV;
    comparator.CompareFeature(feature);
  }
  {
    const ComplexFeature* complex_feature =
        GetAsComplexFeature("gamma.complex_unparented");
    FeatureComparator comparator("gamma.complex_unparented");
    comparator.contexts = {Feature::UNBLESSED_EXTENSION_CONTEXT};
    comparator.channel = version_info::Channel::STABLE;
    // We cheat and have both children exactly the same for ease of comparing;
    // complex features are tested more thoroughly below.
    for (const auto& feature : complex_feature->features_)
      comparator.CompareFeature(static_cast<SimpleFeature*>(feature.get()));
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("delta");
    FeatureComparator comparator("delta");
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT,
                           Feature::WEBUI_CONTEXT};
    comparator.channel = version_info::Channel::DEV;
    comparator.matches.AddPattern(
        URLPattern(URLPattern::SCHEME_ALL, "*://example.com/*"));
    comparator.min_manifest_version = 2;
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("allEnum");
    FeatureComparator comparator("allEnum");
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT,
                           Feature::BLESSED_WEB_PAGE_CONTEXT,
                           Feature::CONTENT_SCRIPT_CONTEXT,
                           Feature::LOCK_SCREEN_EXTENSION_CONTEXT,
                           Feature::WEB_PAGE_CONTEXT,
                           Feature::WEBUI_CONTEXT,
                           Feature::UNBLESSED_EXTENSION_CONTEXT};
    comparator.extension_types = {Manifest::TYPE_EXTENSION,
                                  Manifest::TYPE_HOSTED_APP,
                                  Manifest::TYPE_LEGACY_PACKAGED_APP,
                                  Manifest::TYPE_PLATFORM_APP,
                                  Manifest::TYPE_SHARED_MODULE,
                                  Manifest::TYPE_THEME,
                                  Manifest::TYPE_LOGIN_SCREEN_EXTENSION};
    comparator.channel = version_info::Channel::BETA;
    comparator.CompareFeature(feature);
  }
  {
    // Omega is imported from a second .json file.
    const SimpleFeature* feature = GetAsSimpleFeature("omega");
    FeatureComparator comparator("omega");
    comparator.contexts = {Feature::WEB_PAGE_CONTEXT};
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
      comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
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
      comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
      comparator.extension_types = {Manifest::TYPE_EXTENSION};
      comparator.allowlist = {"0123456789ABCDEF0123456789ABCDEF01234567"};
      comparator.CompareFeature(other_parent);
    }
  }

  // Test API aliases.
  {
    const SimpleFeature* feature = GetAsSimpleFeature("alias");
    FeatureComparator comparator("alias");
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
    comparator.channel = version_info::Channel::STABLE;
    comparator.source = "alias_source";
    comparator.CompareFeature(feature);
  }
  {
    const SimpleFeature* feature = GetAsSimpleFeature("alias_source");
    FeatureComparator comparator("alias_source");
    comparator.contexts = {Feature::BLESSED_EXTENSION_CONTEXT};
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
}

}  // namespace extensions
