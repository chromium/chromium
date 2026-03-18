// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {inlineEventHandler} from './eslint/inline_event_handler.js';
import {litElementExpressions} from './eslint/lit_element_bindings.js';
import {litElementInvalidInterface} from './eslint/lit_element_invalid_interface.js';
import {litElementStructureRule} from './eslint/lit_element_structure.js';
import {litElementTemplateStructure} from './eslint/lit_element_template_structure.js';
import {litPropertyAccessorRule} from './eslint/lit_property_accessor.js';
import {noMixedTypeAndValueImports} from './eslint/no_mixed_type_and_value_imports.js';
import {polymerPropertyClassMemberRule} from './eslint/polymer_property_class_member.js';
import {polymerPropertyDeclareRule} from './eslint/polymer_property_declare.js';
import {webComponentMissingDeps} from './eslint/web_component_missing_deps.js';


const rules = {
  'inline-event-handler': inlineEventHandler,
  'lit-element-expressions': litElementExpressions,
  'lit-element-invalid-interface': litElementInvalidInterface,
  'lit-element-structure': litElementStructureRule,
  'lit-element-template-structure': litElementTemplateStructure,
  'lit-property-accessor': litPropertyAccessorRule,
  'no-mixed-type-and-value-imports': noMixedTypeAndValueImports,
  'polymer-property-class-member': polymerPropertyClassMemberRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
  'web-component-missing-deps': webComponentMissingDeps,
};

export default {rules};
