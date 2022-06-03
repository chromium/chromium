# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCENARIOS = [{
    "name": "chromium_navigation",
    "driver_script": "chromium_navigation",
    "browser": "Chromium",
    "extra_args": [],
    "skip": False
}, {
    "name": "idle",
    "driver_script": "idle",
    "browser": None,
    "extra_args": [],
    "skip": True
}, {
    "name": "canary_idle_on_youtube_slack",
    "driver_script": "canary_idle_on_youtube",
    "browser": "Canary",
    "extra_args": ["--enable-features=LudicrousTimerSlack"],
    "skip": True
}, {
    "name": "canary_idle_on_youtube_noslack",
    "driver_script": "canary_idle_on_youtube",
    "browser": "Canary",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_idle_on_youtube",
    "driver_script": "safari_idle_on_youtube",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "canary_idle_on_wiki_slack",
    "driver_script": "canary_idle_on_wiki",
    "browser": "Canary",
    "extra_args": ["--enable-features=LudicrousTimerSlack"],
    "skip": True
}, {
    "name": "canary_idle_on_wiki_noslack",
    "driver_script": "canary_idle_on_wiki",
    "browser": "Canary",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_navigation",
    "driver_script": "chrome_navigation",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_navigation",
    "driver_script": "safari_navigation",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_idle_on_wiki",
    "driver_script": "chrome_idle_on_wiki",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_idle_on_wiki",
    "driver_script": "safari_idle_on_wiki",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_idle_on_wiki_hidden",
    "driver_script": "chrome_idle_on_wiki_hidden",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_idle_on_wiki_hidden",
    "driver_script": "safari_idle_on_wiki_hidden",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_idle_on_youtube",
    "driver_script": "chrome_idle_on_youtube",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_idle_on_youtube",
    "driver_script": "safari_idle_on_youtube",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_zero_window",
    "driver_script": "chrome_zero_window",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_zero_window",
    "driver_script": "safari_zero_window",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
}, {
    "name": "chrome_meet",
    "driver_script": "chrome_meet",
    "browser": "Chrome",
    "extra_args": [],
    "skip": True
}, {
    "name": "safari_meet",
    "driver_script": "safari_meet",
    "browser": "Safari",
    "extra_args": [],
    "skip": True
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


def get_browser_property(browser, property_name):
  return BROWSERS_DEFINITION[browser][property_name]


def get_browser_process_names():
  return BROWSERS_DEFINITION.keys()
