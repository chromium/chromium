-- Copyright 2026 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.loadline_2;

SELECT page, story_start, visual_presentation, interactive_presentation
FROM chrome_loadline2_stages;
