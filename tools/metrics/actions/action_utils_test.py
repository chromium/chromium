# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import dataclasses
import re

import action_utils


@dataclasses.dataclass(frozen=True)
class _TestScenario:
  name: str
  action_names: list[str]
  tokens_dict: dict[str, list[str]]
  expected_outputs: list[str]

  def _ExtractTokens(self, action_name) -> list[action_utils.Token]:
    token_pattern = r"\{(.*?)\}"
    token_names = re.findall(token_pattern, action_name)
    tokens = [action_utils.Token(key=token_name) for token_name in token_names]
    for token in tokens:
      # Using TypeOf prefix to avoid confusion between type and name of token
      variants_name = f"TypeOf{token.key}"
      if variants_name not in self.tokens_dict:
        continue
      token.variants = [
          action_utils.Variant(name=variant, summary="")
          for variant in self.tokens_dict[variants_name]
      ]
      token.variants_name = variants_name
    return tokens

  def _AllVariants(self, token_name):
    return [
        action_utils.Variant(name=variant, summary="")
        for variant in self.tokens_dict[token_name]
    ]

  def _PrepareActionsDict(self) -> dict[str, action_utils.Action]:
    actions_dict = {
        action_name:
        action_utils.Action(action_name,
                            "dummy description", ["dummy@owner.com"],
                            tokens=self._ExtractTokens(action_name))
        for action_name in self.action_names
    }
    return action_utils.CreateActionsFromVariants(actions_dict)


class ActionXmlTest(unittest.TestCase):

  def _RunSubTest(self, name, test_func):
    with self.subTest(name):
      test_func()

  def testVariantsExpansion(self):
    scenarios = [
        _TestScenario(name="Single token present",
                      action_names=["TestAction.{Token}"],
                      tokens_dict={"TypeOfToken": ["A", "B"]},
                      expected_outputs=["TestAction.A", "TestAction.B"]),
        _TestScenario(name="Variant for token missing",
                      action_names=["TestAction.{Token}"],
                      tokens_dict={},
                      expected_outputs=[]),
        # TODO(afie): Multiple token present
        # TODO(afie): Multiple token present variant missing for some
        # TODO(afie): Entry with no token return directly
        # TODO(afie): Complex scenario with combined elements
    ]

    def testFunction(test_scenario):
      actions_dict = test_scenario._PrepareActionsDict()
      self.assertEqual(len(actions_dict), len(test_scenario.expected_outputs))
      for expected_action_name in test_scenario.expected_outputs:
        self.assertTrue(expected_action_name in actions_dict)

        # Validate that Action object was copied to the derived action.
        self.assertEqual(actions_dict[expected_action_name].name,
                         expected_action_name)

    for scenario in scenarios:
      self._RunSubTest(scenario.name, lambda: testFunction(scenario))


if __name__ == '__main__':
  unittest.main()
