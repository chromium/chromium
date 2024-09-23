// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config::BuildConfig;
use crate::group::Group;
use anyhow::Result;

fn group_vet_criteria(group: Group, shipped: Option<bool>) -> Vec<AuditCriteria> {
    use AuditCriteria::*;
    // No third-party crates from crates.io are allowed to implement crypto today.
    // We only accept crypto implementation from BoringSSL, which we bring in
    // through DEPS. If this should ever change we will need to add a
    // configuration option in gnrt_config.toml to allow a crate to
    // implement crypto.
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
        (Some(true), Group::Safe) | (None, Group::Safe) => {
            vec![CryptoSafe, SafeToDeploy, UbRisk2]
        }
        // Sandbox crates are used in a sandbox, so we have a weaker tolerance. There may be a bunch
        // of ASM code in there for example. Adversarial inputs may have a way to break things,
        // though we certainly try to avoid it.
        //
        // This type of crate is not well described in the UB risk guidelines for now, so we use
        // "ub-risk-3" for this category.
        (Some(true), Group::Sandbox) | (None, Group::Sandbox) => {
            vec![CryptoSafe, SafeToDeploy, UbRisk3]
        }
        // Code in tests is not run on user machines and does not interact with adversarial inputs.
        // Thus it does not need to be safe-to-deploy, but it needs to not be malicious against
        // developers and CI bots which is covered by "safe-to-run".
        (_, Group::Test) => vec![CryptoSafe, SafeToRun],
        // Crates that contribute to the shipped binary but are not themselves shipped (code
        // generators for example) do not get deployed themselves and do not interact with
        // adversarial inputs. Thus they need to be "safe-to-run" by developers and CI only.
        (Some(false), _) => vec![CryptoSafe, SafeToRun],
    }
}

#[derive(serde::Serialize)]
pub struct VetConfigToml {
    policies: Vec<Policy>,
}

#[derive(serde::Serialize)]
pub struct Policy {
    crate_name: String,
    criteria: Vec<AuditCriteria>,
}

/// Audit criteria used by Chromium for `cargo vet` audits.  This enum
/// represents and replicates the criteria that can be found in
/// https://github.com/google/rust-crate-audits/blob/main/audits.toml (e.g. `UbRisk2` corresponds
/// to the `[criteria.ub-risk-2]` entry in that `audits.toml` file.
/// Corresponding auditing standards are described in
/// https://github.com/google/rust-crate-audits/blob/main/auditing_standards.md
#[derive(serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum AuditCriteria {
    CryptoSafe,
    SafeToDeploy,
    SafeToRun,
    #[serde(rename = "ub-risk-2")]
    UbRisk2,
    #[serde(rename = "ub-risk-3")]
    UbRisk3,
}

/// Generate the config.toml for `cargo vet` with policies that match the groups
/// specified for each crate through gnrt_config.toml.
pub fn create_vet_config<'a>(
    packages: impl IntoIterator<Item = &'a cargo_metadata::Package>,
    config: &BuildConfig,
    mut find_group: impl FnMut(&'a cargo_metadata::PackageId) -> Group,
    mut find_shipped: impl FnMut(&'a cargo_metadata::PackageId) -> Option<bool>,
) -> Result<VetConfigToml> {
    let mut vet_config_toml = VetConfigToml { policies: Vec::new() };
    for package in packages {
        let group = find_group(&package.id);
        let shipped = find_shipped(&package.id);

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
