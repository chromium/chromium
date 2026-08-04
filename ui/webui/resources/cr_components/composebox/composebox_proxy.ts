// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {PageHandlerFactory, PageHandlerRemote} from './composebox.mojom-webui.js';

export function createAutocompleteMatch(
    config: Partial<AutocompleteMatch> = {}): AutocompleteMatch {
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
    isContextualSuggestion: false,
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
    keywordModel: null,
    ...config,
  };
}

export interface ComposeboxProxy {
  handler: PageHandlerRemote;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;

  // <if expr="not is_android">
  getSmartTabSharingActive(): Promise<{active: boolean}>;
  setSmartTabSharingActive(active: boolean): void;
  observeSmartTabSharingActive(callback: (active: boolean) => void): number;
  // </if>
}

export class ComposeboxProxyImpl implements ComposeboxProxy {
  handler: PageHandlerRemote;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;
  constructor(
      handler: PageHandlerRemote, searchboxHandler: SearchboxPageHandlerRemote,
      searchboxCallbackRouter: SearchboxPageCallbackRouter) {
    this.handler = handler;
    this.searchboxHandler = searchboxHandler;
    this.searchboxCallbackRouter = searchboxCallbackRouter;
  }

  // <if expr="not is_android">
  getSmartTabSharingActive(): Promise<{active: boolean}> {
    return this.searchboxHandler.getSmartTabSharingActive();
  }

  setSmartTabSharingActive(active: boolean): void {
    this.searchboxHandler.setSmartTabSharingActive(active);
  }

  observeSmartTabSharingActive(callback: (active: boolean) => void): number {
    return this.searchboxCallbackRouter.updateSmartTabSharingActive.addListener(
        callback);
  }
  // </if>

  static getInstance(): ComposeboxProxyImpl {
    if (instance) {
      return instance;
    }

    // Composebox connection variables.
    const handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    // Searchbox connection variables.
    const searchboxHandler = new SearchboxPageHandlerRemote();
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    factory.createPageHandler(
        handler.$.bindNewPipeAndPassReceiver(),
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote(),
        searchboxHandler.$.bindNewPipeAndPassReceiver());
    instance = new ComposeboxProxyImpl(
        handler, searchboxHandler, searchboxCallbackRouter);
    return instance;
  }

  static setInstance(newInstance: ComposeboxProxy) {
    instance = newInstance;
  }
}

let instance: ComposeboxProxy|null = null;
