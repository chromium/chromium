// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use gnrt_lib::manifest::*;

#[gtest(ManifestTest, ParseSingleFullDependency)]
fn test() {
    expect_eq!(
        toml::de::from_str(concat!(
            "version = \"1.0.0\"\n",
            "features = [\"foo\", \"bar\"]\n",
            "allow-first-party-usage = false\n",
            "build-script-outputs = [\"stuff.rs\"]\n",
            "gn-variables-lib = \"\"\"
            deps = []
            configs = []
            \"\"\""
        )),
        Ok(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("1.0.0".to_string())),
            features: vec!["foo".to_string(), "bar".to_string()],
            allow_first_party_usage: false,
            build_script_outputs: vec!["stuff.rs".to_string()],
            gn_variables_lib: Some(
                "            deps = []\n            configs = []\n            ".to_string()
            )
        })
    );

    expect_eq!(
        toml::de::from_str(concat!(
            "version = \"3.14.159\"\n",
            "build-script-outputs = [\"generated.rs\"]\n",
        )),
        Ok(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("3.14.159".to_string())),
            features: vec![],
            allow_first_party_usage: true,
            build_script_outputs: vec!["generated.rs".to_string()],
            gn_variables_lib: None,
        })
    );
}

#[gtest(ManifestTest, NoDefaultFeatures)]
fn test() {
    expect_eq!(
        toml::de::from_str(concat!(
            "default-features = false\n",
            "version = \"1.0.0\"\n",
            "features = [\"foo\", \"bar\"]\n",
            "allow-first-party-usage = false\n",
            "build-script-outputs = [\"stuff.rs\"]\n",
            "gn-variables-lib = \"\"\"
            deps = []
            configs = []
            \"\"\""
        )),
        Ok(FullDependency {
            default_features: false,
            version: Some(VersionConstraint("1.0.0".to_string())),
            features: vec!["foo".to_string(), "bar".to_string()],
            allow_first_party_usage: false,
            build_script_outputs: vec!["stuff.rs".to_string()],
            gn_variables_lib: Some(
                "            deps = []\n            configs = []\n            ".to_string()
            )
        })
    );
}

#[gtest(ManifestTest, ParseManifest)]
fn test() {
    let manifest: ThirdPartyManifest = toml::de::from_str(concat!(
        "[dependencies]\n",
        "cxx = \"1\"\n",
        "serde = \"1\"\n",
        "rustversion = {version = \"1\", build-script-outputs = [\"version.rs\"]}",
        "\n",
        "[dependencies.unicode-linebreak]\n",
        "version = \"0.1\"\n",
        "allow-first-party-usage = false\n",
        "build-script-outputs = [ \"table.rs\" ]\n",
        "\n",
        "[dependencies.special-stuff]\n",
        "version = \"0.1\"\n",
        "gn-variables-lib = \"hello = \\\"world\\\"\"\n",
        "\n",
        "[testonly-dependencies]\n",
        "syn = {version = \"1\", features = [\"full\"]}\n",
    ))
    .unwrap();

    expect_eq!(
        manifest.dependencies.get("cxx"),
        Some(&Dependency::Short(VersionConstraint("1".to_string())))
    );
    expect_eq!(
        manifest.dependencies.get("serde"),
        Some(&Dependency::Short(VersionConstraint("1".to_string())))
    );

    expect_eq!(
        manifest.dependencies.get("rustversion"),
        Some(&Dependency::Full(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("1".to_string())),
            features: vec![],
            allow_first_party_usage: true,
            build_script_outputs: vec!["version.rs".to_string()],
            gn_variables_lib: None,
        }))
    );

    expect_eq!(
        manifest.dependencies.get("unicode-linebreak"),
        Some(&Dependency::Full(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("0.1".to_string())),
            features: vec![],
            allow_first_party_usage: false,
            build_script_outputs: vec!["table.rs".to_string()],
            gn_variables_lib: None,
        }))
    );

    expect_eq!(
        manifest.dependencies.get("special-stuff"),
        Some(&Dependency::Full(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("0.1".to_string())),
            features: vec![],
            allow_first_party_usage: true,
            build_script_outputs: vec![],
            gn_variables_lib: Some("hello = \"world\"".to_string()),
        }))
    );

    expect_eq!(
        manifest.testonly_dependencies.get("syn"),
        Some(&Dependency::Full(FullDependency {
            default_features: true,
            version: Some(VersionConstraint("1".to_string())),
            features: vec!["full".to_string()],
            allow_first_party_usage: true,
            build_script_outputs: vec![],
            gn_variables_lib: None,
        }))
    );
}

#[gtest(ManifestTest, SerializeManifestWithPatches)]
fn test() {
    let manifest = CargoManifest {
        package: CargoPackage {
            name: "chromium".to_string(),
            version: Version::new(0, 1, 0),
            authors: Vec::new(),
            edition: Edition("2021".to_string()),
            description: None,
            license: "funtimes".to_string(),
        },
        workspace: None,
        dependencies: DependencySet::new(),
        patches: vec![(
            "crates-io".to_string(),
            vec![(
                "foo_v1".to_string(),
                CargoPatch {
                    path: "third_party/rust/foo/v1/crate".to_string(),
                    package: "foo".to_string(),
                },
            )]
            .into_iter()
            .collect(),
        )]
        .into_iter()
        .collect(),
    };

    expect_eq!(
        toml::to_string(&manifest).unwrap(),
        "[package]
name = \"chromium\"
version = \"0.1.0\"
edition = \"2021\"
license = \"funtimes\"
[patch.crates-io.foo_v1]
path = \"third_party/rust/foo/v1/crate\"
package = \"foo\"
"
    )
}

#[gtest(ManifestTest, PackageManifest)]
fn test() {
    let manifest: CargoManifest = toml::de::from_str(concat!(
        "[package]
name = \"foo\"
version = \"1.2.3\"
authors = [\"alice@foo.com\", \"bob@foo.com\"]
edition = \"2021\"
description = \"A library to foo the bars\"
license = \"funtimes\"
"
    ))
    .unwrap();

    expect_eq!(
        manifest.package,
        CargoPackage {
            name: "foo".to_string(),
            version: Version::new(1, 2, 3),
            authors: vec!["alice@foo.com".to_string(), "bob@foo.com".to_string()],
            edition: Edition("2021".to_string()),
            description: Some("A library to foo the bars".to_string()),
            license: "funtimes".to_string(),
        }
    )
}
