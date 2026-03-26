// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {ComposeboxFile, ComposeboxState} from './common.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import type {InputState} from './composebox_query.mojom-webui.js';

type Constructor<T> = new (...args: any[]) => T;

export const ComposeboxEmbedderMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<ComposeboxEmbedderMixinInterface> => {
      class ComposeboxEmbedderMixin extends superClass implements
          ComposeboxEmbedderMixinInterface {
        static get properties() {
          return {
            addedTabsIds: {type: Object},
            canSubmitFilesAndInput: {
              type: Boolean,
              reflect: true,
            },
            contextMenuEnabled: {type: Boolean},
            errorMessage: {type: String},
            files: {type: Object},
            fileUploadsComplete: {
              type: Boolean,
              reflect: true,
            },
            hasAllowedInputs: {
              type: Boolean,
              reflect: true,
            },
            input: {type: String},
            inputPlaceholder: {type: String, reflect: true},
            inputState: {type: Object},
            inToolMode: {
              type: Boolean,
              reflect: true,
            },
            inVoiceSearchMode: {
              type: Boolean,
              reflect: true,
            },
            maxSuggestions: {type: Number},
            receivedSpeech: {type: Boolean},
            result: {type: Object},
            selectedMatchIndex: {type: Number},
            showDropdown: {
              reflect: true,
              type: Boolean,
            },
            showFileCarousel: {
              reflect: true,
              type: Boolean,
            },
            showModelPicker: {
              type: Boolean,
              reflect: true,
            },
            smartComposeInlineHint: {type: String},
            state: {type: Object},
            submitEnabled: {
              reflect: true,
              type: Boolean,
            },
            tabSuggestions: {type: Array},
            transcript: {type: String},
            uploadButtonDisabled: {
              type: Boolean,
              reflect: true,
            },
          };
        }

        accessor addedTabsIds: Map<number, UnguessableToken> = new Map();
        accessor canSubmitFilesAndInput: boolean = true;
        accessor contextMenuEnabled: boolean =
            loadTimeData.getBoolean('composeboxShowContextMenu');
        accessor errorMessage: string = '';
        accessor files: Map<UnguessableToken, ComposeboxFile> = new Map();
        accessor fileUploadsComplete: boolean = true;
        accessor hasAllowedInputs: boolean = false;
        accessor input: string = '';
        accessor inputPlaceholder: string =
            loadTimeData.getString('searchboxComposePlaceholder');
        accessor inputState: InputState|null = null;
        accessor inToolMode: boolean = false;
        accessor inVoiceSearchMode: boolean = false;
        accessor maxSuggestions: number|null = null;
        accessor receivedSpeech: boolean = false;
        accessor result: AutocompleteResult|null = null;
        selectedMatch: AutocompleteMatch|null = null;
        accessor selectedMatchIndex: number = -1;
        accessor showDropdown: boolean =
            loadTimeData.getBoolean('composeboxShowZps');
        accessor showFileCarousel: boolean = false;
        accessor showModelPicker: boolean =
            loadTimeData.valueExists('contextualMenuUsePecApi') ?
            loadTimeData.getBoolean('contextualMenuUsePecApi') :
            false;
        showVoiceSearch: boolean =
            loadTimeData.valueExists('composeboxShowVoiceSearch') ?
            loadTimeData.getBoolean('composeboxShowVoiceSearch') :
            false;
        accessor smartComposeInlineHint: string = '';
        accessor state: ComposeboxState|null = null;
        accessor submitEnabled: boolean = false;
        accessor tabSuggestions: TabInfo[] = [];
        accessor transcript: string = '';
        accessor uploadButtonDisabled: boolean = false;

        getInputElement(): ComposeboxInputElement {
          assertNotReached();
        }
      }

      return ComposeboxEmbedderMixin;
    };

export interface ComposeboxEmbedderMixinInterface {
  addedTabsIds: Map<number, UnguessableToken>;
  errorMessage: string;
  files: Map<UnguessableToken, ComposeboxFile>;
  input: string;
  inputPlaceholder: string;
  inputState: InputState|null;
  canSubmitFilesAndInput: boolean;
  contextMenuEnabled: boolean;
  fileUploadsComplete: boolean;
  hasAllowedInputs: boolean;
  inToolMode: boolean;
  inVoiceSearchMode: boolean;
  maxSuggestions: number|null;
  receivedSpeech: boolean;
  result: AutocompleteResult|null;
  selectedMatch: AutocompleteMatch|null;
  selectedMatchIndex: number;
  showDropdown: boolean;
  showFileCarousel: boolean;
  // TODO(crbug.com/493988206): Rename to usePecApi_ and update all references.
  showModelPicker: boolean;
  showVoiceSearch: boolean;
  smartComposeInlineHint: string;
  state: ComposeboxState|null;
  submitEnabled: boolean;
  tabSuggestions: TabInfo[];
  transcript: string;
  uploadButtonDisabled: boolean;

  getInputElement(): ComposeboxInputElement;
}
