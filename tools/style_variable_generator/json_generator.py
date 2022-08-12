# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from style_variable_generator.base_generator import Color, BaseGenerator
from style_variable_generator.model import Modes, VariableType

TOKEN_NAME_LABEL = "name"
TOKEN_MODE_LABEL = "mode_values"
TOKEN_FORMULA_LABEL = "formula"
TOKEN_VAL_LABEL = "value"


# This generator is used to provide a raw JSON dump of token names, colors and
# a formula of how they were calculated, in the case of sys tokens. The colors
# are provided in 6 or 8 digit hexadecimal format.
class JSONStyleGenerator(BaseGenerator):
    @staticmethod
    def GetName():
        return 'json'

    def GetParameters(self):
        color_model = self.model.colors
        token_list = []
        for name, mode_values in color_model.items():
            # TODO(b/222408581): Resolve formulas to a more readable format.
            color_item = {
                TOKEN_NAME_LABEL: name,
                TOKEN_MODE_LABEL: {
                    Modes.LIGHT: {
                        TOKEN_VAL_LABEL:
                        str(color_model.ResolveToHexString(name, Modes.LIGHT)),
                        TOKEN_FORMULA_LABEL:
                        str(color_model.Resolve(name, Modes.LIGHT)),
                    },
                    Modes.DARK: {
                        TOKEN_VAL_LABEL:
                        str(color_model.ResolveToHexString(name, Modes.DARK)),
                        TOKEN_FORMULA_LABEL:
                        str(color_model.Resolve(name, Modes.DARK))
                    },
                },
            }
            token_list.append(color_item)
        return {"tokens": json.dumps(token_list, indent=4)}

    def GetGlobals(self):
        return {}

    def GetFilters(self):
        return {}

    def Render(self):
        return self.ApplyTemplate(self, "templates/json_generator.tmpl",
                                  self.GetParameters())
