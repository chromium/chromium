# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import dataclasses
import re

import setup_modules

import chromium_src.tools.metrics.actions.action_utils as action_utils


@dataclasses.dataclass(frozen=True)
class _TestScenario:
  name: str
  action_names: list[str]
  tokens_dict: dict[str, list[str]]
  expected_outputs: list[str] | None

  def _ExtractTokens(self, action_name) -> list[action_utils.Token]:
    token_pattern = r"\{(.*?)\}"
    token_names = re.findall(token_pattern, action_name)
    tokens = [action_utils.Token(key=token_name) for token_name in token_names]
    for token in tokens:
      # Using TypeOf prefix to avoid confusion between type and name of token
      variants_name = f"TypeOf{token.key}"
      if variants_name in self.tokens_dict:
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
                      expected_outputs=None),
        _TestScenario(name="Multiple tokens present",
                      action_names=["TestAction.{Token}.{OtherToken}"],
                      tokens_dict={
                          "TypeOfToken": ["A", "B"],
                          "TypeOfOtherToken": ["1", "2"]
                      },
                      expected_outputs=[
                          "TestAction.A.1", "TestAction.B.1", "TestAction.A.2",
                          "TestAction.B.2"
                      ]),
        _TestScenario(name="Multiple tokens present variant missing for some",
                      action_names=["TestAction.{Token}.{OtherToken}"],
                      tokens_dict={"TypeOfToken": ["A", "B"]},
                      expected_outputs=None),
        _TestScenario(name="Entry with no token return directly",
                      action_names=["TestAction.NoTokens"],
                      tokens_dict={},
                      expected_outputs=["TestAction.NoTokens"]),
        _TestScenario(
            name="Multiple actions handled with some tokens overlaps",
            action_names=[
                "TestAction1.{Token}.{OtherToken}", "TestAction2.NoTokens",
                "TestAction3.{OtherToken}.{DifferentToken}"
                ".{ThirdTokenWithOneValue}"
            ],
            tokens_dict={
                "TypeOfToken": ["A", "B"],
                "TypeOfOtherToken": ["1", "2"],
                "TypeOfDifferentToken": ["x", "y", "z"],
                "TypeOfThirdTokenWithOneValue": ["C"],
            },
            expected_outputs=[
                "TestAction1.A.1", "TestAction1.B.1", "TestAction1.A.2",
                "TestAction1.B.2", "TestAction2.NoTokens", "TestAction3.1.x.C",
                "TestAction3.1.y.C", "TestAction3.1.z.C", "TestAction3.2.x.C",
                "TestAction3.2.y.C", "TestAction3.2.z.C"
            ]),
    ]

    def testFunction(test_scenario):
      if test_scenario.expected_outputs is None:
        with self.assertRaises(ValueError):
          test_scenario._PrepareActionsDict()
        return
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
