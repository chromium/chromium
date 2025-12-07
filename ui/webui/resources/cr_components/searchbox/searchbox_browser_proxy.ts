// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the
 * <cr-searchbox> or the <cr-searchbox-dropdown> and the browser.
 */

import type {AutocompleteMatch, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

export function createAutocompleteMatch(): AutocompleteMatch {
  return {
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
    destinationUrl: {url: ''},
    inlineAutocompletion: '',
    fillIntoEdit: '',
    iconPath: '',
    iconUrl: {url: ''},
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
