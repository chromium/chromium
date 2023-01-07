# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from style_variable_generator.base_generator import Color, BaseGenerator
from style_variable_generator.model import Modes, VariableType
from .css_generator import CSSStyleGenerator

SYS_SUBSTR = "sys"
REF_SUBSTR = "ref"
TOKENS_SUBSTR = "tokens"


def TokenArrayPropertyTitle(property):
    return "%s_%s" % (property, TOKENS_SUBSTR)


# This generator is used to provide a raw JSON dump of token names, css
# variables and, in the case of sys tokens, a formula of how they were
# calculated.
class JSONStyleGenerator(CSSStyleGenerator):
    @staticmethod
    def GetName():
        return 'json'

    def __init__(self):
        super().__init__()
        # We want the output JSON to include the formula for how a color was
        # calculated, which means we don't want to resolve blended colors down
        # to RGBA values.
        self.resolve_blended_colors = False

    def GetParameters(self):
        color_model = self.model.colors
        ref_tokens = []
        sys_tokens = []
        tokens = {}
        for name, mode_values in color_model.items():
            color_item = {
                "token_name": name,
                "css_variable": self.ToCSSVarName(name),
            }
            # Ref tokens should not contain a dark mode entry, as we default to
            # light mode. Sys tokens that do not have generate_per_mode will
            # have an entry for both light and dark, and we want to include the
            # formula for these.
            if SYS_SUBSTR in name:
                color_item["mode_values"] = {
                    Modes.LIGHT:
                    color_model.Resolve(name, Modes.LIGHT).GetFormula(),
                    Modes.DARK:
                    color_model.Resolve(name, Modes.DARK).GetFormula()
                }
                sys_tokens.append(color_item)
            else:
                ref_tokens.append(color_item)
        return {
            "tokens":
            json.dumps(
                {
                    TokenArrayPropertyTitle(REF_SUBSTR): ref_tokens,
                    TokenArrayPropertyTitle(SYS_SUBSTR): sys_tokens,
                },
                indent=4)
        }

    def GetGlobals(self):
        return {}

    def GetFilters(self):
        return {}

    def Render(self):
        return self.ApplyTemplate(self, "templates/json_generator.tmpl",
                                  self.GetParameters())
