# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def IsGeminiCli():
  return os.getenv("GEMINI_CLI") not in (None, '', '0')


def IsAntigravity():
  return os.getenv("ANTIGRAVITY_AGENT") not in (None, '', '0')


def IsLlm():
  return IsGeminiCli() or IsAntigravity()
