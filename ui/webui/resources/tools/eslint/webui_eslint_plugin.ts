// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {inlineEventHandler} from './inline_event_handler.js';
import {litElementExpressions} from './lit_element_bindings.js';
import {litElementInvalidInterface} from './lit_element_invalid_interface.js';
import {litElementStructureRule} from './lit_element_structure.js';
import {litElementTemplateStructure} from './lit_element_template_structure.js';
import {litPropertyAccessorRule} from './lit_property_accessor.js';
import {noAssertEqualsBoolean} from './no_assert_equals_boolean.js';
import {noMixedTypeAndValueImports} from './no_mixed_type_and_value_imports.js';
import {polymerPropertyClassMemberRule} from './polymer_property_class_member.js';
import {polymerPropertyDeclareRule} from './polymer_property_declare.js';
import {webComponentMissingDeps} from './web_component_missing_deps.js';


const rules = {
  'inline-event-handler': inlineEventHandler,
  'lit-element-expressions': litElementExpressions,
  'lit-element-invalid-interface': litElementInvalidInterface,
  'lit-element-structure': litElementStructureRule,
  'lit-element-template-structure': litElementTemplateStructure,
  'lit-property-accessor': litPropertyAccessorRule,
  'no-assert-equals-boolean': noAssertEqualsBoolean,
  'no-mixed-type-and-value-imports': noMixedTypeAndValueImports,
  'polymer-property-class-member': polymerPropertyClassMemberRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
  'web-component-missing-deps': webComponentMissingDeps,
};

export default {rules};
