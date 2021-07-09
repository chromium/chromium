# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCENARIOS = [{
    "name": "idle"
}, {
    "name": "canary_idle_on_youtube_slack",
    "browser": "Canary"
}, {
    "name": "canary_idle_on_youtube_noslack",
    "browser": "Canary"
}, {
    "name": "safari_idle_on_youtube",
    "browser": "Safari"
}, {
    "name": "canary_idle_on_wiki_slack",
    "browser": "Canary"
}, {
    "name": "canary_idle_on_wiki_noslack",
    "browser": "Canary"
}, {
    "name": "chrome_navigation",
    "browser": "Chrome"
}, {
    "name": "safari_navigation",
    "browser": "Safari"
}, {
    "name": "chrome_idle_on_wiki",
    "browser": "Chrome"
}, {
    "name": "safari_idle_on_wiki",
    "browser": "Safari"
}, {
    "name": "chrome_idle_on_wiki_hidden",
    "browser": "Chrome"
}, {
    "name": "safari_idle_on_wiki_hidden",
    "browser": "Safari"
}, {
    "name": "chrome_idle_on_youtube",
    "browser": "Chrome"
}, {
    "name": "safari_idle_on_youtube",
    "browser": "Safari"
}, {
    "name": "chrome_zero_window",
    "browser": "Chrome"
}, {
    "name": "safari_zero_window",
    "browser": "Safari"
}]

BROWSERS_DEFINITION = {
    "Chrome": {
        "executable": "Google Chrome",
        "process_name": "Google Chrome",
        "identifier": "com.google.Chrome"
    },
    "Canary": {
        "executable": "Google Chrome Canary",
        "process_name": "Google Chrome Canary",
        "identifier": "com.google.Chrome.canary"
    },
    "Chromium": {
        "process_name": "Chromium",
        "executable": "Chromium",
        "identifier": "org.chromium.Chromium"
    },
    "Edge": {
        "executable": "Microsoft Edge",
        "process_name": "Microsoft Edge",
        "identifier": "com.microsoft.edgemac"
    },
    "Safari": {
        "executable": "Safari",
        "process_name": "Safari",
        "identifier": "com.apple.Safari"
    }
}
