// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './composebox_file_inputs.js';
import './composebox_lens_search.js';
import './composebox_tool_chip.js';
import './contextual_entrypoint_and_menu.js';
import './contextual_entrypoint_button.js';
import './composebox_dropdown.js';
import './composebox_voice_search.js';
import './error_scrim.js';
import './file_carousel.js';
import './file_thumbnail.js';
import './icons.html.js';
import './composebox_input.js';
import './composebox_submit.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ComposeboxFile, mapUploadErrorToProcessFilesError, ProcessFilesError, recordContextAdditionMethod} from './common.js';
import type {TabUpload} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from './composebox_file_inputs.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ComposeboxEmbedderMixin} from './composebox_mixin.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import type {ContextUploadErrorType} from './composebox_query.mojom-webui.js';
import {ContextUploadStatus, ToolMode} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';

export interface ComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    fileInputs: ComposeboxFileInputsElement,
    matches: ComposeboxDropdownElement,
  };
}

// LINT.IfChange
export class ComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'cr-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showLensButton: {type: Boolean},
      lensButtonTriggersOverlay: {type: Boolean},
      isCollapsible: {
        reflect: true,
        type: Boolean,
      },
      expanding_: {
        reflect: true,
        type: Boolean,
      },
      /**
       * Feature flag for New Tab Page Realbox Next.
       */
      ntpRealboxNextEnabled: {
        type: Boolean,
        // NOTE: Do not reflect this property. For shared UI, use
        // `searchboxNextEnabled`. For NTP-specific styling, use CSS from the
        // embedding component (e.g., `new_tab_page/app.css`).
      },
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
      },
      carouselOnTop_: {
        type: Boolean,
      },
      entrypointName: {type: String, reflect: true},
      disableComposeboxAnimation: {type: Boolean},
      isOmniboxInCompactMode_: {
        type: Boolean,
        reflect: true,
      },
      // Must be property so can pass it down to children.
      searchboxCallbackRouter_: {type: Object},
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor disableComposeboxAnimation: boolean = false;
  accessor lensButtonTriggersOverlay: boolean = false;
  accessor showLensButton: boolean = true;
  accessor ntpRealboxNextEnabled: boolean = false;
  accessor carouselOnTop_: boolean = false;
  accessor entrypointName: string = '';
  accessor lensButtonDisabled: boolean = false;
  accessor applyContextButtonBackground: boolean = false;

  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanding_: boolean = false;
  protected accessor isOmniboxInCompactMode_: boolean = false;
  protected accessor searchboxCallbackRouter_: SearchboxPageCallbackRouter;

  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;

  override shouldShowDivider(): boolean {
    // TODO(crbug.com/476175193): Remove `entrypointName` condition.
    if (this.entrypointName === 'Omnibox' &&
        this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    // TODO(crbug.com/486707841): Move to the NTP embedder.
    // Retain the divider for NTP when only tab favicons are present.
    // Omnibox is excluded as it does not need this divider.
    if (this.entrypointName !== 'Omnibox' && this.hasTabs() &&
        !this.hasNonTabFiles()) {
      return this.showDropdown;
    }
    return super.shouldShowDivider();
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getContextEntrypointElement(): ContextualEntrypointButtonElement|
      ContextualEntrypointAndMenuElement|null {
    return this.shadowRoot?.querySelector<ContextualEntrypointButtonElement|
                                          ContextualEntrypointAndMenuElement>(
               '#contextEntrypoint') ||
        null;
  }

  override getLensButtonElement(): HTMLElement|null {
    return this.shadowRoot?.querySelector('#lensIcon') || null;
  }

  override getFileInputsElement(): ComposeboxFileInputsElement|null {
    return this.shouldDisableFileInputs() ? null : this.$.fileInputs;
  }

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set the initial expanded state based on the inputted property.
    this.expanding_ = !this.isCollapsible;
    this.animationState = this.isCollapsible ? GlowAnimationState.NONE :
                                               GlowAnimationState.EXPANDING;

    this.focusInput();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('entrypointName') ||
        changedProperties.has('searchboxLayoutMode')) {
      this.isOmniboxInCompactMode_ = this.entrypointName === 'Omnibox' &&
          this.searchboxLayoutMode === 'Compact';
    }

    if (changedProperties.has('inputState') ||
        changedProperties.has('entrypointName') ||
        changedProperties.has('webuiOmniboxSimplificationEnabled')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      const hasBackground = this.entrypointName === 'Omnibox' ?
          this.webuiOmniboxSimplificationEnabled :
          false;
      this.applyContextButtonBackground = hasBackground && !inToolMode;
    }

    // If it is the first render, or 'entrypointName' has changed from its default.
    if (!this.hasUpdated || changedProperties.has('entrypointName')) {
      this.voiceSearchCoherenceEnabled =
          this.computeVoiceSearchCoherenceEnabled();
    }
  }

  playGlowAnimation() {
    // If |animationState_| were still EXPANDING, this function would have no
    // effect because nothing changes in CSS and therefore animations wouldn't
    // be re-trigered. Resetting it to NONE forces the animation related styles
    // to reset before switching to EXPANDING.
    this.animationState = GlowAnimationState.NONE;
    // Wait for the style change for NONE to commit. This ensures the browser
    // detects a state change when we switch to EXPANDING.

    // If the composebox is not submittable or it is already expanded, do not
    // trigger the animation.
    if (this.expanding_ && !this.submitEnabled) {
      requestAnimationFrame(() => {
        this.animationState = GlowAnimationState.EXPANDING;
      });
    }
  }

  isExpanded(): boolean {
    return this.expanding_;
  }

  setExpandingForTesting(expanding: boolean) {
    this.expanding_ = expanding;
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const file = super.deleteFile(uuidToDelete, fromUserAction);
    if (file) {
      this.queryAutocomplete(/* clearMatches= */ true);
    }
    return file;
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles(e.detail.files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource);
  }

  protected onLensClick_() {
    if (this.lensButtonTriggersOverlay) {
      this.pageHandler_.handleLensButtonClick();
    } else {
      this.pageHandler_.handleFileUpload(/*is_image=*/ true);
    }
  }

  protected onLensIconMousedown_(e: MouseEvent) {
    // Prevent the composebox from expanding due to being focused by capturing
    // the mousedown event. This is needed to allow the Lens icon to be
    // clicked when the composebox does not have focus without expanding the
    // composebox.
    e.preventDefault();
  }

  protected onComposeboxFocusin_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }

    // If the composebox was focused out, collapsed and now focused in,
    // requery autocomplete to get fresh contextual suggestions.
    if (this.isCollapsible) {
      this.queryAutocomplete(/* clearMatches= */ true);
    }

    this.expanding_ = true;
    this.pageHandler_.focusChanged(true);
    this.fire('composebox-focus-in');
  }

  protected onComposeboxFocusout_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    // If the the composebox is collapsible and empty, collapse it.
    // Else, keep the composebox expanded.
    this.expanding_ = this.isCollapsible ? this.submitEnabled : true;
    this.pageHandler_.focusChanged(false);
    this.fire('composebox-focus-out');
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input = context.input;
      }
      for (const attachment of context.attachments) {
        if (attachment.fileAttachment) {
          this.addFileFromAttachment_(attachment.fileAttachment);
        } else if (attachment.tabAttachment) {
          this.addTabFromAttachment_(attachment.tabAttachment);
        }
      }
    }
    // Query for ZPS even if there's no context.
    if (this.showZps) {
      // Clear the autocomplete matches here, as failure to do so triggers a
      // DCHECK in `ZpsSection::InitMatches()` whenever the user tries to upload
      // a file after having uploaded an invalid file earlier in the session.
      this.queryAutocomplete(/* clearMatches= */ true);
    }
  }

  // TODO(crbug.com/486706573): Common logic is moved to the mixin class. Move
  // embedder specific logic to the embedder class.
  override closeComposebox(afterSubmit: boolean = false) {
    super.closeComposebox(afterSubmit);

    if (this.isCollapsible) {
      this.expanding_ = false;
      this.animationState = GlowAnimationState.NONE;
      this.getInputElement().inputElement.blur();
    }
  }

  override submitCleanup() {
    if (this.isCollapsible) {
      this.clearAllInputs(/* querySubmitted= */ true,
                          /* shouldBlockAutoSuggestedTabs= */ false);
    }

    super.submitCleanup();
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    const errorType = fileAttachment.errorType ?? null;
    if (errorType) {
      const processFilesError = mapUploadErrorToProcessFilesError(
          errorType as ContextUploadErrorType);
      if (processFilesError !== ProcessFilesError.NONE) {
        this.handleProcessFilesError(processFilesError);
        this.deleteFileContext(fileAttachment.uuid);
        return;
      }
    }

    const pendingStatus = this.attachedContext.get(fileAttachment.uuid)?.status;
    const composeboxFile = ComposeboxFile.createFromFile(
        fileAttachment.uuid as unknown as UnguessableToken,
        {name: fileAttachment.name, type: fileAttachment.mimeType},
        pendingStatus ?? ContextUploadStatus.kNotUploaded, {
          dataUrl: fileAttachment.imageDataUrl ?? null,
          iconUrl: fileAttachment.iconUrl ?? null,
          supportsUnimodal: true,
        });
    this.onFileContextAdded(composeboxFile);
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContextHandleCallback({
      tabId: tabAttachment.tabId,
      title: tabAttachment.title,
      url: tabAttachment.url,
      delayUpload: /*delay_upload=*/ false,
      origin: tabAttachment.source,
    } as TabUpload);
  }

}
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/Componentization.md)

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
