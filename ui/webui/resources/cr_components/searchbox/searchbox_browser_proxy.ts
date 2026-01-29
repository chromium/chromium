// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the
 * <cr-searchbox> or the <cr-searchbox-dropdown> and the browser.
 *
 * Testing helpers are located here to prevent duplication across various
 * WebUI test suites (e.g., New Tab Page, Omnibox Popup, Lens) that share
 * these Mojo-based searchbox types.
 */

import type {AutocompleteMatch, AutocompleteResult, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

export function createAutocompleteMatch(
    modifiers: Partial<AutocompleteMatch> = {}): AutocompleteMatch {
  const base: AutocompleteMatch = {
    isHidden: false,
    a11yLabel: '',
    actions: [],
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    isEnterpriseSearchAggregatorPeopleType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,
    contents: '',
    contentsClass: [{offset: 0, style: 0}],
    description: '',
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: '',
    inlineAutocompletion: '',
    fillIntoEdit: '',
    iconPath: '',
    iconUrl: '',
    imageDominantColor: '',
    imageUrl: '',
    isNoncannedAimSuggestion: false,
    removeButtonA11yLabel: '',
    type: '',
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
    hasInstantKeyword: false,
    keywordChipHint: '',
    keywordChipA11y: '',
  };

  return Object.assign(base, modifiers);
}

export function createAutocompleteResultForTesting(
    modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
  const base: AutocompleteResult = {
    input: '',
    matches: [],
    suggestionGroupsMap: {},
    smartComposeInlineHint: null,
  };

  return Object.assign(base, modifiers);
}

export function createSearchMatchForTesting(
    modifiers: Partial<AutocompleteMatch> = {}): AutocompleteMatch {
  const base = createAutocompleteMatch({
    isSearchType: true,
    contents: 'hello world',
    contentsClass: [{offset: 0, style: 0}],
    description: 'Google search',
    descriptionClass: [{offset: 0, style: 4}],
    destinationUrl: 'https://www.google.com/search?q=hello+world',
    fillIntoEdit: 'hello world',
    type: 'search-what-you-typed',
  });

  return Object.assign(base, modifiers);
}

export class SearchboxBrowserProxy {
  static getInstance(): SearchboxBrowserProxy {
    return instance || (instance = new SearchboxBrowserProxy());
  }

  static setInstance(newInstance: SearchboxBrowserProxy) {
    instance = newInstance;
  }

  handler: PageHandlerInterface;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = PageHandler.getRemote();
    this.callbackRouter = new PageCallbackRouter();

    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}

let instance: SearchboxBrowserProxy|null = null;
