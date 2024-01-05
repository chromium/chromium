// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config::BuildConfig;
use crate::group::Group;
use anyhow::Result;

fn group_vet_criteria(group: Group, shipped: Option<bool>) -> Vec<String> {
    match (shipped, group) {
        // Safe crates can be used on adversarial inputs without a sandbox. They can not cause
        // security bugs in this case, and must satisfy the Rule of Two.
        //
        // "rule-of-two-safe-to-deploy" destructures into a combination of
        // * "safe-to-deploy", and
        // * "ub-risk-1".
        //
        // We currently consider ub-risk-2 as satisfying the Rule of Two, though there seems to be
        // some spot in between risk 1 and 2 that fits better and this could be improved.
        (Some(true), Group::Safe) | (None, Group::Safe) => vec!["safe-to-deploy", "ub-risk-2"],
        // Sandbox crates are used in a sandbox, so we have a weaker tolerance. There may be a bunch
        // of ASM code in there for example. Adversarial inputs may have a way to break things,
        // though we certainly try to avoid it.
        //
        // This type of crate is not well described in the UB risk guidelines for now, so we use
        // "ub-risk-3" for this category.
        (Some(true), Group::Sandbox) | (None, Group::Sandbox) => {
            vec!["safe-to-deploy", "ub-risk-3"]
        }
        // Code in tests is not run on user machines and does not interact with adversarial inputs.
        // Thus it does not need to be safe-to-deploy, but it needs to not be malicious against
        // developers and CI bots which is covered by "safe-to-run".
        (_, Group::Test) => vec!["safe-to-run"],
        // Crates that contribute to the shipped binary but are not themselves shipped (code
        // generators for example) do not get deployed themselves and do not interact with
        // adversarial inputs. Thus they need to be "safe-to-run" by developers and CI only.
        (Some(false), _) => vec!["safe-to-run"],
    }
    .into_iter()
    .map(String::from)
    .collect()
}

#[derive(serde::Serialize)]
pub struct VetConfigToml {
    policies: Vec<Policy>,
}
#[derive(serde::Serialize)]
pub struct Policy {
    crate_name: String,
    criteria: Vec<String>,
}

/// Generate the config.toml for `cargo vet` with policies that match the groups
/// specified for each crate through gnrt_config.toml.
pub fn create_vet_config<'a>(
    packages: impl IntoIterator<Item = &'a cargo_metadata::Package>,
    config: &BuildConfig,
    mut find_group: impl FnMut(&'a cargo_metadata::PackageId) -> Result<Group>,
    mut find_shipped: impl FnMut(&'a cargo_metadata::PackageId) -> Result<Option<bool>>,
) -> Result<VetConfigToml> {
    let mut vet_config_toml = VetConfigToml { policies: Vec::new() };
    for package in packages {
        let group = find_group(&package.id)?;
        let shipped = find_shipped(&package.id)?;

        let mut crate_name = package.name.clone();
        crate_name.push(':');
        crate_name.push_str(&package.version.to_string());

        let criteria = if config.resolve.remove_crates.contains(&package.name) {
            vec![]
        } else {
            group_vet_criteria(group, shipped)
        };

        vet_config_toml.policies.push(Policy { crate_name, criteria });
    }

    vet_config_toml.policies.sort_unstable_by(|a, b| a.crate_name.cmp(&b.crate_name));
    Ok(vet_config_toml)
}
