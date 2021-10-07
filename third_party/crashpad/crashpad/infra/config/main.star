#!/usr/bin/env lucicfg
# Copyright 2021 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

lucicfg.check_version("1.28.0", "Please update depot_tools")

# Enable LUCI Realms support and Launch 100% of Swarming tasks for builds in
# "realms-aware mode".
lucicfg.enable_experiment("crbug.com/1085650")
luci.builder.defaults.experiments.set({"luci.use_realms": 100})

REPO_URL = "https://chromium.googlesource.com/crashpad/crashpad"
REVIEW_URL = "https://chromium-review.googlesource.com/crashpad/crashpad"

luci.project(
    name = "crashpad",
    buildbucket = "cr-buildbucket.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
                acl.BUILDBUCKET_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-crashpad-admins",
        ),
    ],
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
)

luci.cq(
    status_host = "chromium-cq-status.appspot.com",
    submit_max_burst = 4,
    submit_burst_delay = 8 * time.minute,
)

luci.cq_group(
    name = "crashpad",
    watch = cq.refset(repo = REVIEW_URL, refs = ["refs/heads/.+"]),
    retry_config = cq.retry_config(
        single_quota = 1,
        global_quota = 2,
        failure_weight = 1,
        transient_failure_weight = 1,
        timeout_weight = 2,
    ),
    acls = [
        acl.entry(
            roles = acl.CQ_COMMITTER,
            groups = "project-crashpad-tryjob-access",
        ),
        acl.entry(
            roles = acl.CQ_DRY_RUNNER,
            groups = "project-crashpad-tryjob-access",
        ),
    ],
)

luci.gitiles_poller(
    name = "master-gitiles-trigger",
    bucket = "ci",
    repo = REPO_URL,
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/crashpad-logo.svg",
)

luci.console_view(
    name = "main",
    repo = REPO_URL,
    title = "Crashpad Main Console",
)

luci.list_view(
    name = "try",
    title = "Crashpad Try Builders",
)

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            acl.BUILDBUCKET_OWNER,
            groups = "project-crashpad-admins",
        ),
        acl.entry(
            acl.BUILDBUCKET_TRIGGERER,
            users = "luci-scheduler@appspot.gserviceaccount.com",
        ),
    ],
)

luci.bucket(
    name = "try",
    acls = [
        acl.entry(
            acl.BUILDBUCKET_OWNER,
            groups = [
                "service-account-crashpad-cq",
                "project-crashpad-admins",
            ],
        ),
        acl.entry(
            acl.BUILDBUCKET_TRIGGERER,
            groups = "service-account-cq",
        ),
        acl.entry(
            acl.BUILDBUCKET_TRIGGERER,
            groups = "project-crashpad-tryjob-access",
        ),
    ],
)

def crashpad_recipe():
    return luci.recipe(
        name = "crashpad/build",
        cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
    )

def crashpad_caches(platform):
    if platform == "ios":
        return [swarming.cache("osx_sdk", name = "osx_sdk_ios")]
    elif platform == "mac":
        return [swarming.cache("osx_sdk", name = "osx_sdk_mac")]

def crashpad_dimensions(platform, bucket):
    dimensions = {}
    dimensions["cpu"] = "x86-64"
    dimensions["pool"] = "luci.flex." + bucket

    if platform == "fuchsia":
        dimensions["os"] = "Ubuntu-16.04"
    elif platform == "ios":
        dimensions["os"] = "Mac-10.15"
    elif platform == "linux":
        dimensions["os"] = "Ubuntu-16.04"
    elif platform == "mac":
        dimensions["os"] = "Mac-10.15"
    elif platform == "win":
        dimensions["os"] = "Windows-10"

    if platform == "fuchsia" or platform == "linux" or platform == "win":
        dimensions["cores"] = "8"

    return dimensions

def crashpad_properties(platform, cpu, config, bucket):
    properties = {}
    properties["target_os"] = platform
    properties["$kitchen"] = {
        "devshell": True,
        "git_auth": True,
    }

    if cpu != "x64":
        properties["target_cpu"] = cpu

    if bucket == "ci":
        properties["$gatekeeper"] = {
            "group": "client.crashpad",
        }

    if config == "dbg":
        properties["config"] = "Debug"
    elif config == "rel":
        properties["config"] = "Release"

    return properties

def crashpad_builder(platform, cpu, config, bucket):
    name = "_".join(["crashpad", platform, cpu, config])
    triggered_by = None

    if bucket == "ci":
        luci.console_view_entry(
            builder = "ci/" + name,
            console_view = "main",
            short_name = config,
            category = platform + "|" + cpu,
        )
        triggered_by = ["master-gitiles-trigger"]
    elif bucket == "try":
        luci.list_view_entry(
            builder = "try/" + name,
            list_view = "try",
        )
        luci.cq_tryjob_verifier(
            "try/" + name,
            cq_group = "crashpad",
        )

    return luci.builder(
        name = name,
        bucket = bucket,
        executable = crashpad_recipe(),
        build_numbers = True,
        caches = crashpad_caches(platform),
        dimensions = crashpad_dimensions(platform, bucket),
        execution_timeout = 3 * time.hour,
        properties = crashpad_properties(platform, cpu, config, bucket),
        service_account = "crashpad-" + bucket + "-builder@chops-service-accounts.iam.gserviceaccount.com",
        triggered_by = triggered_by,
    )

crashpad_builder("fuchsia", "arm64", "dbg", "ci")
crashpad_builder("fuchsia", "arm64", "rel", "ci")
crashpad_builder("fuchsia", "x64", "dbg", "ci")
crashpad_builder("fuchsia", "x64", "rel", "ci")
crashpad_builder("ios", "arm64", "dbg", "ci")
crashpad_builder("ios", "arm64", "rel", "ci")
crashpad_builder("ios", "x64", "dbg", "ci")
crashpad_builder("ios", "x64", "rel", "ci")
crashpad_builder("linux", "x64", "dbg", "ci")
crashpad_builder("linux", "x64", "rel", "ci")
crashpad_builder("mac", "x64", "dbg", "ci")
crashpad_builder("mac", "x64", "rel", "ci")
crashpad_builder("win", "x64", "dbg", "ci")
crashpad_builder("win", "x64", "rel", "ci")

crashpad_builder("fuchsia", "arm64", "dbg", "try")
crashpad_builder("fuchsia", "arm64", "rel", "try")
crashpad_builder("fuchsia", "x64", "dbg", "try")
crashpad_builder("fuchsia", "x64", "rel", "try")
crashpad_builder("ios", "arm64", "dbg", "try")
crashpad_builder("ios", "arm64", "rel", "try")
crashpad_builder("ios", "x64", "dbg", "try")
crashpad_builder("ios", "x64", "rel", "try")
crashpad_builder("linux", "x64", "dbg", "try")
crashpad_builder("linux", "x64", "rel", "try")
crashpad_builder("mac", "x64", "dbg", "try")
crashpad_builder("mac", "x64", "rel", "try")
crashpad_builder("win", "x64", "dbg", "try")
crashpad_builder("win", "x64", "rel", "try")
