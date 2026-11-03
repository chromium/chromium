# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


_AI_AGENT_ENV_VARS = (
  'GEMINI_CLI',
  'CLAUDECODE',
  'ANTIGRAVITY_AGENT',
  'CODEX_SANDBOX',
  'CURSOR_AGENT',
  'COPILOT_CLI',
  'AI_AGENT',
)


def _IsEnvVarEnabled(name: str) -> bool:
  return (os.getenv(name) or '').lower() not in ('', '0', 'false')


def IsGeminiCli():
  return _IsEnvVarEnabled('GEMINI_CLI')


def IsAntigravity():
  return _IsEnvVarEnabled('ANTIGRAVITY_AGENT')


def IsLlm():
  return any(_IsEnvVarEnabled(var) for var in _AI_AGENT_ENV_VARS)
