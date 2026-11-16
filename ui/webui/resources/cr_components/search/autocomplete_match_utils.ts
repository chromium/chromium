// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {IconType, SecondaryTextPlacement, SuggestStyle} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import type {FormattedString, FormattedStringFragment, SuggestTemplateInfo} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';

export function createAutocompleteMatch(
    overrides: Partial<AutocompleteMatch> = {}): AutocompleteMatch {
  const base: AutocompleteMatch = {
    isHidden: false,
    a11yLabel: '',
    actions: [],
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    isEnterpriseSearchAggregatorPeopleType: false,
    descriptionIconPath: '',
    showSecondaryTextOnlyOnSelection: false,
    supportsDeletion: false,
    suggestionGroupId: -1,
    destinationUrl: '',
    inlineAutocompletion: '',
    fillIntoEdit: '',
    additionalText: '',
    iconPath: '',
    iconUrl: '',
    removeButtonA11yLabel: '',
    type: '',
    tailSuggestCommonPrefix: null,
    keywordModel: null,
    suggestTemplate: createSuggestTemplateInfo(),
    secondaryTextIsUrl: false,
    urlEmphasis: null,
  };

  return Object.assign(base, overrides);
}

export function createFormattedString(
    text: string = '',
    fragments: FormattedStringFragment[] = []): FormattedString {
  return {text, fragments, a11yText: null};
}

export function createSuggestTemplateInfo(
    overrides: Partial<SuggestTemplateInfo> = {}): SuggestTemplateInfo {
  const base: SuggestTemplateInfo = {
    primaryText: createFormattedString(),
    secondaryText: null,
    secondaryTextPlacement: SecondaryTextPlacement.kUnspecified,
    image: null,
    style: SuggestStyle.kUnspecified,
    typeIcon: IconType.kIconTypeUnspecified,
    fuseboxAction: null,
    actionIcon: null,
    typeIconBadge: null,
  };

  return Object.assign(base, overrides);
}
