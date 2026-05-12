// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import type {I18nMixinLitInterface} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SelectedFileInfo, SmartComposeStats, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {DriveUploadError} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ComposeboxFile, ComposeboxFileValidationError, ContextType, ContextualSearchInputStateDeletionType, FILE_VALIDATION_ERRORS_MAP, getLoadTimeBoolean, isContextUploadStatusTerminal, ProcessFilesError, recordBoolean, recordContextAdditionMethod, recordContextualElementClickedMetric, recordEnumerationValue, recordInputTypeShown, recordModelModeSelection, recordModelModeShown, recordToolModeSelection, recordToolModeShown, recordUserAction} from './common.js';
import type {ComposeboxState, DriveUpload, TabUpload, TabUploadOrigin} from './common.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ContextUploadStatus, InputType, ModelMode, ToolMode} from './composebox_query.mojom-webui.js';
import type {ContextUploadErrorType, InputState} from './composebox_query.mojom-webui.js';
import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';
import {WindowProxy} from './window_proxy.js';

export enum VoiceSearchAction {
  ACTIVATE = 0,
  QUERY_SUBMITTED = 1,
}

type Constructor<T> = new (...args: any[]) => T;

export const ComposeboxEmbedderMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<I18nMixinLitInterface>&
    Constructor<ComposeboxEmbedderMixinInterface> => {
      class ComposeboxEmbedderMixin extends I18nMixinLit
      (superClass) implements ComposeboxEmbedderMixinInterface {
        static get properties() {
          return {
            addedTabsIds: {type: Object},
            animationState: {
              reflect: true,
              type: String,
            },
            disableCaretColorAnimation: {
              type: Boolean,
              reflect: true,
            },
            disableVoiceSearchAnimation: {type: Boolean},
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
            smartTabSharingActive: {type: Boolean},
            shouldShowGhostFiles: {type: Boolean},
            showMenuOnClick: {type: Boolean},
            isCanvasQuerySubmitted: {type: Boolean},
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
            searchboxLayoutMode: {
              type: String,
              reflect: true,
            },
            /**
             * Generic flag indicating a "Next" searchbox (Realbox Next, Omnibox
             * Next, etc.). Used for all styling and behavior shared across
             * 'Next' searchbox implementations.
             */
            searchboxNextEnabled: {
              type: Boolean,
              reflect: true,
            },
            selectedMatchIndex: {type: Number},
            showDropdown: {
              reflect: true,
              type: Boolean,
            },
            dropdownNeeded: {type: Boolean},
            clearAllInputsWhenSubmittingQuery: {type: Boolean},
            closeOnEscape: {type: Boolean},
            composeboxNoFlickerSuggestionsFix: {type: Boolean},
            showFileCarousel: {
              reflect: true,
              type: Boolean,
            },
            showTypedSuggestWithContext: {type: Boolean},
            showVoiceSearch: {type: Boolean},
            usePecApi: {
              type: Boolean,
              reflect: true,
            },
            smartComposeInlineHint: {type: String},
            smartComposeStats: {type: Object},
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
            hasVoiceSearchError: {type: Boolean},
            isListening: {type: Boolean},
          };
        }

        automaticActiveTab: ComposeboxFile|null = null;
        accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
        accessor disableCaretColorAnimation: boolean = false;
        accessor disableVoiceSearchAnimation: boolean = false;
        accessor addedTabsIds: Map<number, UnguessableToken> = new Map();
        accessor isDraggingFile: boolean = false;
        accessor enableImageContextualSuggestions: boolean =
            loadTimeData.getBoolean('composeboxShowImageSuggest');
        accessor smartComposeEnabled: boolean =
            loadTimeData.getBoolean('composeboxSmartComposeEnabled');
        accessor smartTabSharingActive: boolean = false;
        contextMenuDescriptionEnabled: boolean =
            loadTimeData.getBoolean('composeboxShowContextMenuDescription');
        accessor showContextMenuDescription: boolean =
            this.contextMenuDescriptionEnabled;
        accessor shouldShowGhostFiles: boolean = false;
        accessor showMenuOnClick: boolean = true;
        accessor isCanvasQuerySubmitted: boolean = false;
        // If voice search error scrim is showing:
        accessor hasVoiceSearchError: boolean = false;
        // Voice search is listening if there is no error and voice search
        // overlay is open (and active).
        accessor isListening: boolean = false;

        browserTabContextAdded: boolean = false;
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
        queryZpsOnLoad: boolean =
            getLoadTimeBoolean('queryZpsOnLoad', /*defaultValue=*/ true);
        contextMenuOpened: boolean = false;

        accessor canSubmitFilesAndInput: boolean = true;
        accessor clearAllInputsWhenSubmittingQuery: boolean = false;
        accessor closeOnEscape: boolean = true;
        accessor composeboxNoFlickerSuggestionsFix: boolean = false;
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
        // Indicates if voice search overlay is open. Does not indicate if it
        // is 'listening'. This is because there might be an error scrim showing
        // (see `hasVoiceSearchError`), making voice search not 'listening'.
        accessor inVoiceSearchMode: boolean = false;
        accessor maxSuggestions: number|null = null;
        accessor receivedSpeech: boolean = false;
        accessor result: AutocompleteResult|null = null;
        accessor searchboxLayoutMode: string = '';
        accessor searchboxNextEnabled: boolean = false;
        selectedMatch: AutocompleteMatch|null = null;
        accessor selectedMatchIndex: number = -1;
        accessor showDropdown: boolean =
            loadTimeData.getBoolean('composeboxShowZps');
        accessor dropdownNeeded: boolean = true;
        accessor showFileCarousel: boolean = false;
        accessor showTypedSuggestWithContext: boolean = false;
        accessor usePecApi: boolean = false;
        searchboxListenerIds: number[] = [];
        showZps: boolean = loadTimeData.getBoolean('composeboxShowZps');
        // Attribute that can be set by parent to enable/disable voice search
        // overlay. `inVoiceSearchMode` indicates if voice search overlay is (at
        // least) open.
        accessor showVoiceSearch: boolean = false;
        accessor smartComposeInlineHint: string = '';
        accessor smartComposeStats: SmartComposeStats = {
          enabled: loadTimeData.getBoolean('composeboxSmartComposeEnabled'),
          shownCount: 0,
          acceptedCount: 0,
          charactersAccepted: 0,
          shownLength: 0,
        };
        accessor state: ComposeboxState|null = null;
        accessor submitEnabled: boolean = false;
        accessor tabSuggestions: TabInfo[] = [];
        accessor transcript: string = '';
        accessor uploadButtonDisabled: boolean = false;
        showTypedSuggest: boolean =
            loadTimeData.getBoolean('composeboxShowTypedSuggest');
        lastQueriedInput: string = '';
        haveReceivedAutcompleteResponse: boolean = false;
        lensSendRawFileMediaTypesEnabled: boolean =
            loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled');

        // =====================================================================
        // Lifecycle Hooks
        // =====================================================================

        override connectedCallback() {
          super.connectedCallback();

          this.searchboxListenerIds = [
            this.getSearchboxCallbackRouter()
                .autocompleteResultChanged.addListener(
                    this.onAutocompleteResultChanged.bind(this)),
            this.getSearchboxCallbackRouter()
                .onContextualInputStatusChanged.addListener(
                    this.onContextualInputStatusChanged.bind(this)),
            this.getSearchboxCallbackRouter().onTabStripChanged.addListener(
                this.refreshTabSuggestions.bind(this)),
            this.getSearchboxCallbackRouter().addFileContext.addListener(
                this.addFileContextFromBrowser.bind(this)),
            this.getSearchboxCallbackRouter().onInputStateChanged.addListener(
                this.onInputStateChanged.bind(this)),
          ];

          this.getSearchboxHandler().notifySessionStarted();

          this.initializeInitialState_();

          // For "next" searchboxes (Realbox Next, Omnibox Next, etc.), the zps
          // autocomplete query is triggered after the state has been initialized.
          if (this.queryZpsOnLoad && !this.searchboxNextEnabled) {
            this.queryAutocomplete(/* clearMatches= */ false);
          }
        }

        private async initializeInitialState_() {
          const inputStateResponse =
              await this.getSearchboxHandler().getInputState();
          if (inputStateResponse) {
            this.inputState = inputStateResponse.state;
          }
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          this.getSearchboxHandler().notifySessionAbandoned();

          this.searchboxListenerIds.forEach(
              id =>
                  assert(this.getSearchboxCallbackRouter().removeListener(id)));
          this.searchboxListenerIds = [];
        }

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          const changedPrivateProperties =
              changedProperties as Map<PropertyKey, unknown>;
          // When the result initially gets set check if dropdown should show.
          if (changedPrivateProperties.has('input') ||
              changedPrivateProperties.has('result') ||
              changedPrivateProperties.has('files') ||
              changedPrivateProperties.has('errorMessage')) {
            this.showFileCarousel = this.files.size > 0;
            this.showDropdown = this.computeShowDropdown();
          }

          if (changedPrivateProperties.has('input') ||
              changedPrivateProperties.has('selectedMatchIndex') ||
              changedPrivateProperties.has('inputState') ||
              changedPrivateProperties.has('isFollowupQuery') ||
              changedPrivateProperties.has('files') ||
              changedPrivateProperties.has('submitEnabled') ||
              changedPrivateProperties.has('fileUploadsComplete')) {
            this.submitEnabled = this.computeSubmitEnabled();
            this.uploadButtonDisabled = !this.fileUploadsComplete;
            // `canSubmitFilesAndInput` checks if there is a valid query rather
            // than if submit is enabled, as `submitEnabled` only defines if the
            // submit button should be shown rather than its actual active
            // state.
            this.canSubmitFilesAndInput =
                this.hasValidQuery() && this.fileUploadsComplete;
          }

          if (changedPrivateProperties.has('canSubmitFilesAndInput')) {
            this.fire('can-submit-files-and-input-changed', {
              canSubmitFilesAndInput: this.canSubmitFilesAndInput,
            });
          }

          if (changedPrivateProperties.has('inputState') && this.inputState) {
            this.hasAllowedInputs =
                (this.inputState.allowedModels.length > 0 ||
                 this.inputState.allowedTools.length > 0 ||
                 this.inputState.allowedInputTypes.length > 0);
            this.inToolMode =
                this.inputState.activeTool !== ToolMode.kUnspecified;
            this.dispatchEvent(new CustomEvent('input-state-changed', {
              detail: {inputState: this.inputState},
            }));
          }

          if (changedPrivateProperties.has('files') ||
              changedPrivateProperties.has('inputState') ||
              changedPrivateProperties.has('inputState.activeTool')) {
            this.updateInputPlaceholder();
          }

          // Listening is true if there is no error and voice search is open.
          if (changedProperties.has('inVoiceSearchMode') ||
              changedProperties.has('hasVoiceSearchError')) {
            this.isListening =
                this.inVoiceSearchMode && !this.hasVoiceSearchError;
          }
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          if (changedProperties.has('inputState')) {
            const oldInputState =
                changedProperties.get('inputState') as InputState | undefined;
            if (oldInputState &&
                this.inputState?.activeTool !== oldInputState.activeTool) {
              this.focusInput();
              this.queryAutocomplete(/* clearMatches= */ true);
            }
          }

          const changedPrivateProperties =
              changedProperties as Map<PropertyKey, unknown>;
          if (changedPrivateProperties.has('selectedMatchIndex')) {
            if (this.selectedMatch) {
              // Update the input.
              const text = this.selectedMatch.fillIntoEdit;
              this.input = text;
            } else if (!this.lastQueriedInput) {
              // This is for cases when focus leaves the matches/input.
              // If there was already text in the input do not clear it.
              this.clearInput();
            } else {
              // For typed queries reset the input back to typed value when
              // focus leaves the match.
              this.input = this.lastQueriedInput;
            }
          }

          if (changedPrivateProperties.has('smartComposeInlineHint')) {
            if (this.smartComposeInlineHint) {
              // TODO(crbug.com/452619068): Investigate why screenreader is
              // inconsistent.
              const announcer = getAnnouncerInstance();
              announcer.announce(
                  this.smartComposeInlineHint + ', ' +
                  this.i18n('composeboxSmartComposeTitle'));
            }
          }
        }

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

        getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
          assertNotReached();
        }

        getSearchboxHandler(): SearchboxPageHandlerRemote {
          assertNotReached();
        }

        // =====================================================================
        // Common event handlers
        // =====================================================================

        // This function is called when backend starts a file upload flow,
        // whether through `addFileFromAttachment_`,
        // `addFileContextFromBrowser`, etc. This contrasts with the workflows
        // where the frontend starts a file upload flow
        // (`addFileContext`).
        onFileContextAdded(file: ComposeboxFile) {
          const newFiles = new Map(this.files);
          newFiles.set(file.uuid, file);
          this.files = newFiles;
          if (file.status !== ContextUploadStatus.kUploadSuccessful) {
            this.addToPendingUploads(file.uuid);
          }
        }

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

        onMatchClick(e: CustomEvent<{
          ctrlKey: boolean,
          metaKey: boolean,
          shiftKey: boolean,
        }>) {
          this.clearAutocompleteMatches();
          // We only close the composebox when opening in a new tab because
          // doing so in the current tab causes a visual jitter where the
          // composebox closes before the new results page finishes loading.
          if (e && e.detail &&
              (e.detail.ctrlKey || e.detail.metaKey || e.detail.shiftKey)) {
            this.closeComposebox();
          }
        }

        /**
         * @param e Event containing index of the match that received focus.
         */
        onMatchFocusin(e: CustomEvent<{index: number}>) {
          // Select the match that received focus.
          this.getDropdownElement().selectIndex(e.detail.index);
        }

        onInputStateChanged(inputState: InputState) {
          this.inputState = inputState;

          const allowedTypes = this.inputState.allowedInputTypes;
          this.files.forEach((file, uuid) => {
            if (!allowedTypes.includes(file.inputType)) {
              this.deleteFile(uuid);
            }
          });
        }

        onAutocompleteResultChanged(result: AutocompleteResult) {
          if (this.lastQueriedInput === null ||
              this.lastQueriedInput.trimStart() !== result.input) {
            return;
          }

          this.result = result;

          const hasMatches = this.result.matches.length > 0;
          const firstMatch = hasMatches ? this.result.matches[0] : null;
          // Zero suggest matches are not allowed to be default. Therefore, this
          // makes sure zero suggest results aren't focused when they are
          // returned.
          if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
            this.selectFirstMatch();
          } else if (
              this.input.trim() && hasMatches && this.selectedMatchIndex >= 0 &&
              this.selectedMatchIndex < this.result.matches.length) {
            // Restore the selection and update the input. Don't restore when
            // the user deletes all their input and autocomplete is queried or
            // else the empty input will change to the value of the first
            // result.
            this.getDropdownElement().selectIndex(this.selectedMatchIndex);

            // Set the selected match since the `selectedMatchIndex` does not
            // change (and therefore `selectedMatch` does not get updated since
            // `onSelectedMatchIndexChanged_` is not called).
            this.selectedMatch = this.result.matches[this.selectedMatchIndex]!;
            this.input = this.selectedMatch.fillIntoEdit;
          } else {
            this.getDropdownElement().unselect();
          }

          // Populate the smart compose suggestion.
          const nextHint = this.result.smartComposeInlineHint?.trim() ?
              this.result.smartComposeInlineHint :
              '';
          if (this.smartComposeInlineHint !== nextHint) {
            this.smartComposeInlineHint = nextHint;
          }

          // Smart compose stats are incremented on every response from the
          // server.
          if (this.smartComposeInlineHint) {
            this.smartComposeStats.shownCount++;
            this.smartComposeStats.shownLength +=
                this.smartComposeInlineHint.length;
          }
        }

        onContextualInputStatusChanged(
            token: UnguessableToken, status: ContextUploadStatus,
            errorType: ContextUploadErrorType|null) {
          // If error message is updated, then the returned file is stale and
          // removed from carousel. File is removed from carousel on
          // `kUploadReplaced` as well despite no error message being returned
          // (special case). Else, `file` below is updated to its most recent
          // state, and `errorMessage` is null.
          const {file, errorMessage} =
              this.updateFileStatus(token, status, errorType);
          if (errorMessage) {  // `file` value is definitely stale.
            this.errorMessage = errorMessage;
            this.pendingUploads.delete(token);
            this.fileUploadsComplete = this.pendingUploads.size === 0;
          } else if (file) {
            // Treat `kUploadReplaced` like an error upload state
            // (like `kUploadFailed`. `kValidationFailed`,
            // `kUploadExpired`), just without setting `errorMessage`.
            // This means for `kUploadReplaced`, we do not fetch suggestions,
            // etc.
            if (file.status === ContextUploadStatus.kUploadReplaced) {
              this.pendingUploads.delete(file.uuid);
              this.fileUploadsComplete = this.pendingUploads.size === 0;
              return;
            } else if (file.status === ContextUploadStatus.kUploadSuccessful) {
              // At this point, due to the error message handling above (for
              // `kValidationFailed`, `kUploadExpired`, and `kUploadFailed`),
              // if kUploadSuccessful, the file upload is complete.
              // Else, the file upload is in progress.
              this.pendingUploads.delete(file.uuid);
              this.fileUploadsComplete = this.pendingUploads.size === 0;

              const announcer = getAnnouncerInstance();
              announcer.announce(this.i18n('composeboxFileUploadCompleteText'));
            } else if (
                file.status === ContextUploadStatus.kProcessing ||
                file.status ===
                    ContextUploadStatus.kProcessingSuggestSignalsReady) {
              // `NotUploaded`, `UploadStarted` come before and after
              // `kProcessing`
              //  respectively, so we only need to add to `pendingUploads` when
              //  in a type of processing state.
              this.addToPendingUploads(file.uuid);
            }

            // Fetch contextual suggestions for processingSuggestSignalsReady
            // non-images:
            if (status === ContextUploadStatus.kProcessingSuggestSignalsReady &&
                this.showZps && !file.type.includes('image')) {
              // Query autocomplete to get contextual suggestions for files.
              this.queryAutocomplete(/* clearMatches= */ true);
            }
            // For image files:
            if (status === ContextUploadStatus.kProcessingSuggestSignalsReady &&
                file.type.includes('image')) {
              if (this.enableImageContextualSuggestions) {
                // Query autocomplete to get contextual suggestions for files.
                this.queryAutocomplete(/* clearMatches= */ true);
              } else {
                this.showDropdown = false;
              }
            }

            // Query autocomplete to get contextual suggestions for tabs.
            if (status === ContextUploadStatus.kProcessing &&
                file.type.includes('tab')) {
              this.queryAutocomplete(/* clearMatches= */ true);
            }
          }
        }

        onInputInput(_e: CustomEvent<Event>) {
          const newInput = this.getInputElement().input;
          if (this.smartComposeEnabled && this.smartComposeInlineHint) {
            if (newInput === this.input + this.smartComposeInlineHint[0]) {
              this.smartComposeInlineHint =
                  this.smartComposeInlineHint.substring(1);
            } else if (newInput !== this.input) {
              this.smartComposeInlineHint = '';
            }
          }
          this.input = newInput;

          // `clearMatches` is true if input is empty stop any in progress
          // providers before requerying for on-focus (zero-suggest) inputs. The
          // searchbox doesn't allow zero-suggest requests to be made while the
          // ACController is not done.
          if (this.composeboxNoFlickerSuggestionsFix) {
            // If the composebox no flickering fix is enabled, stop the
            // ACController from querying for suggestions when the input is
            // empty, but don't clear the matches so the dropdown doesn't close.
            if (this.input === '') {
              this.getSearchboxHandler().stopAutocomplete(
                  /*clearResult=*/ true);
            }
            this.queryAutocomplete(/* clearMatches= */ false);
          } else {
            this.queryAutocomplete(/* clearMatches= */ this.input === '');
          }
        }

        onInputFocusin() {
          // if there's a last queried input, it's guaranteed that at least
          // the verbatim match will exist.
          if (this.lastQueriedInput) {
            this.selectFirstMatch();
          }
        }

        onRecordingStopped(e: CustomEvent<string>) {
          const newTranscript = e.detail;
          if (newTranscript && newTranscript.trim().length > 0) {
            this.input = newTranscript;
            this.queryAutocomplete(/* clearMatches= */ false);
          }
          this.voiceSearchEndCleanup();
        }

        isFocusInInput(): boolean {
          return this.getActiveElement() === this.getInputElement();
        }

        finalizeMatchSelection(e: KeyboardEvent) {
          this.smartComposeInlineHint = '';
          e.preventDefault();
          if (this.getActiveElement() === this.getDropdownElement()) {
            this.getDropdownElement().focusSelected();
          }
        }

        protected handleArrowKey_(e: KeyboardEvent) {
          if (!this.dropdownNeeded) {
            return;
          }
          if (this.isFocusInInput() && !this.showDropdown) {
            return;
          }
          if (!this.hasMatches() || hasKeyModifiers(e)) {
            return;
          }

          if (e.key === 'ArrowDown') {
            this.getDropdownElement().selectNext();
          } else if (e.key === 'ArrowUp') {
            this.getDropdownElement().selectPrevious();
          }
          this.finalizeMatchSelection(e);
        }

        protected handleTab_(e: KeyboardEvent) {
          if (this.isFocusInInput()) {
            // If focus leaves the input, unselect the first match.
            if (e.shiftKey) {
              this.getDropdownElement().unselect();
            } else if (
                this.smartComposeEnabled && this.smartComposeInlineHint) {
              this.smartComposeStats.acceptedCount++;
              this.smartComposeStats.charactersAccepted +=
                  this.smartComposeInlineHint.length;
              this.input = this.input + this.smartComposeInlineHint;
              this.smartComposeInlineHint = '';
              e.preventDefault();
              this.queryAutocomplete(/* clearMatches= */ false);
            }
            return;
          }

          if (this.hasMatches() && this.dropdownNeeded && !hasKeyModifiers(e)) {
            // If focus goes past the last match, unselect the last match.
            if (this.selectedMatchIndex === this.result!.matches.length - 1) {
              if (this.selectedMatch!.supportsDeletion) {
                const focusedMatchElem =
                    this.getActiveElement()?.shadowRoot?.activeElement;
                const focusedButtonElem =
                    focusedMatchElem?.shadowRoot?.activeElement;
                if (focusedButtonElem?.id === 'remove') {
                  this.getDropdownElement().unselect();
                }
              } else {
                this.getDropdownElement().unselect();
              }
            }
          }
        }

        protected handleEnter_(e: KeyboardEvent) {
          if (this.getActiveElement() === this.getDropdownElement() ||
              !e.shiftKey) {
            e.preventDefault();
            if (this.canSubmitFilesAndInput) {
              this.submitQuery(e);
            }
          }
        }

        protected handleEscape_(e: KeyboardEvent) {
          this.handleEscapeKeyLogic();
          e.stopPropagation();
          e.preventDefault();
        }

        protected handlePageNavigation_(e: KeyboardEvent) {
          if (!this.hasMatches() || !this.dropdownNeeded ||
              hasKeyModifiers(e)) {
            return;
          }

          if (e.key === 'PageUp') {
            this.selectFirstMatch();
          } else {
            this.getDropdownElement().selectLast();
          }
          this.finalizeMatchSelection(e);
        }

        onKeydown(e: KeyboardEvent) {
          const HANDLED_KEYS = [
            'ArrowDown',
            'ArrowUp',
            'Enter',
            'Escape',
            'PageDown',
            'PageUp',
            'Tab',
          ];
          if (!HANDLED_KEYS.includes(e.key)) {
            return;
          }

          const handlers: Record<string, (e: KeyboardEvent) => void> = {
            'ArrowDown': (e) => this.handleArrowKey_(e),
            'ArrowUp': (e) => this.handleArrowKey_(e),
            'Enter': (e) => this.handleEnter_(e),
            'Escape': (e) => this.handleEscape_(e),
            'Tab': (e) => this.handleTab_(e),
            'PageUp': (e) => this.handlePageNavigation_(e),
            'PageDown': (e) => this.handlePageNavigation_(e),
          };

          handlers[e.key]!(e);
        }

        updateInputPlaceholder() {
          if (this.inputState) {
            if (this.inputState.activeTool !== ToolMode.kUnspecified) {
              const config = this.inputState.toolConfigs.find(
                  c => c.tool === this.inputState!.activeTool);
              if (config?.hintText) {
                this.inputPlaceholder = config.hintText;
                return;
              }
            }

            if (this.inputState.activeModel !== ModelMode.kUnspecified) {
              const config = this.inputState.modelConfigs.find(
                  c => c.model === this.inputState!.activeModel);
              if (config?.hintText) {
                this.inputPlaceholder = config.hintText;
                return;
              }
            }

            if (this.inputState.hintText) {
              this.inputPlaceholder = this.inputState.hintText;
              return;
            }
          }
          if (this.inputState?.activeTool === ToolMode.kDeepSearch) {
            this.inputPlaceholder =
                loadTimeData.getString('composeDeepSearchPlaceholder');
          } else if (this.inputState?.activeTool === ToolMode.kImageGen) {
            this.inputPlaceholder =
                loadTimeData.getString('composeCreateImagePlaceholder');
          } else {
            this.inputPlaceholder =
                loadTimeData.getString('searchboxComposePlaceholder');
          }
        }

        isTogglingOff(tool: ToolMode): boolean {
          return this.inputState?.activeTool === tool;
        }

        onToolClick(e: CustomEvent<{toolMode: ToolMode}>) {
          if (!this.isTogglingOff(e.detail.toolMode)) {
            recordToolModeSelection(
                e.detail.toolMode, this.composeboxSource, 'AimPopup');
          }
          this.handleToolClick(e.detail.toolMode);
        }

        handleToolClick(tool: ToolMode) {
          const isTogglingOff = this.isTogglingOff(tool);

          const newToolMode = isTogglingOff ? ToolMode.kUnspecified : tool;

          if (isTogglingOff) {
            const metricName =
                `ContextualSearch.UserAction.InputStateDeletion.${
                    this.composeboxSource}`;
            recordEnumerationValue(
                metricName, ContextualSearchInputStateDeletionType.TOOL,
                ContextualSearchInputStateDeletionType.MAX_VALUE + 1);

            const userActionName =
                `ContextualSearch.UserAction.InputStateDeletion.Tool.${
                    this.composeboxSource}`;
            recordUserAction(userActionName);
          } else {
            this.getSearchboxHandler().recordToolSelectionAction(newToolMode);
          }
          this.handleToolModeUpdate(newToolMode);
        }

        handleToolModeUpdate(newTool: ToolMode) {
          this.getSearchboxHandler().setActiveToolMode(newTool);
          this.queryAutocomplete(/* clearMatches= */ true);
          this.updateInputPlaceholder();
        }

        onModelClick(e: CustomEvent<{model: ModelMode}>) {
          recordModelModeSelection(
              e.detail.model, this.composeboxSource, 'AimPopup');
          this.getSearchboxHandler().recordModelSelectionAction(e.detail.model);
          this.getSearchboxHandler().setActiveModelMode(e.detail.model);
          this.updateInputPlaceholder();
        }

        onOpenImageUpload() {
          recordContextualElementClickedMetric(
              this.composeboxSource, 'AimPopup', ContextType.IMAGE);
        }

        onOpenFileUpload() {
          recordContextualElementClickedMetric(
              this.composeboxSource, 'AimPopup', ContextType.FILE);
        }

        async onOpenDriveUpload() {
          recordContextualElementClickedMetric(
              this.composeboxSource, 'AimPopup', ContextType.DRIVE);
          const {response} =
              await this.getSearchboxHandler().onDriveUploadClicked();
          this.addDriveUploads(response.files, response.error ?? undefined);
        }

        onSmartTabSharingActiveChanged(e: CustomEvent<{active: boolean}>) {
          this.smartTabSharingActive = e.detail.active;
          this.getPageHandler().setSmartTabSharingActive(e.detail.active);
        }

        onContextMenuContainerMousedown(e: FocusEvent) {
          // Special treatment for the "Tall" layout variants where not clicking
          // on an inner element should be treated as clicking on a
          // non-focusable area.
          if (this.searchboxLayoutMode !== 'Compact' &&
              (e.target instanceof HTMLElement &&
               e.target.id === 'contextMenuContainer')) {
            e.preventDefault();
            e.stopPropagation();
          }
        }

        onContextMenuContainerClick(e: MouseEvent) {
          e.preventDefault();
          e.stopPropagation();

          // Ignore non-primary button clicks.
          if (e.button !== 0) {
            return;
          }

          if (this.searchboxLayoutMode !== 'Compact') {
            this.focusInput();
          }
        }

        onDeleteTabContext(
            e: CustomEvent<
                {uuid: UnguessableToken, fromUserAction?: boolean}>) {
          this.deleteFile(e.detail.uuid, e.detail.fromUserAction);
        }

        onAddTabContext(e: CustomEvent<{
          id: number,
          title: string,
          url: Url,
          delayUpload: boolean,
          origin: TabUploadOrigin,
        }>) {
          if (!this.browserTabContextAdded) {
            recordContextualElementClickedMetric(
                this.composeboxSource, 'AimPopup', ContextType.TAB);
            this.browserTabContextAdded = true;
          }
          this.addTabContextHandleCallback({
            tabId: e.detail.id,
            title: e.detail.title,
            url: e.detail.url,
            delayUpload: e.detail.delayUpload,
            origin: e.detail.origin,
          });
        }

        addTabContextHandleCallback(
            _tabUpload: TabUpload,
            _replaceAutoActiveTabToken: boolean = false): Promise<void> {
          assertNotReached();
        }

        async onContextMenuClosed() {
          this.contextMenuOpened = false;

          await this.updateComplete;
          this.focusInput();
        }

        onContextMenuOpened() {
          this.browserTabContextAdded = false;
          this.contextMenuOpened = true;
          this.refreshTabSuggestions();
          this.getPageHandler().onContextMenuOpened();

          if (this.inputState) {
            const {allowedInputTypes, disabledInputTypes} = this.inputState;
            allowedInputTypes.forEach((inputType: InputType) => {
              if (inputType !== InputType.kBrowserTab &&
                  !disabledInputTypes.includes(inputType)) {
                recordInputTypeShown(
                    inputType, this.composeboxSource, 'AimPopup');
              }
            });

            const {allowedTools, disabledTools} = this.inputState;
            allowedTools.forEach((tool: ToolMode) => {
              if (!disabledTools.includes(tool)) {
                recordToolModeShown(tool, this.composeboxSource, 'AimPopup');
              }
            });

            const {allowedModels, disabledModels} = this.inputState;
            allowedModels.forEach((model: ModelMode) => {
              if (!disabledModels.includes(model)) {
                recordModelModeShown(model, this.composeboxSource, 'AimPopup');
              }
            });
          }
        }

        onVoiceSearchButtonClick() {
          this.inVoiceSearchMode = true;
          this.hasVoiceSearchError = false;
          this.animationState = GlowAnimationState.LISTENING;
          this.fire('voice-search-action', {value: VoiceSearchAction.ACTIVATE});
          // For contextual tasks composebox voice metrics.
          this.fire('composebox-voice-search-start');
          this.shadowRoot
              ?.querySelector<ComposeboxVoiceSearchElement>(
                  'cr-composebox-voice-search')
              ?.start();
        }

        onFileChange(e: CustomEvent<{files: FileList}>) {
          this.processFiles(e.detail.files);
          recordContextAdditionMethod(
              ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource);
        }

        onPaste(event: ClipboardEvent) {
          if (!this.dragAndDropEnabled || !event.clipboardData?.items) {
            return;
          }

          const dataTransfer = new DataTransfer();

          for (const item of event.clipboardData.items) {
            if (item.kind === 'file') {
              const file = item.getAsFile();
              if (file) {
                dataTransfer.items.add(file);
              }
            }
          }

          const fileList: FileList = dataTransfer.files;

          if (fileList.length > 0) {
            event.preventDefault();
            this.processFiles(fileList);
            recordContextAdditionMethod(
                ComposeboxContextAddedMethod.COPY_PASTE, this.composeboxSource);
          }
        }

        onCancelClick() {
          if (this.hasContent()) {
            this.resetModes();
            this.clearAllInputs(/* querySubmitted= */ false,
                                /* shouldBlockAutoSuggestedTabs= */ true);
            this.focusInput();
            this.queryAutocomplete(/* clearMatches= */ true);

            if (!this.disableCaretColorAnimation) {
              this.getInputElement().resetCaret();
            }
          } else {
            this.closeComposebox();
          }
        }

        onSubmitFocusin(_e: FocusEvent) {
          // Matches should always be greater than 0 due to verbatim match.
          if (this.input && !this.selectedMatch) {
            this.selectFirstMatch();
          }
        }

        onSubmitClick(e: MouseEvent) {
          if (this.hasFiles() ||
              this.inputState?.activeTool !== ToolMode.kUnspecified) {
            this.getPageHandler().notifyComposeboxQuerySubmittedWithContext();
          }
          this.submitQuery(e);
        }

        // =====================================================================
        // Common helper methods
        // =====================================================================

        addToPendingUploads(uuid: UnguessableToken) {
          this.pendingUploads.add(uuid);
          this.fileUploadsComplete = false;
        }

        closeMenu() {
          assertNotReached();
        }

        deleteFile(_uuidToDelete: UnguessableToken, _fromUserAction?: boolean) {
          assertNotReached();
        }

        focusInput() {
          this.getInputElement().inputElement.focus();
        }

        hasContent(): boolean {
          return this.inputState?.activeTool !== ToolMode.kUnspecified ||
              this.input.trim().length > 0 || this.files.size > 0;
        }

        clearInput() {
          this.input = '';
          this.lastQueriedInput = '';
          this.getDropdownElement().unselect();
        }

        clearAllInputs(
            querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean) {
          this.clearInput();
          // Let `querySubmit` handle clearing files if the tool mode is a tool
          // mode that should be cleared after submitting. For all other general
          // clearing, clear input here.
          if (!querySubmitted) {
            this.resetModes();
          }
          const undeletableFiles =
              Array.from(this.files.values()).filter(file => !file.isDeletable);
          if (undeletableFiles.length !== this.files.size) {
            this.files =
                new Map(undeletableFiles.map(file => [file.uuid, file]));
            this.addedTabsIds =
                new Map(undeletableFiles.filter(file => file.tabId)
                            .map(file => [file.tabId!, file.uuid]));
          }
          // Reset files in set to match remaining files in carousel.
          this.pendingUploads = new Set([...this.files.keys()]);
          this.smartComposeInlineHint = '';
          this.resetSmartComposeStats();
          if (!querySubmitted) {
            // If the query was submitted, the searchbox handler will clear its
            // own uploaded file state when the query submission is handled.
            this.getSearchboxHandler().clearFiles(shouldBlockAutoSuggestedTabs);
          }
          this.fileUploadsComplete = this.pendingUploads.size === 0;
          if (this.inVoiceSearchMode) {
            this.voiceSearchEndCleanup();
          }
        }

        handleProcessFilesError(error: ProcessFilesError) {
          if (error === ProcessFilesError.NONE) {
            return;
          }

          let metric = ComposeboxFileValidationError.NONE;

          switch (error) {
            case ProcessFilesError.MAX_FILES_EXCEEDED:
              metric = ComposeboxFileValidationError.TOO_MANY_FILES;
              this.errorMessage = this.i18n('maxFilesReachedError');
              break;
            case ProcessFilesError.MAX_IMAGES_EXCEEDED:
              metric = ComposeboxFileValidationError.TOO_MANY_FILES;
              this.errorMessage = this.i18n('maxImagesReachedError');
              break;
            case ProcessFilesError.MAX_PDFS_EXCEEDED:
              metric = ComposeboxFileValidationError.TOO_MANY_FILES;
              this.errorMessage = this.i18n('maxPdfsReachedError');
              break;
            case ProcessFilesError.FILE_EMPTY:
              metric = ComposeboxFileValidationError.FILE_EMPTY;
              this.errorMessage =
                  this.i18n('composeboxFileUploadInvalidEmptySize');
              break;
            case ProcessFilesError.FILE_TOO_LARGE:
              metric = ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE;
              const unitMiB = 1024 * 1024;
              this.errorMessage = this.i18n(
                  'composeboxFileUploadInvalidTooLarge',
                  Math.floor(this.maxFileSize / unitMiB));
              break;
            case ProcessFilesError.INVALID_TYPE:
              this.errorMessage = this.i18n('composeFileTypesAllowedError');
              break;
            case ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED:
              this.errorMessage = this.i18n('composeboxFileUploadNotAllowed');
              break;
            default:
              break;
          }

          this.recordFileValidationMetric(metric);
          this.closeMenu();
        }

        isFileAllowed(fileType: string): boolean {
          if (this.lensSendRawFileMediaTypesEnabled) {
            return true;
          }
          return this.isMimeTypeAllowed(fileType, this.imageFileTypes) ||
              this.isMimeTypeAllowed(fileType, this.attachmentFileTypes);
        }

        isMimeTypeAllowed(mimeType: string, allowedTypes: string[]): boolean {
          const lowerMimeType = mimeType.toLowerCase();
          return allowedTypes.some(type => {
            if (type.endsWith('/*')) {
              const prefix = type.slice(0, -1);
              return lowerMimeType.startsWith(prefix);
            }
            return lowerMimeType === type;
          });
        }

        getInputType(type: string): InputType {
          if (type === 'tab') {
            return InputType.kBrowserTab;
          }
          if (type === 'image') {
            return InputType.kLensImage;
          }
          if (type === 'pdf') {
            return InputType.kLensFile;
          }

          if (this.imageFileTypes.some(t => {
                if (t.endsWith('/*')) {
                  const prefix = t.slice(0, -1);
                  return type.startsWith(prefix);
                }
                return type === t;
              })) {
            return InputType.kLensImage;
          }

          // Arbitrary file types are treated as Lens files.
          return InputType.kLensFile;
        }

        resetModes() {
          const previousTool = this.inputState?.activeTool;
          this.uploadButtonDisabled = false;

          if (previousTool !== ToolMode.kUnspecified) {
            this.showContextMenuDescription =
                this.contextMenuDescriptionEnabled;
            this.handleToolModeUpdate(ToolMode.kUnspecified);
          }
        }

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

        closeComposebox() {
          this.resetModes();
          this.getSearchboxHandler().clearFiles(
              /*shouldBlockAutoSuggestedTabs=*/ false);
          this.resetToolsAndModels();
          this.fire('close-composebox', {composeboxText: this.input});
        }

        handleEscapeKeyLogic() {
          if (!this.closeOnEscape && this.hasContent()) {
            this.resetModes();
            this.clearAllInputs(/* querySubmitted= */ false,
                                /* shouldBlockAutoSuggestedTabs= */ false);
            this.focusInput();
            this.queryAutocomplete(/* clearMatches= */ true);
          } else {
            this.closeComposebox();
          }
        }

        submitQuery(e?: KeyboardEvent|MouseEvent) {
          if (!this.canSubmitFilesAndInput || !this.hasValidQuery()) {
            return;
          }
          // Submissions do not need a mouse or keyboard event to be submitted.
          // For example, inputs that are injected into the composebox can be
          // set to submit immediately after injection.
          const mouseButton = (e as MouseEvent)?.button ?? 0;
          const altKey = e?.altKey ?? false;
          const ctrlKey = e?.ctrlKey ?? false;
          const metaKey = e?.metaKey ?? false;
          const shiftKey = e?.shiftKey ?? false;

          // If there is a match that is selected, open that match, else follow
          // the non-autocomplete submission flow. The non-autocomplete
          // submission flow will not have omnibox metrics recorded for it.
          if (this.selectedMatchIndex >= 0) {
            const match = this.result!.matches[this.selectedMatchIndex];
            assert(match);
            this.getSearchboxHandler().setSmartComposeStats(
                this.smartComposeStats);
            this.getSearchboxHandler().openAutocompleteMatch(
                this.selectedMatchIndex, match.destinationUrl,
                /* are_matches_showing */ true, mouseButton, altKey, ctrlKey,
                metaKey, shiftKey);
          } else {
            this.getSearchboxHandler().submitQuery(
                this.input.trim(), mouseButton, altKey, ctrlKey, metaKey,
                shiftKey);
          }

          this.submitCleanup();
          // Only close the composebox when opening in a new tab because
          // doing so in the current tab causes a visual jitter where the
          // composebox closes before the new results page finishes loading.
          if (ctrlKey || metaKey || shiftKey) {
            this.closeComposebox();
          }
        }

        submitCleanup() {
          this.clearAutocompleteMatches();
          this.resetSmartComposeStats();
          this.animationState = GlowAnimationState.SUBMITTING;
          // Standard behavior: clear inputs if flag is enabled
          if (this.clearAllInputsWhenSubmittingQuery) {
            this.clearAllInputs(/* querySubmitted= */ true,
                                /* shouldBlockAutoSuggestedTabs= */ false);
          }
          this.fire('composebox-submit');
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

        resetSmartComposeStats() {
          this.smartComposeStats = {
            enabled: loadTimeData.getBoolean('composeboxSmartComposeEnabled'),
            shownCount: 0,
            acceptedCount: 0,
            charactersAccepted: 0,
            shownLength: 0,
          };
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

        recordFileValidationMetric(enumValue: ComposeboxFileValidationError) {
          recordEnumerationValue(
              'ContextualSearch.File.WebUI.UploadAttemptFailure.' +
                  this.composeboxSource,
              enumValue, ComposeboxFileValidationError.MAX_VALUE + 1);
        }

        async addFileContext(files: File[]) {
          const composeboxFiles: Map<UnguessableToken, ComposeboxFile> =
              new Map();
          for (const file of files) {
            const fileBuffer = await file.arrayBuffer();
            const bigBuffer: BigBuffer = {
              bytes: Array.from(new Uint8Array(fileBuffer)),
            };
            let token: UnguessableToken;
            try {
              token = await this.getSearchboxHandler().addFileContext(
                  {
                    fileName: file.name,
                    imageDataUrl: null,
                    mimeType: file.type,
                    isDeletable: true,
                    selectionTime: new Date(),
                  },
                  bigBuffer);
            } catch (e) {
              const err = e as ContextUploadErrorType;
              if (FILE_VALIDATION_ERRORS_MAP.has(err)) {
                this.errorMessage =
                    this.i18n(FILE_VALIDATION_ERRORS_MAP.get(err)!);
              }
              continue;
            }

            const attachment = ComposeboxFile.createFromFile(
                token, file, ContextUploadStatus.kNotUploaded, {
                  dataUrl: null,
                  objectUrl: file.type.includes('image') ?
                      URL.createObjectURL(file) :
                      null,
                  iconName: null,
                  supportsUnimodal: true,
                });
            composeboxFiles.set(token, attachment);
            const announcer = getAnnouncerInstance();
            announcer.announce(this.i18n('composeboxFileUploadStartedText'));
          }
          this.files =
              new Map([...this.files.entries(), ...composeboxFiles.entries()]);
          this.recordFileValidationMetric(ComposeboxFileValidationError.NONE);
          this.focusInput();
        }

        addFileContextFromBrowser(
            uuid: UnguessableToken, fileInfo: SelectedFileInfo) {
          const attachment: ComposeboxFile = {
            uuid: uuid,
            name: fileInfo.fileName,
            dataUrl: fileInfo.imageDataUrl ?? null,
            objectUrl: null,
            type: fileInfo.mimeType || (fileInfo.imageDataUrl ? 'image' : ''),
            inputType: fileInfo.imageDataUrl ? InputType.kLensImage :
                                               InputType.kLensFile,
            status: fileInfo.imageDataUrl ?
                ContextUploadStatus.kUploadSuccessful :
                ContextUploadStatus.kNotUploaded,
            url: null,
            tabId: null,
            isDeletable: fileInfo.isDeletable,
            iconName: null,
            supportsUnimodal: true,
          };

          this.onFileContextAdded(attachment);
        }

        addDroppedFiles(files: FileList|null) {
          this.processFiles(files);
          recordContextAdditionMethod(
              ComposeboxContextAddedMethod.DRAG_AND_DROP,
              this.composeboxSource);
        }

        addDriveUploads(driveUploads: DriveUpload[], error?: DriveUploadError) {
          // Handle any pre-existing error passed in (e.g., from the NTP)
          if (error === DriveUploadError.kMaxFilesExceeded) {
            this.handleProcessFilesError(ProcessFilesError.MAX_FILES_EXCEEDED);
          } else if (error === DriveUploadError.kSizeLimitExceeded) {
            this.handleProcessFilesError(ProcessFilesError.FILE_TOO_LARGE);
          }

          // Add the successful files to the Composebox
          const composeboxFiles: Map<UnguessableToken, ComposeboxFile> =
              new Map();
          for (const file of driveUploads) {
            const attachment = ComposeboxFile.createFromFile(
                file.token, {name: file.fileName, type: file.mimeType},
                ContextUploadStatus.kNotUploaded, {
                  dataUrl: file.thumbnailUrl ?? null,
                  objectUrl: null,
                  iconName: null,
                  supportsUnimodal: true,
                  thumbnailUrl: file.thumbnailUrl ?? null,
                });
            composeboxFiles.set(file.token, attachment);

            const announcer = getAnnouncerInstance();
            announcer.announce(this.i18n('composeboxFileUploadStartedText'));
          }

          if (composeboxFiles.size > 0) {
            this.files = new Map([
              ...this.files.entries(),
              ...composeboxFiles.entries(),
            ]);
            this.recordFileValidationMetric(ComposeboxFileValidationError.NONE);
            this.focusInput();
          }
        }

        // LINT.IfChange(getValidationError)
        getValidationError(
            inputType: InputType, fileType: string, fileSize: number,
            totalCount: number, currentTypeCount: number): ProcessFilesError {
          if (this.inputState?.activeTool === ToolMode.kDeepSearch) {
            return ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED;
          }

          if (this.inputState?.activeTool !== ToolMode.kUnspecified) {
            const disabledTypes = this.inputState?.disabledInputTypes || [];
            if (disabledTypes.includes(inputType)) {
              return ProcessFilesError.INVALID_TYPE;
            }
          }

          if (fileSize === 0 || fileSize > this.maxFileSize) {
            return fileSize === 0 ? ProcessFilesError.FILE_EMPTY :
                                    ProcessFilesError.FILE_TOO_LARGE;
          }

          if (!this.isFileAllowed(fileType)) {
            return ProcessFilesError.INVALID_TYPE;
          }

          let maxTotal = this.maxFileCount;
          if (this.inputState && this.inputState.maxTotalInputs > 0) {
            maxTotal = this.inputState.maxTotalInputs;
          }

          let maxType = maxTotal;
          if (this.inputState &&
              this.inputState.maxInputsByType[inputType] !== undefined) {
            maxType = this.inputState.maxInputsByType[inputType];
          }

          if (currentTypeCount >= maxType) {
            switch (inputType) {
              case InputType.kLensImage:
                return ProcessFilesError.MAX_IMAGES_EXCEEDED;
              case InputType.kLensFile:
                return ProcessFilesError.MAX_PDFS_EXCEEDED;
              default:
                return ProcessFilesError.MAX_FILES_EXCEEDED;
            }
          }

          if (totalCount >= maxTotal) {
            return ProcessFilesError.MAX_FILES_EXCEEDED;
          }

          return ProcessFilesError.NONE;
        }
        // LINT.ThenChange(//chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.cc:ContextualSearchboxHandler_AddFileContextFromBrowser)

        processFiles(files: FileList|null) {
          if (!files || files.length === 0) {
            return;
          }
          if (this.inputState?.activeTool === ToolMode.kDeepSearch) {
            this.handleProcessFilesError(
                ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED);
            return;
          }

          const filesToUpload: File[] = [];
          let errorToDisplay = ProcessFilesError.NONE;

          const counts = new Map<InputType, number>();
          counts.set(InputType.kLensImage, 0);
          counts.set(InputType.kLensFile, 0);
          counts.set(InputType.kBrowserTab, 0);

          for (const file of this.files.values()) {
            const type = this.getInputType(file.type);
            counts.set(type, (counts.get(type) || 0) + 1);
          }

          let totalCount = this.files.size;

          let maxTotal = this.maxFileCount;
          if (this.inputState && this.inputState.maxTotalInputs > 0) {
            maxTotal = this.inputState.maxTotalInputs;
          }

          if (totalCount + files.length > maxTotal) {
            errorToDisplay =
                Math.max(errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
          }

          for (const file of files) {
            const inputType = this.getInputType(file.type);
            const currentTypeCount = counts.get(inputType) || 0;
            const error = this.getValidationError(
                inputType, file.type, file.size, totalCount, currentTypeCount);

            if (error !== ProcessFilesError.NONE) {
              errorToDisplay = Math.max(errorToDisplay, error);
              continue;
            }

            filesToUpload.push(file);
            totalCount++;
            counts.set(inputType, currentTypeCount + 1);
          }

          if (filesToUpload.length > 0) {
            this.addFileContext(filesToUpload);
          }

          this.handleProcessFilesError(errorToDisplay);
        }

        updateFileStatus(
            token: UnguessableToken, status: ContextUploadStatus,
            errorType: ContextUploadErrorType|
            null): {file: ComposeboxFile|null, errorMessage: string|null} {
          let errorMessage = null;
          let file = this.files.get(token) ?? null;
          if (file) {
            if (isContextUploadStatusTerminal(status) &&
                status !== ContextUploadStatus.kUploadSuccessful) {
              this.files.delete(token);

              if (file.tabId) {
                this.addedTabsIds =
                    new Map([...this.addedTabsIds.entries()].filter(
                        ([id, _]) => id !== file!.tabId));
              }
              switch (status) {
                case ContextUploadStatus.kValidationFailed:
                  if (errorType) {
                    errorMessage = this.i18n(
                        FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                        'composeboxFileUploadValidationFailed');
                  } else {
                    errorMessage =
                        this.i18n('composeboxFileUploadValidationFailed');
                  }
                  break;
                case ContextUploadStatus.kUploadFailed:
                  errorMessage = this.i18n('composeboxFileUploadFailed');
                  break;
                case ContextUploadStatus.kUploadExpired:
                  errorMessage = this.i18n('composeboxFileUploadExpired');
                  break;
                case ContextUploadStatus.kUploadReplaced:
                  // Update `composebox.ts` with the status since
                  // this should not return an error message for this
                  // 'non-uploaded' terminal file state, meaning
                  // its file status is still needed for understanding state
                  // when returned and back in the context of the function
                  // caller.
                  file = {...file, status: status};
                  break;
                default:
                  break;
              }
              this.closeMenu();
            } else {
              file = {...file, status: status};
              this.files.set(token, file);
            }
            this.files = new Map([...this.files]);
          } else {
            // File is unknown but its status is known. Show this if
            // ghost/unknown files in frontend are allowed to be in
            // carousel.
            if (this.shouldShowGhostFiles) {
              file = {
                uuid: token,
                name: '',
                objectUrl: null,
                dataUrl: null,
                type: '',
                inputType: InputType.kLensFile,
                // Override this since first upload status is this or
                // processing. Need this or processing in order to show tab
                // spinner.
                status: ContextUploadStatus.kUploadStarted,
                url: null,
                tabId: null,
                isDeletable: true,
                iconName: null,
                supportsUnimodal: true,
              };
              // Update pending uploads in 'composebox.ts' to disable
              // submit button.
              this.onFileContextAdded(file);
            }
          }
          return {file, errorMessage};
        }

        async refreshTabSuggestions() {
          if (!this.contextMenuOpened) {
            return;
          }
          const {tabs} = await this.getSearchboxHandler().getRecentTabs();
          this.tabSuggestions = [...tabs];

          if (this.inputState) {
            const {allowedInputTypes, disabledInputTypes} = this.inputState;
            if (allowedInputTypes.includes(InputType.kBrowserTab) &&
                !disabledInputTypes.includes(InputType.kBrowserTab)) {
              // Get the set of IDs of tabs currently added as context.
              const addedTabIdsSet = new Set(this.addedTabsIds.keys());

              // Filter out suggestions that are already added as context.
              const filteredSuggestions = this.tabSuggestions.filter(
                  tab => !addedTabIdsSet.has(tab.tabId));

              if (filteredSuggestions.length > 0) {
                recordInputTypeShown(
                    InputType.kBrowserTab, this.composeboxSource, 'AimPopup');
              }
            }
          }
        }

        async onGetTabPreview(e: CustomEvent<{
          tabId: number,
          onPreviewFetched: (previewDataUrl: string) => void,
        }>) {
          const {previewDataUrl} =
              await this.getSearchboxHandler().getTabPreview(e.detail.tabId);
          e.detail.onPreviewFetched(previewDataUrl || '');
        }

        voiceSearchEndCleanup() {
          this.inVoiceSearchMode = false;
          this.animationState = GlowAnimationState.NONE;
          this.transcript = '';
        }

        onVoiceSearchFinalResult(e: CustomEvent<string>) {
          e.stopPropagation();
          this.voiceSearchEndCleanup();
          // For contextual tasks composebox voice metrics.
          // TODO(crbug.com/466412331): Don't only fire this for composebox,
          // this should be recorded for all.
          this.fire('composebox-voice-search-transcription-success');
          // TODO(crbug.com/466412331): Remove, only recorded for the NTP.
          this.fire(
              'voice-search-action',
              {value: VoiceSearchAction.QUERY_SUBMITTED});
          this.input = e.detail;
          const metricName = `ContextualSearch.UserAction.SubmitVoiceQuery.${
              this.composeboxSource}`;
          recordUserAction(metricName);
          recordBoolean(metricName, true);
          this.getSearchboxHandler().submitQuery(
              e.detail, /*mouse_button=*/ 0, /*alt_key=*/ false,
              /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false);
          this.submitCleanup();
        }

        onVoiceSearchCancel(e: CustomEvent<boolean>) {
          // If closing was the user canceling voice search:
          if (e.detail) {
            // For contextual tasks composebox voice metrics.
            this.fire('composebox-voice-search-user-canceled');
          }
          this.voiceSearchEndCleanup();
          this.receivedSpeech = false;
        }

        onVoiceSearchError(e: CustomEvent<boolean>) {
          this.hasVoiceSearchError = true;
          // For contextual tasks composebox voice metrics:
          if (e.detail) {
            // An error that canceled voice search.
            this.fire('composebox-voice-search-error-and-canceled');
          } else {
            // An error that did not cancel voice search.
            this.fire('composebox-voice-search-error');
          }
          // Do not call `voiceSearchEndCleanup()` here since
          // error scrim should stay open, or close itself based on timer.
          // When scrim does that, it will call `onVoiceSearchCancel` in this
          // file.
        }

        shouldShowVoiceSearch(): boolean {
          return this.showVoiceSearch &&
              WindowProxy.getInstance().hasWebkitSpeechRecognition();
        }

        shouldShowVoiceSearchAnimation(): boolean {
          return !this.disableVoiceSearchAnimation &&
              this.shouldShowVoiceSearch();
        }

        shouldShowVoiceSearchAtBottom(): boolean {
          return (this.searchboxLayoutMode === 'TallBottomContext' ||
                  !this.searchboxLayoutMode) &&
              this.shouldShowVoiceSearch();
        }

        shouldShowDivider(): boolean {
          return this.showDropdown &&
              (this.showFileCarousel || this.shouldShowSubmitButton() ||
               this.inToolMode);
        }

        shouldShowSubmitButton(): boolean {
          return this.searchboxNextEnabled && this.submitEnabled;
        }

        computeShowDropdown() {
          // Don't show dropdown if there's multiple files.
          if (this.files.size > 1) {
            return false;
          }

          // Don't show dropdown if there's no results.
          if (!this.result?.matches.length) {
            return false;
          }

          // Don't show dropdown if there's only verbatim match.
          if (this.result?.matches.length === 1 &&
              this.result?.matches[0]?.allowedToBeDefaultMatch) {
            return false;
          }

          // Do not show dropdown if there's an error scrim.
          if (this.errorMessage !== '') {
            return false;
          }

          // Do not show dropdown if there's an image and contextual image
          // suggestions are disabled.
          if (!this.enableImageContextualSuggestions && this.hasImageFiles()) {
            return false;
          }

          if (this.showTypedSuggest && this.lastQueriedInput.trim()) {
            // If context is present, but not enabled, continue to avoid showing
            // the dropdown.
            if (!this.showTypedSuggestWithContext && this.files.size > 0) {
              return false;
            }
            // Do not show the dropdown for multiline input or if only the
            // verbatim match is present (we always expect a verbatim match for
            // typed suggest, so we ensure the length of the matches is >1).
            if (this.getInputElement().inputElement.scrollHeight <= 48 &&
                this.result?.matches.length > 1) {
              return true;
            }
          }

          // lastQueriedInput is used here since the input changes based on
          // the selected match. If typed suggest is not enabled and input is
          // used, the dropdown will hide if the user keys down over zps
          // matches.
          return this.showZps && !this.lastQueriedInput;
        }
      }

      return ComposeboxEmbedderMixin;
    };

export interface ComposeboxEmbedderMixinInterface extends
    I18nMixinLitInterface {
  addedTabsIds: Map<number, UnguessableToken>;
  animationState: GlowAnimationState;
  automaticActiveTab: ComposeboxFile|null;
  disableCaretColorAnimation: boolean;
  disableVoiceSearchAnimation: boolean;
  isDraggingFile: boolean;
  enableImageContextualSuggestions: boolean;
  smartComposeEnabled: boolean;
  smartTabSharingActive: boolean;
  contextMenuDescriptionEnabled: boolean;
  showContextMenuDescription: boolean;
  shouldShowGhostFiles: boolean;
  showMenuOnClick: boolean;
  isCanvasQuerySubmitted: boolean;
  browserTabContextAdded: boolean;
  pendingUploads: Set<UnguessableToken>;
  dragAndDropEnabled: boolean;
  composeboxSource: string;
  maxFileCount: number;
  maxFileSize: number;
  attachmentFileTypes: string[];
  imageFileTypes: string[];
  showTypedSuggestWithContext: boolean;
  queryZpsOnLoad: boolean;
  closeOnEscape: boolean;
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
  searchboxLayoutMode: string;
  searchboxNextEnabled: boolean;
  selectedMatch: AutocompleteMatch|null;
  selectedMatchIndex: number;
  showDropdown: boolean;
  dropdownNeeded: boolean;
  showFileCarousel: boolean;
  usePecApi: boolean;
  showZps: boolean;
  showVoiceSearch: boolean;
  smartComposeInlineHint: string;
  smartComposeStats: SmartComposeStats;
  state: ComposeboxState|null;
  submitEnabled: boolean;
  tabSuggestions: TabInfo[];
  transcript: string;
  uploadButtonDisabled: boolean;
  composeboxNoFlickerSuggestionsFix: boolean;
  searchboxListenerIds: number[];
  showTypedSuggest: boolean;
  lastQueriedInput: string;
  haveReceivedAutcompleteResponse: boolean;
  lensSendRawFileMediaTypesEnabled: boolean;
  hasVoiceSearchError: boolean;
  isListening: boolean;

  // Embedder-provided methods for DOM and Mojo access
  updateInputPlaceholder(): void;
  deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean): void;
  closeMenu(): void;
  closeComposebox(): void;
  submitQuery(e?: KeyboardEvent|MouseEvent): void;
  submitCleanup(): void;
  getInputElement(): ComposeboxInputElement;
  getDropdownElement(): ComposeboxDropdownElement;
  getActiveElement(): Element|null;
  getPageHandler(): PageHandlerRemote;
  getSearchboxCallbackRouter(): SearchboxPageCallbackRouter;
  getSearchboxHandler(): SearchboxPageHandlerRemote;
  addTabContextHandleCallback(
      tabUpload: TabUpload, replaceAutoActiveTabToken?: boolean): Promise<void>;

  // Common event handlers
  onContextMenuContainerMousedown(e: FocusEvent): void;
  onContextMenuContainerClick(e: MouseEvent): void;
  onDeleteTabContext(
      e: CustomEvent<{uuid: UnguessableToken, fromUserAction?: boolean}>): void;
  onAddTabContext(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>): void;
  onContextMenuClosed(): Promise<void>;
  onContextMenuOpened(): void;
  onVoiceSearchButtonClick(): void;
  onFileContextAdded(file: ComposeboxFile): void;
  voiceSearchEndCleanup(): void;
  onVoiceSearchFinalResult(e: CustomEvent<string>): void;
  onVoiceSearchCancel(e: CustomEvent<boolean>): void;
  onVoiceSearchError(e: CustomEvent<boolean>): void;
  shouldShowVoiceSearch(): boolean;
  shouldShowVoiceSearchAnimation(): boolean;
  shouldShowVoiceSearchAtBottom(): boolean;
  onTranscriptUpdate(e: CustomEvent<string>): void;
  onSpeechReceived(): void;
  onDismissErrorScrim(): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  onMatchClick(
      e: CustomEvent<{ctrlKey: boolean, metaKey: boolean, shiftKey: boolean}>):
      void;
  onMatchFocusin(e: CustomEvent<{index: number}>): void;
  onInputStateChanged(inputState: InputState): void;
  onAutocompleteResultChanged(_result: AutocompleteResult): void;
  onContextualInputStatusChanged(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|null): void;
  onInputInput(e: CustomEvent<Event>): void;
  onInputFocusin(): void;
  onRecordingStopped(e: CustomEvent<string>): void;
  onKeydown(e: KeyboardEvent): void;
  handleEscapeKeyLogic(): void;
  isTogglingOff(tool: ToolMode): boolean;
  onToolClick(e: CustomEvent<{toolMode: ToolMode}>): void;
  handleToolClick(tool: ToolMode): void;
  handleToolModeUpdate(newTool: ToolMode): void;
  onModelClick(e: CustomEvent<{model: ModelMode}>): void;
  onOpenImageUpload(): void;
  onOpenFileUpload(): void;
  onOpenDriveUpload(): void;
  onSmartTabSharingActiveChanged(e: CustomEvent<{active: boolean}>): void;
  onFileChange(e: CustomEvent<{files: FileList}>): void;
  onPaste(event: ClipboardEvent): void;
  onCancelClick(): void;
  onSubmitFocusin(e: FocusEvent): void;
  onSubmitClick(e: MouseEvent): void;

  // Common helper methods
  addToPendingUploads(token: UnguessableToken): void;
  focusInput(): void;
  hasContent(): boolean;
  clearInput(): void;
  clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean): void;
  handleProcessFilesError(error: ProcessFilesError): void;
  getValidationError(
      inputType: InputType, fileType: string, fileSize: number,
      totalCount: number, currentTypeCount: number): ProcessFilesError;
  isFileAllowed(fileType: string): boolean;
  isMimeTypeAllowed(mimeType: string, allowedTypes: string[]): boolean;
  getInputType(type: string): InputType;
  resetModes(): void;
  setDefaultModel(): void;
  resetToolsAndModels(): void;
  closeDropdown(): void;
  hasImageFiles(): boolean;
  hasMatches(): boolean;
  selectFirstMatch(): void;
  hasFiles(): boolean;
  resetSmartComposeStats(): void;
  queryAutocomplete(clearMatches: boolean): void;
  clearAutocompleteMatches(): void;
  computeSubmitEnabled(): boolean;
  hasValidQuery(): boolean;
  recordFileValidationMetric(enumValue: ComposeboxFileValidationError): void;
  addFileContext(files: File[]): Promise<void>;
  addFileContextFromBrowser(uuid: UnguessableToken, fileInfo: SelectedFileInfo):
      void;
  addDroppedFiles(files: FileList|null): void;
  addDriveUploads(driveUploads: DriveUpload[], error?: DriveUploadError): void;
  processFiles(files: FileList|null): void;
  updateFileStatus(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|
      null): {file: ComposeboxFile|null, errorMessage: string|null};
  refreshTabSuggestions(): Promise<void>;
  onGetTabPreview(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>): Promise<void>;
  shouldShowDivider(): boolean;
  shouldShowSubmitButton(): boolean;
  computeShowDropdown(): boolean;
}
