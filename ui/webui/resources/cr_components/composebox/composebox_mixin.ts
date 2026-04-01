// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageHandlerRemote as SearchboxPageHandlerRemote, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getLoadTimeBoolean} from './common.js';
import type {ComposeboxFile, ComposeboxState} from './common.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ModelMode, ToolMode} from './composebox_query.mojom-webui.js';
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
            isDraggingFile: {
              reflect: true,
              type: Boolean,
            },
            enableImageContextualSuggestions: {
              reflect: true,
              type: Boolean,
            },
            smartComposeEnabled: {
              reflect: true,
              type: Boolean,
            },
            showContextMenuDescription: {type: Boolean},
            shouldShowGhostFiles: {type: Boolean},
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
            usePecApi: {
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
        accessor isDraggingFile: boolean = false;
        accessor enableImageContextualSuggestions: boolean =
            loadTimeData.getBoolean('composeboxShowImageSuggest');
        accessor smartComposeEnabled: boolean =
            loadTimeData.getBoolean('composeboxSmartComposeEnabled');
        contextMenuDescriptionEnabled: boolean =
            loadTimeData.getBoolean('composeboxShowContextMenuDescription');
        accessor showContextMenuDescription: boolean =
            this.contextMenuDescriptionEnabled;
        accessor shouldShowGhostFiles: boolean = false;
        pendingUploads: Set<UnguessableToken> = new Set();
        dragAndDropEnabled: boolean =
            loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
        composeboxSource: string = loadTimeData.getString('composeboxSource');
        maxFileCount: number =
            loadTimeData.getInteger('composeboxFileMaxCount');
        maxFileSize: number = loadTimeData.getInteger('composeboxFileMaxSize');
        attachmentFileTypes: string[] =
            loadTimeData.getString('composeboxAttachmentFileTypes').split(',');
        imageFileTypes: string[] =
            loadTimeData.getString('composeboxImageFileTypes').split(',');
        showTypedSuggestWithContext: boolean = getLoadTimeBoolean(
            'composeboxShowTypedSuggestWithContext', /*defaultValue=*/ false);
        queryZpsOnLoad: boolean =
            getLoadTimeBoolean('queryZpsOnLoad', /*defaultValue=*/ true);
        composeboxCloseByEscape: boolean = getLoadTimeBoolean(
            'composeboxCloseByEscape', /*defaultValue=*/ true);
        clearAllInputsWhenSubmittingQuery: boolean = getLoadTimeBoolean(
            'clearAllInputsWhenSubmittingQuery', /*defaultValue=*/ false);
        contextMenuOpened: boolean = false;

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
        accessor usePecApi: boolean = getLoadTimeBoolean(
            'contextualMenuUsePecApi', /*defaultValue=*/ false);
        showVoiceSearch: boolean = getLoadTimeBoolean(
            'composeboxShowVoiceSearch', /*defaultValue=*/ false);
        accessor smartComposeInlineHint: string = '';
        accessor state: ComposeboxState|null = null;
        accessor submitEnabled: boolean = false;
        accessor tabSuggestions: TabInfo[] = [];
        accessor transcript: string = '';
        accessor uploadButtonDisabled: boolean = false;
        composeboxNoFlickerSuggestionsFix: boolean = getLoadTimeBoolean(
            'composeboxNoFlickerSuggestionsFix', /*defaultValue=*/ false);
        showTypedSuggest: boolean =
            loadTimeData.getBoolean('composeboxShowTypedSuggest');
        lastQueriedInput: string = '';
        haveReceivedAutcompleteResponse: boolean = false;
        lensSendRawFileMediaTypesEnabled: boolean =
            loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled');

        // =====================================================================
        // Embedder-provided methods for DOM and Mojo access
        // =====================================================================

        getInputElement(): ComposeboxInputElement {
          assertNotReached();
        }

        getDropdownElement(): ComposeboxDropdownElement {
          assertNotReached();
        }

        getActiveElement(): Element|null {
          assertNotReached();
        }

        getPageHandler(): PageHandlerRemote {
          assertNotReached();
        }

        getSearchboxHandler(): SearchboxPageHandlerRemote {
          assertNotReached();
        }

        // =====================================================================
        // Common event handlers
        // =====================================================================

        onTranscriptUpdate(e: CustomEvent<string>) {
          this.transcript = e.detail;
        }

        onSpeechReceived() {
          this.receivedSpeech = true;
        }

        onDismissErrorScrim() {
          this.errorMessage = '';
        }

        onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>) {
          this.selectedMatchIndex = e.detail.value;
          this.selectedMatch =
              this.result?.matches[this.selectedMatchIndex] || null;
        }

        /**
         * @param e Event containing index of the match that received focus.
         */
        onMatchFocusin(e: CustomEvent<{index: number}>) {
          // Select the match that received focus.
          this.getDropdownElement().selectIndex(e.detail.index);
        }

        // =====================================================================
        // Common helper methods
        // =====================================================================

        setDefaultModel() {
          if (this.inputState?.activeModel &&
              (this.inputState.activeModel as ModelMode) !==
                  ModelMode.kUnspecified) {
            this.getSearchboxHandler().setActiveModelMode(
                this.inputState.activeModel);
          } else if (
              this.inputState?.allowedModels &&
              this.inputState.allowedModels.length > 0) {
            this.getSearchboxHandler().setActiveModelMode(
                this.inputState.allowedModels[0]!);
          }
        }

        resetToolsAndModels() {
          if (this.inputState) {
            this.getSearchboxHandler().setActiveToolMode(ToolMode.kUnspecified);
            this.getSearchboxHandler().setActiveModelMode(
                ModelMode.kUnspecified);
          }
        }

        closeDropdown() {
          this.clearAutocompleteMatches();
        }


        hasImageFiles(): boolean {
          return Array.from(this.files.values())
              .some(file => file.type.includes('image'));
        }

        hasMatches(): boolean {
          return !!(this.result && this.result.matches.length > 0);
        }

        selectFirstMatch() {
          if (this.result?.matches.length) {
            this.getDropdownElement().selectFirst();
          }
        }

        hasFiles(): boolean {
          return this.files.size > 0;
        }

        queryAutocomplete(clearMatches: boolean) {
          if (clearMatches) {
            this.clearAutocompleteMatches();
          }
          this.lastQueriedInput = this.input;
          this.haveReceivedAutcompleteResponse = false;
          this.getSearchboxHandler().queryAutocomplete(this.input, false);
        }

        clearAutocompleteMatches() {
          this.showDropdown = false;
          this.result = null;
          this.getDropdownElement().unselect();
          this.getSearchboxHandler().stopAutocomplete(/*clearResult=*/ true);
          // Autocomplete sends updates once it is stopped. Invalidate those
          // results by setting the |this.lastQueriedInput| to its default
          // value.
          this.lastQueriedInput = '';
        }

        computeSubmitEnabled(): boolean {
          // Embedders can override this.
          return this.hasValidQuery();
        }

        hasValidQuery(): boolean {
          // Embedders can override this.
          return false;
        }
      }

      return ComposeboxEmbedderMixin;
    };

export interface ComposeboxEmbedderMixinInterface {
  addedTabsIds: Map<number, UnguessableToken>;
  isDraggingFile: boolean;
  enableImageContextualSuggestions: boolean;
  smartComposeEnabled: boolean;
  contextMenuDescriptionEnabled: boolean;
  showContextMenuDescription: boolean;
  shouldShowGhostFiles: boolean;
  pendingUploads: Set<UnguessableToken>;
  dragAndDropEnabled: boolean;
  composeboxSource: string;
  maxFileCount: number;
  maxFileSize: number;
  attachmentFileTypes: string[];
  imageFileTypes: string[];
  showTypedSuggestWithContext: boolean;
  queryZpsOnLoad: boolean;
  composeboxCloseByEscape: boolean;
  clearAllInputsWhenSubmittingQuery: boolean;
  contextMenuOpened: boolean;
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
  usePecApi: boolean;
  showVoiceSearch: boolean;
  smartComposeInlineHint: string;
  state: ComposeboxState|null;
  submitEnabled: boolean;
  tabSuggestions: TabInfo[];
  transcript: string;
  uploadButtonDisabled: boolean;
  composeboxNoFlickerSuggestionsFix: boolean;
  showTypedSuggest: boolean;
  lastQueriedInput: string;
  haveReceivedAutcompleteResponse: boolean;
  lensSendRawFileMediaTypesEnabled: boolean;

  // Embedder-provided methods for DOM and Mojo access
  getInputElement(): ComposeboxInputElement;
  getDropdownElement(): ComposeboxDropdownElement;
  getActiveElement(): Element|null;
  getPageHandler(): PageHandlerRemote;
  getSearchboxHandler(): SearchboxPageHandlerRemote;

  // Common event handlers
  onTranscriptUpdate(e: CustomEvent<string>): void;
  onSpeechReceived(): void;
  onDismissErrorScrim(): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  onMatchFocusin(e: CustomEvent<{index: number}>): void;

  // Common helper methods
  setDefaultModel(): void;
  resetToolsAndModels(): void;
  closeDropdown(): void;
  hasImageFiles(): boolean;
  hasMatches(): boolean;
  selectFirstMatch(): void;
  hasFiles(): boolean;
  queryAutocomplete(clearMatches: boolean): void;
  clearAutocompleteMatches(): void;
  computeSubmitEnabled(): boolean;
  hasValidQuery(): boolean;
}
