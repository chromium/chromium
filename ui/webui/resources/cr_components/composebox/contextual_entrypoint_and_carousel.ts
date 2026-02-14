// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox_tool_chip.js';
import './context_menu_entrypoint.js';
import './contextual_entrypoint_button.js';
import './composebox_lens_search.js';
import './file_carousel.js';
import './file_thumbnail.js';
import './icons.html.js';
import './recent_tab_chip.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {FileAttachment, SearchContext, TabAttachment, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile, ContextualUpload} from './common.js';
import {GlifAnimationState, recordBoolean, recordContextAdditionMethod, recordEnumerationValue, recordUserAction, TabUploadOrigin} from './common.js';
import {FileUploadErrorType, FileUploadStatus, InputType, ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ContextMenuEntrypointElement} from './context_menu_entrypoint.js';
import {getCss} from './contextual_entrypoint_and_carousel.css.js';
import {getHtml} from './contextual_entrypoint_and_carousel.html.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';
import type {RecentTabChipElement} from './recent_tab_chip.js';

export interface ContextualEntrypointAndCarouselElement {
  $: {
    fileInput: HTMLInputElement,
    fileUploadButton: CrIconButtonElement,
    contextEntrypoint: ContextMenuEntrypointElement|
    ContextualEntrypointButtonElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: CrIconButtonElement,
    recentTabChip: RecentTabChipElement,
    voiceSearchButton: CrIconButtonElement,
  };
}

const FILE_VALIDATION_ERRORS_MAP = new Map<FileUploadErrorType, string>([
  [
    FileUploadErrorType.kImageProcessingError,
    'composeFileTypesAllowedError',
  ],
  [
    FileUploadErrorType.kUnknown,
    'composeboxFileUploadValidationFailed',
  ],
]);

// LINT.IfChange(FileValidationError)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
const enum ComposeboxFileValidationError {
  NONE = 0,
  TOO_MANY_FILES = 1,
  FILE_EMPTY = 2,
  FILE_SIZE_TOO_LARGE = 3,
  MAX_VALUE = FILE_SIZE_TOO_LARGE,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:FileValidationError)

// These values are sorted by precedence. The error with the highest value
// will be the one shown to the user if multiple errors apply.
enum ProcessFilesError {
  NONE = 0,
  INVALID_TYPE = 1,
  FILE_TOO_LARGE = 2,
  FILE_EMPTY = 3,
  MAX_FILES_EXCEEDED = 4,
  MAX_IMAGES_EXCEEDED = 5,
  MAX_PDFS_EXCEEDED = 6,
}


export class ContextualEntrypointAndCarouselElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'contextual-entrypoint-and-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      showDropdown: {type: Boolean},
      showLensSearchChip: {reflect: true, type: Boolean},
      searchboxLayoutMode: {type: String},
      tabSuggestions: {type: Array},
      showMenuOnClick: {type: Boolean},
      entrypointName: {type: String},
      showVoiceSearch: {
        reflect: true,
        type: Boolean,
      },
      inputState: {type: Object},
      contextMenuGlifAnimationState: {type: String, reflect: true},
      // Determines if the entrypoint button should be hidden. This applies
      // specifically to Omnibox Searchbox in compact mode, as opposed to the
      // AIM composebox where the entrypoint is always visible.
      hideEntrypointButton: {type: Boolean},
      inComposebox: {type: Boolean},
      showModelPicker: {type: Boolean},
      fileUploadsComplete: {
        type: Boolean,
        reflect: true,
      },

      // =========================================================================
      // Protected properties
      // =========================================================================
      attachmentFileTypes_: {type: Array},
      contextMenuEnabled_: {type: Boolean},
      files_: {type: Object},
      pendingFiles_: {type: Object},
      addedTabsIds_: {type: Object},
      imageFileTypes_: {type: Array},
      composeboxShowPdfUpload_: {
        reflect: true,
        type: Boolean,
      },
      showContextMenuDescription_: {type: Boolean},
      showFileCarousel_: {
        reflect: true,
        type: Boolean,
      },
      showRecentTabChip: {type: Boolean},
      activeTool_: {type: Number},
      recentTabForChip_: {type: Object},
      carouselOnTop_: {type: Boolean},
      submitButtonShown: {type: Boolean},
      isOmniboxInCompactMode_: {
        type: Boolean,
        reflect: true,
      },
      uploadButtonDisabled_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor fileUploadsComplete: boolean = true;
  accessor showDropdown: boolean = false;
  accessor showLensSearchChip: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor showMenuOnClick: boolean = true;
  accessor entrypointName: string = '';
  accessor tabSuggestions: TabInfo[] = [];
  accessor carouselOnTop_: boolean = false;
  accessor showVoiceSearch: boolean = false;
  accessor showRecentTabChip: boolean = false;
  accessor inputState: InputState|null = null;
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor inComposebox: boolean = false;
  accessor showModelPicker: boolean = false;
  accessor isOmniboxInCompactMode_: boolean = false;

  protected accessor attachmentFileTypes_: string[] =
      loadTimeData.getString('composeboxAttachmentFileTypes').split(',');
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor addedTabsIds_: Map<number, UnguessableToken> = new Map();
  protected accessor pendingFiles_: Map<UnguessableToken, FileUploadStatus> =
      new Map();
  protected accessor imageFileTypes_: string[] =
      loadTimeData.getString('composeboxImageFileTypes').split(',');
  protected accessor uploadButtonDisabled_: boolean = false;
  protected accessor composeboxShowPdfUpload_: boolean =
      loadTimeData.getBoolean('composeboxShowPdfUpload');
  protected contextMenuDescriptionEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  protected accessor showContextMenuDescription_: boolean =
      this.contextMenuDescriptionEnabled_;
  protected accessor showFileCarousel_: boolean = false;
  protected accessor activeTool_: ComposeboxToolMode =
      ComposeboxToolMode.kUnspecified;
  protected accessor recentTabForChip_: TabInfo|null = null;
  protected accessor submitButtonShown: boolean = false;
  protected accessor hideEntrypointButton: boolean = false;

  computeUploadButtonDisabled(): boolean {
    // If only 1 image is uploaded and the create image tool is enabled, we
    // don't want to disable the context menu entrypoint because the user
    // should still be able to use the tool within the context menu.
    const isCreateImageToolAvailableWithImages = this.createImageModeEnabled_ &&
        this.hasImageFiles() && this.files_.size === 1;
    // Only return true if:
    //   1. The max number of files is reached, and the create image tool button
    //      is not available.
    //   2. The user has an image uploaded and is in create image mode.
    //   3. The user is in deep search mode.
    return (this.activeTool_ === ComposeboxToolMode.kDeepSearch) ||
        (this.files_.size >= this.maxFileCount_ &&
         !isCreateImageToolAvailableWithImages) ||
        (this.hasImageFiles() &&
         this.activeTool_ === ComposeboxToolMode.kImageGen) ||
        !this.fileUploadsComplete;
  }

  hasAutomaticActiveTabChipToken(): boolean {
    return this.automaticActiveTabChipToken_ !== null;
  }

  getAutomaticActiveTabChipElement(): HTMLElement|null {
    if (!this.automaticActiveTabChipToken_) {
      return null;
    }
    const carousel =
        this.shadowRoot?.querySelector<ComposeboxFileCarouselElement>(
            '#carousel');
    if (!carousel) {
      return null;
    }

    return carousel.getThumbnailElementByUuid(
        this.automaticActiveTabChipToken_);
  }

  protected get inToolMode_(): boolean {
    return this.activeTool_ !== ComposeboxToolMode.kUnspecified;
  }

  private shouldShowContextualSearchChips_(): boolean {
    return this.files_.size === 0 && !this.inToolMode_ &&
        !(this.searchboxLayoutMode === 'Compact' &&
          this.entrypointName === 'Realbox');
  }

  protected get shouldShowRecentTabChip_(): boolean {
    const isBrowserTabAllowed = !this.showModelPicker ||
        (!!this.inputState &&
         this.inputState.allowedInputTypes.includes(InputType.kBrowserTab));
    return this.shouldShowContextualSearchChips_() &&
        !!this.recentTabForChip_ && this.showRecentTabChip &&
        isBrowserTabAllowed;
  }

  protected get shouldShowLensSearchChip_(): boolean {
    return this.shouldShowContextualSearchChips_() && this.showLensSearchChip;
  }

  protected get shouldShowContextualChipsForCompactMode_(): boolean {
    return this.searchboxLayoutMode === 'Compact' &&
        (this.shouldShowRecentTabChip_ || this.shouldShowLensSearchChip_);
  }

  protected get shouldShowToolChipsForTallMode_(): boolean {
    // TODO(b/476405347): Consolidate logic here and remove the Omnibox specific
    // code.
    if (this.entrypointName === 'Omnibox') {
      return !this.shouldShowToolChipsForCompactMode_;
    }
    return this.searchboxLayoutMode !== 'Compact' ||
        this.shouldShowContextualChipsForCompactMode_;
  }

  protected get toolChipsVisible_(): boolean {
    return this.shouldShowRecentTabChip_ || this.shouldShowLensSearchChip_ ||
        this.inToolMode_;
  }

  protected get shouldShowToolChipsForCompactMode_(): boolean {
    if (this.searchboxLayoutMode !== 'Compact' || !this.toolChipsVisible_) {
      return false;
    }

    return this.entrypointName !== 'Omnibox' || this.inComposebox;
  }

  protected get shouldShowDivider_(): boolean {
    // TODO(b/476175193): Remove `entrypointName` condition.
    if (this.entrypointName === 'Omnibox' &&
        this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel_) {
      return false;
    }

    // TODO(b/476405347): Remove `entrypointName` condition.
    // `this.shouldShowContextualChipsForCompactMode_` can possibly be removed
    // without consequence.
    return this.showDropdown &&
        ((this.entrypointName !== 'Omnibox' &&
          this.shouldShowContextualChipsForCompactMode_) ||
         this.showFileCarousel_ ||
         this.searchboxLayoutMode === 'TallTopContext' ||
         this.submitButtonShown);
  }

  protected get shouldHideEntrypointButton_(): boolean {
    return this.shouldShowContextualChipsForCompactMode_ ||
        this.hideEntrypointButton;
  }

  protected shouldShowDescription_(): boolean {
    return this.showContextMenuDescription_ && !this.shouldShowRecentTabChip_ &&
        !this.shouldShowLensSearchChip_;
  }

  protected getToolChipLabel_(tool: ComposeboxToolMode): string {
    if (this.inputState && this.inputState.toolConfigs) {
      const config = this.inputState.toolConfigs.find(c => c.tool === tool);
      if (config && config.chipLabel) {
        return config.chipLabel;
      }
    }
    // Fallback to i18n strings
    switch (tool) {
      case ComposeboxToolMode.kDeepSearch:
        return this.i18n('deepSearch');
      case ComposeboxToolMode.kImageGen:
        return this.i18n('createImages');
      case ComposeboxToolMode.kCanvas:
        return this.i18n('canvas');
      default:
        return '';
    }
  }

  // TODO(b/481755889): Move property declarations above methods and remove
  // getters.
  private eventTracker_: EventTracker = new EventTracker();
  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private createImageModeEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowCreateImageButton');
  private composeboxSource_: string =
      loadTimeData.getString('composeboxSource');
  private automaticActiveTabChipToken_: UnguessableToken|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        window.matchMedia('(width <= 264px)'), 'change',
        (e: MediaQueryListEvent) => {
          this.showContextMenuDescription_ =
              !e.matches && this.contextMenuDescriptionEnabled_;
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('fileUploadsComplete')) {
      this.uploadButtonDisabled_ = this.computeUploadButtonDisabled();
    }
    if (changedPrivateProperties.has('files_') ||
        changedPrivateProperties.has(`activeTool_`)) {
      // `uploadButtonDisabled_` decides whether or not the context menu
      // entrypoint is shown to the user.
      this.uploadButtonDisabled_ = this.computeUploadButtonDisabled();
      this.showFileCarousel_ = this.files_.size > 0;
      this.fire('on-context-files-changed', {files: this.files_.size});
    }

    if (changedProperties.has('tabSuggestions')) {
      this.recentTabForChip_ =
          this.tabSuggestions.find(tab => tab.showInCurrentTabChip) || null;
      if (!this.recentTabForChip_) {
        this.recentTabForChip_ =
            this.tabSuggestions.find(tab => tab.showInPreviousTabChip) || null;
      }
    }

    if (changedProperties.has('entrypointName') ||
        changedProperties.has('searchboxLayoutMode')) {
      this.isOmniboxInCompactMode_ = this.entrypointName === 'Omnibox' &&
          this.searchboxLayoutMode === 'Compact';
    }
  }

  addDroppedFiles(files: FileList|null) {
    this.processFiles_(files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.DRAG_AND_DROP, this.composeboxSource_);
  }

  addPastedFiles(files: FileList|null) {
    this.processFiles_(files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.COPY_PASTE, this.composeboxSource_);
  }

  blurEntrypoint() {
    this.$.contextEntrypoint.blur();
  }

  closeMenu() {
    if (this.contextMenuEnabled_) {
      this.$.contextEntrypoint.closeMenu();
    }
  }

  setContextFiles(files: ContextualUpload[]) {
    for (const file of files) {
      if ('tabId' in file) {
        // If the composebox is being initialized with tab context from the
        // context menu, we want to keep the context menu open to allow for
        // multi-tab selection.
        if (this.contextMenuEnabled_ &&
            file.origin === TabUploadOrigin.CONTEXT_MENU) {
          this.$.contextEntrypoint.openMenuForMultiSelection();
        }
        this.addTabContext_(new CustomEvent('addTabContext', {
          detail: {
            id: file.tabId,
            title: file.title,
            url: file.url,
            delayUpload: file.delayUpload,
            replaceAutoActiveTabToken: false,
            origin: file.origin,
          },
        }));
      } else {
        this.addFileContext_([file.file]);
      }
    }
  }

  setInitialMode(mode: ComposeboxToolMode) {
    if (mode !== ComposeboxToolMode.kUnspecified) {
      this.handleToolClick_(mode);
    }
  }

  updateFileStatus(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType) {
    let errorMessage = null;
    let file = this.files_.get(token);
    if (file) {
      if ([
            FileUploadStatus.kValidationFailed,
            FileUploadStatus.kUploadFailed,
            FileUploadStatus.kUploadExpired,
            FileUploadStatus.kUploadReplaced,
          ].includes(status)) {
        this.files_.delete(token);
        if (file.tabId) {
          this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
              ([id, _]) => id !== file!.tabId));
        }
        switch (status) {
          case FileUploadStatus.kValidationFailed:
            errorMessage = this.i18n(
                FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                'composeboxFileUploadValidationFailed');
            break;
          case FileUploadStatus.kUploadFailed:
            errorMessage = this.i18n('composeboxFileUploadFailed');
            break;
          case FileUploadStatus.kUploadExpired:
            errorMessage = this.i18n('composeboxFileUploadExpired');
            break;
          default:
            break;
        }
        if (this.contextMenuEnabled_) {
          this.$.contextEntrypoint.closeMenu();
        }
      } else {
        file = {...file, status: status};
        this.files_.set(token, file);
      }
      this.files_ = new Map([...this.files_]);
    } else {
      this.pendingFiles_.set(token, status);
    }
    return {file, errorMessage};
  }

  resetContextFiles(): string[] {
    // Only keep files that are not deletable. Return remaining files IDs.
    const undeletableFiles =
        Array.from(this.files_.values()).filter(file => !file.isDeletable);

    if (undeletableFiles.length === this.files_.size) {
      return [...this.files_.keys()];
    }

    this.files_ = new Map(undeletableFiles.map(file => [file.uuid, file]));
    this.addedTabsIds_ = new Map(undeletableFiles.filter(file => file.tabId)
                                     .map(file => [file.tabId!, file.uuid]));
    return [...this.files_.keys()];
  }

  resetModes() {
    const previousTool = this.activeTool_;
    this.activeTool_ = ComposeboxToolMode.kUnspecified;
    this.uploadButtonDisabled_ = false;

    if (previousTool !== ComposeboxToolMode.kUnspecified) {
      this.showContextMenuDescription_ = this.contextMenuDescriptionEnabled_;
    }

    if (previousTool !== ComposeboxToolMode.kUnspecified) {
      this.fire('set-tool-mode', {
        tool: previousTool,
        enabled: false,
      });
    }
  }

  hasImageFiles() {
    if (this.files_) {
      for (const file of this.files_.values()) {
        if (file.type.includes('image')) {
          return true;
        }
      }
    }
    return false;
  }

  hasTabFile() {
    if (this.files_) {
      for (const file of this.files_.values()) {
        if (file.type === 'tab') {
          return true;
        }
      }
    }
    return false;
  }

  hasDeletableFiles() {
    return Array.from(this.files_.values()).some(file => file.isDeletable);
  }

  onFileContextAdded(file: ComposeboxFile) {
    const newFiles = new Map(this.files_);
    newFiles.set(file.uuid, file);
    this.files_ = newFiles;
  }

  updateAutoActiveTabContext(tab: TabInfo|null) {
    // If there is already a suggested tab context, remove it.
    if (this.automaticActiveTabChipToken_) {
      this.onDeleteFile_(new CustomEvent('deleteTabContext', {
        detail: {
          uuid: this.automaticActiveTabChipToken_,
        },
      }));
      this.automaticActiveTabChipToken_ = null;
    }

    if (!tab) {
      return;  // No new tab to add, so we're done.
    }

    this.addTabContext_(new CustomEvent('addTabContext', {
      detail: {
        id: tab.tabId,
        title: tab.title,
        url: tab.url,
        delayUpload: /*delay_upload=*/ true,
        replaceAutoActiveTabToken: true,
        origin: TabUploadOrigin.OTHER,
      },
    }));
  }

  private isMimeTypeAllowed_(
      mimeType: string, allowedTypes: string[]): boolean {
    const lowerMimeType = mimeType.toLowerCase();
    return allowedTypes.some(type => {
      if (type.endsWith('/*')) {
        const prefix = type.slice(0, -1);
        return lowerMimeType.startsWith(prefix);
      }
      return lowerMimeType === type;
    });
  }

  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    if (!this.isFileAllowed_(fileAttachment.mimeType)) {
      this.handleProcessFilesError_(ProcessFilesError.INVALID_TYPE);
      return;
    }
    const pendingStatus = this.pendingFiles_.get(fileAttachment.uuid);
    const composeboxFile: ComposeboxFile = {
      uuid: fileAttachment.uuid,
      name: fileAttachment.name,
      objectUrl: null,
      dataUrl: fileAttachment.imageDataUrl ?? null,
      type: fileAttachment.mimeType,
      status: pendingStatus ?? FileUploadStatus.kNotUploaded,
      url: null,
      tabId: null,
      isDeletable: true,
    };
    if (pendingStatus) {
      this.pendingFiles_.delete(fileAttachment.uuid);
    }
    this.onFileContextAdded(composeboxFile);
  }

  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContext_(new CustomEvent('addTabContext', {
      detail: {
        id: tabAttachment.tabId,
        title: tabAttachment.title,
        url: tabAttachment.url,
        delayUpload: /*delay_upload=*/ false,
        replaceAutoActiveTabToken: false,
        origin: TabUploadOrigin.OTHER,
      },
    }));
  }

  addSearchContext(context: SearchContext) {
    for (const attachment of context.attachments) {
      if (attachment.fileAttachment) {
        this.addFileFromAttachment_(attachment.fileAttachment);
      } else if (attachment.tabAttachment) {
        this.addTabFromAttachment_(attachment.tabAttachment);
      }
    }

    if (context.attachments) {
      recordContextAdditionMethod(
          ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource_);
    }

    switch (context.toolMode) {
      case ToolMode.kDeepSearch:
        this.setInitialMode(ComposeboxToolMode.kDeepSearch);
        break;
      case ToolMode.kCreateImage:
        this.setInitialMode(ComposeboxToolMode.kImageGen);
        break;
      case ToolMode.kCanvas:
        this.setInitialMode(ComposeboxToolMode.kCanvas);
        break;
      default:
    }
  }

  protected preventFocus_(e: FocusEvent) {
    e.preventDefault();
    e.stopPropagation();
  }

  protected onContextMenuContainerClick_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();

    // Ignore non-primary button clicks.
    if (e.button !== 0) {
      return;
    }

    if (this.shadowRoot.activeElement === null) {
      // If focus did not move to an inner element from this click event,
      // the user clicked on some non-focusable area.
      this.fire('context-menu-container-click');
    }
  }

  protected onDeleteFile_(
      e: CustomEvent<{uuid: UnguessableToken, fromUserAction?: boolean}>) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }

    const file = this.files_.get(e.detail.uuid);
    if (file?.tabId) {
      this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
          ([id, _]) => id !== file.tabId));
    }

    const fromAutoSuggestedChip =
        e.detail.uuid === this.automaticActiveTabChipToken_ &&
        (e.detail.fromUserAction === true);
    if (fromAutoSuggestedChip) {
      const metricName = 'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
          this.composeboxSource_;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.automaticActiveTabChipToken_ = null;
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== e.detail.uuid));
    this.fire(
        'delete-context',
        {uuid: e.detail.uuid, fromAutoSuggestedChip: fromAutoSuggestedChip});
  }

  private getProcessFilesErrorData_(error: ProcessFilesError):
      {metric: ComposeboxFileValidationError, errorMessage: string} {
    let metric = ComposeboxFileValidationError.NONE;
    let errorMessage = '';

    switch (error) {
      case ProcessFilesError.MAX_FILES_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        errorMessage = 'maxFilesReachedError';
        break;
      case ProcessFilesError.MAX_IMAGES_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        errorMessage = 'maxImagesReachedError';
        break;
      case ProcessFilesError.MAX_PDFS_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        errorMessage = 'maxPdfsReachedError';
        break;
      case ProcessFilesError.FILE_EMPTY:
        metric = ComposeboxFileValidationError.FILE_EMPTY;
        errorMessage = 'composeboxFileUploadInvalidEmptySize';
        break;
      case ProcessFilesError.FILE_TOO_LARGE:
        metric = ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE;
        errorMessage = 'composeboxFileUploadInvalidTooLarge';
        break;
      case ProcessFilesError.INVALID_TYPE:
        errorMessage = 'composeFileTypesAllowedError';
        break;
      default:
        break;
    }

    return {metric, errorMessage};
  }

  private handleProcessFilesError_(error: ProcessFilesError) {
    if (error === ProcessFilesError.NONE) {
      return;
    }

    const {metric, errorMessage} = this.getProcessFilesErrorData_(error);
    this.recordFileValidationMetric_(metric);
    if (this.contextMenuEnabled_) {
      this.$.contextEntrypoint.closeMenu();
    }
    this.fire('on-file-validation-error', {
      errorMessage: this.i18n(errorMessage),
    });
  }

  private isFileAllowed_(fileType: string): boolean {
    return this.isMimeTypeAllowed_(fileType, this.imageFileTypes_) ||
        this.isMimeTypeAllowed_(fileType, this.attachmentFileTypes_);
  }

  private getInputType_(type: string): InputType {
    if (type === 'tab') {
      return InputType.kBrowserTab;
    }
    if (type === 'image') {
      return InputType.kLensImage;
    }
    if (type === 'pdf') {
      return InputType.kLensFile;
    }

    if (this.imageFileTypes_.some(t => {
          if (t.endsWith('/*')) {
            const prefix = t.slice(0, -1);
            return type.startsWith(prefix);
          }
          return type === t;
        })) {
      return InputType.kLensImage;
    }

    return InputType.kLensFile;
  }

  protected processFiles_(files: FileList|null) {
    if (!files || files.length === 0) {
      return;
    }

    const filesToUpload: File[] = [];
    let errorToDisplay = ProcessFilesError.NONE;

    const counts = new Map<InputType, number>();
    counts.set(InputType.kLensImage, 0);
    counts.set(InputType.kLensFile, 0);
    counts.set(InputType.kBrowserTab, 0);

    for (const file of this.files_.values()) {
      const type = this.getInputType_(file.type);
      counts.set(type, (counts.get(type) || 0) + 1);
    }

    let totalCount = this.files_.size;

    let maxTotal = this.maxFileCount_;
    if (this.inputState && this.inputState.maxTotalInputs > 0) {
      maxTotal = this.inputState.maxTotalInputs;
    }

    if (totalCount + files.length > maxTotal) {
      errorToDisplay = ProcessFilesError.MAX_FILES_EXCEEDED;
    }

    for (const file of files) {
      if (file.size === 0 || file.size > this.maxFileSize_) {
        const sizeError = file.size === 0 ? ProcessFilesError.FILE_EMPTY :
                                            ProcessFilesError.FILE_TOO_LARGE;
        errorToDisplay = Math.max(errorToDisplay, sizeError);
        continue;
      }

      if (!this.isFileAllowed_(file.type)) {
        errorToDisplay =
            Math.max(errorToDisplay, ProcessFilesError.INVALID_TYPE);
        continue;
      }

      const inputType = this.getInputType_(file.type);
      let maxType = maxTotal;
      if (this.inputState &&
          this.inputState.maxInstances[inputType] !== undefined) {
        maxType = this.inputState.maxInstances[inputType];
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
              errorToDisplay = ProcessFilesError.MAX_IMAGES_EXCEEDED;
              break;
            case InputType.kLensFile:
              errorToDisplay = ProcessFilesError.MAX_PDFS_EXCEEDED;
              break;
            default:
              errorToDisplay = ProcessFilesError.MAX_FILES_EXCEEDED;
          }
        } else {
          errorToDisplay = ProcessFilesError.MAX_FILES_EXCEEDED;
        }
      }
    }

    const hasError = errorToDisplay !== ProcessFilesError.NONE;
    if (filesToUpload.length > 0) {
      let errorMessage = '';
      if (this.entrypointName === 'Realbox' && hasError) {
        const {metric, errorMessage: messageKey} =
            this.getProcessFilesErrorData_(errorToDisplay);
        errorMessage = this.i18n(messageKey);
        this.recordFileValidationMetric_(metric);
      }
      this.addFileContext_(filesToUpload, errorMessage);
    }

    if (this.entrypointName !== 'Realbox' || !hasError ||
        filesToUpload.length === 0) {
      this.handleProcessFilesError_(errorToDisplay);
    }
  }

  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;
    this.processFiles_(files);
    input.value = '';
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource_);
  }

  protected addFileContext_(filesToUpload: File[], errorMessage: string = '') {
    this.fire('add-file-context', {
      files: filesToUpload,
      errorMessage: errorMessage,
      onContextAdded: (files: Map<UnguessableToken, ComposeboxFile>) => {
        this.files_ = new Map([...this.files_.entries(), ...files.entries()]);
        this.recordFileValidationMetric_(ComposeboxFileValidationError.NONE);
      },
    });
  }

  protected addTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    replaceAutoActiveTabToken: boolean,
    origin: TabUploadOrigin,
  }>) {
    e.stopPropagation();

    this.fire('add-tab-context', {
      id: e.detail.id,
      title: e.detail.title,
      url: e.detail.url,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
      onContextAdded: (file: ComposeboxFile) => {
        this.files_ = new Map([...this.files_.entries(), [file.uuid, file]]);
        this.addedTabsIds_ = new Map(
            [...this.addedTabsIds_.entries(), [e.detail.id, file.uuid]]);
        if (e.detail.replaceAutoActiveTabToken) {
          if (this.automaticActiveTabChipToken_) {
            this.onDeleteFile_(new CustomEvent('deleteTabContext', {
              detail: {
                uuid: this.automaticActiveTabChipToken_,
              },
            }));
          }
          this.automaticActiveTabChipToken_ = file.uuid;
        }
      },
    });
  }

  protected openImageUpload_() {
    if (this.entrypointName === 'ContextualTasks') {
      // Open file dialog using top level primary window
      // in contextual tasks composebox.
      this.fire('open-file-dialog', {isImage: true});
    } else {
      assert(this.$.imageInput);
      this.$.imageInput.click();
    }
  }

  protected openFileUpload_() {
    if (this.entrypointName === 'ContextualTasks') {
      // Open file dialog using top level primary window
      // in contextual tasks composebox.
      this.fire('open-file-dialog', {isImage: false});
    } else if (this.$.fileInput) {
      this.$.fileInput.click();
    }
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ComposeboxToolMode}>) {
    this.handleToolClick_(e.detail.toolMode);
  }

  protected handleDeepSearchClick_() {
    this.handleToolClick_(ComposeboxToolMode.kDeepSearch);
  }

  protected handleImageGenClick_() {
    this.handleToolClick_(ComposeboxToolMode.kImageGen);
  }

  protected handleCanvasClick_() {
    this.handleToolClick_(ComposeboxToolMode.kCanvas);
  }

  protected handleToolClick_(tool: ComposeboxToolMode) {
    if (this.entrypointName !== 'Realbox') {
      if (this.contextMenuDescriptionEnabled_) {
        if (this.activeTool_ === tool) {
          this.showContextMenuDescription_ = true;
        } else {
          this.showContextMenuDescription_ =
              tool === ComposeboxToolMode.kUnspecified;
        }
      }

      if (this.activeTool_ === tool) {
        this.activeTool_ = ComposeboxToolMode.kUnspecified;
      } else {
        this.activeTool_ = tool;
      }
    }

    const isActive = this.activeTool_ === tool;
    this.fire('set-tool-mode', {
      tool: tool,
      enabled: isActive,
    });
  }

  protected onVoiceSearchClick_() {
    this.fire('open-voice-search');
  }

  private recordFileValidationMetric_(
      enumValue: ComposeboxFileValidationError) {
    recordEnumerationValue(
        'ContextualSearch.File.WebUI.UploadAttemptFailure.' +
            this.composeboxSource_,
        enumValue, ComposeboxFileValidationError.MAX_VALUE + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-entrypoint-and-carousel':
        ContextualEntrypointAndCarouselElement;
  }
}

customElements.define(
    ContextualEntrypointAndCarouselElement.is,
    ContextualEntrypointAndCarouselElement);
