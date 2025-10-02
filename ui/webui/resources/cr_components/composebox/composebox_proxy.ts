// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './composebox.mojom-webui.js';

export function createAutocompleteMatch(): AutocompleteMatch {
  return {
    a11yLabel: {data: []},
    actions: [],
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    isEnterpriseSearchAggregatorPeopleType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,
    contents: {data: []},
    contentsClass: [{offset: 0, style: 0}],
    description: {data: []},
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: {url: ''},
    inlineAutocompletion: {data: []},
    fillIntoEdit: {data: []},
    iconPath: '',
    iconUrl: {url: ''},
    imageDominantColor: '',
    imageUrl: '',
    isNoncannedAimSuggestion: false,
    removeButtonA11yLabel: {data: []},
    type: '',
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
    keywordChipHint: '',
    keywordChipA11y: '',
  };
}

export interface ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;
}

export class ComposeboxProxyImpl implements ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;
  constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter,
      searchboxHandler: SearchboxPageHandlerRemote,
      searchboxCallbackRouter: SearchboxPageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
    this.searchboxHandler = searchboxHandler;
    this.searchboxCallbackRouter = searchboxCallbackRouter;
  }

  static getInstance(): ComposeboxProxyImpl {
    if (instance) {
      return instance;
    }

    // Composebox connection variables.
    const callbackRouter = new PageCallbackRouter();
    const handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    // Searchbox connection variables.
    const searchboxHandler = new SearchboxPageHandlerRemote();
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    factory.createPageHandler(
        callbackRouter.$.bindNewPipeAndPassRemote(),
        handler.$.bindNewPipeAndPassReceiver(),
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote(),
        searchboxHandler.$.bindNewPipeAndPassReceiver());
    instance = new ComposeboxProxyImpl(
        handler, callbackRouter, searchboxHandler, searchboxCallbackRouter);
    return instance;
  }

  static setInstance(newInstance: ComposeboxProxy) {
    instance = newInstance;
  }
}

let instance: ComposeboxProxy|null = null;
