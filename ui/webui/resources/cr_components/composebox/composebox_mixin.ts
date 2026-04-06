// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import type {I18nMixinLitInterface} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageHandlerRemote as SearchboxPageHandlerRemote, SelectedFileInfo, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ComposeboxFile, ComposeboxFileValidationError, FILE_VALIDATION_ERRORS_MAP, getLoadTimeBoolean, isContextUploadStatusTerminal, ProcessFilesError, recordEnumerationValue} from './common.js';
import type {ComposeboxState} from './common.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ContextUploadStatus, InputType, ModelMode, ToolMode} from './composebox_query.mojom-webui.js';
import type {ContextUploadErrorType, InputState} from './composebox_query.mojom-webui.js';

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

        closeMenu() {
          assertNotReached();
        }

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

        // This function is called when backend starts a file upload flow,
        // whether through `addFileFromAttachment_`,
        // `addFileContextFromBrowser`, etc. This contrasts with the workflows
        // where the frontend starts a file upload flow
        // (`addFileContext`).
        onFileContextAdded(file: ComposeboxFile) {
          const newFiles = new Map(this.files);
          newFiles.set(file.uuid, file);
          this.files = newFiles;
          this.addToPendingUploads(file.uuid);
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

        /**
         * @param e Event containing index of the match that received focus.
         */
        onMatchFocusin(e: CustomEvent<{index: number}>) {
          // Select the match that received focus.
          this.getDropdownElement().selectIndex(e.detail.index);
        }

        onInputInput(_e: CustomEvent<Event>) {
          this.input = this.getInputElement().input;

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

        // =====================================================================
        // Common helper methods
        // =====================================================================

        addToPendingUploads(uuid: UnguessableToken) {
          this.pendingUploads.add(uuid);
          this.fileUploadsComplete = false;
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
              this.errorMessage =
                  this.i18n('composeboxFileUploadInvalidTooLarge');
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
            if (this.inputState?.activeTool !== ToolMode.kUnspecified) {
              const disabledTypes = this.inputState?.disabledInputTypes || [];
              if (disabledTypes.includes(inputType)) {
                errorToDisplay =
                    Math.max(errorToDisplay, ProcessFilesError.INVALID_TYPE);
                continue;
              }
            }

            if (file.size === 0 || file.size > this.maxFileSize) {
              const sizeError = file.size === 0 ?
                  ProcessFilesError.FILE_EMPTY :
                  ProcessFilesError.FILE_TOO_LARGE;
              errorToDisplay = Math.max(errorToDisplay, sizeError);
              continue;
            }

            if (!this.isFileAllowed(file.type)) {
              errorToDisplay =
                  Math.max(errorToDisplay, ProcessFilesError.INVALID_TYPE);
              continue;
            }

            let maxType = maxTotal;
            if (this.inputState &&
                this.inputState.maxInputsByType[inputType] !== undefined) {
              maxType = this.inputState.maxInputsByType[inputType];
            }

            const currentTypeCount = counts.get(inputType) || 0;

            if (totalCount < maxTotal && currentTypeCount < maxType) {
              filesToUpload.push(file);
              totalCount++;
              counts.set(inputType, currentTypeCount + 1);
            } else {
              if (currentTypeCount >= maxType) {
                switch (inputType) {
                  case InputType.kLensImage:
                    errorToDisplay = Math.max(
                        errorToDisplay, ProcessFilesError.MAX_IMAGES_EXCEEDED);
                    break;
                  case InputType.kLensFile:
                    errorToDisplay = Math.max(
                        errorToDisplay, ProcessFilesError.MAX_PDFS_EXCEEDED);
                    break;
                  default:
                    errorToDisplay = Math.max(
                        errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
                }
              } else {
                errorToDisplay = Math.max(
                    errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
              }
            }
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
      }

      return ComposeboxEmbedderMixin;
    };

export interface ComposeboxEmbedderMixinInterface extends
    I18nMixinLitInterface {
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
  closeMenu(): void;
  getInputElement(): ComposeboxInputElement;
  getDropdownElement(): ComposeboxDropdownElement;
  getActiveElement(): Element|null;
  getPageHandler(): PageHandlerRemote;
  getSearchboxHandler(): SearchboxPageHandlerRemote;

  // Common event handlers
  onFileContextAdded(file: ComposeboxFile): void;
  onTranscriptUpdate(e: CustomEvent<string>): void;
  onSpeechReceived(): void;
  onDismissErrorScrim(): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  onMatchFocusin(e: CustomEvent<{index: number}>): void;
  onInputInput(e: CustomEvent<Event>): void;
  onInputFocusin(): void;

  // Common helper methods
  addToPendingUploads(token: UnguessableToken): void;
  focusInput(): void;
  hasContent(): boolean;
  clearInput(): void;
  handleProcessFilesError(error: ProcessFilesError): void;
  isFileAllowed(fileType: string): boolean;
  isMimeTypeAllowed(mimeType: string, allowedTypes: string[]): boolean;
  getInputType(type: string): InputType;
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
  recordFileValidationMetric(enumValue: ComposeboxFileValidationError): void;
  addFileContext(files: File[]): Promise<void>;
  addFileContextFromBrowser(uuid: UnguessableToken, fileInfo: SelectedFileInfo):
      void;
  processFiles(files: FileList|null): void;
  updateFileStatus(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|
      null): {file: ComposeboxFile|null, errorMessage: string|null};
}
