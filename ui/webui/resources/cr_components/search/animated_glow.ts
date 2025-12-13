// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './audio_wave.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './animated_glow.css.js';
import {getHtml} from './animated_glow.html.js';
import {GlowAnimationState} from './constants.js';

/*
 * Animation glow for expanding, submitting, voice, dragging.
 * Parent must pass animationState to trigger one of 4 animations, or idle.
 * RequiresVoice, isCollapsible are optional. RequiresVoice is
 * false by default. And if it is true, then it renders the eclipse audio wave
 * element as well. IsCollapsible adds an animation for expand (see .css file).
 * Transcript and receivedSpeech are optional. Allows for audio to simulate
 * audio input without opening audio stream.
 */
export class SearchAnimatedGlowElement extends CrLitElement {
  static get is() {
    return 'search-animated-glow';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      animationState: {
        type: String,
        reflect: true,
      },
      entrypointName: {type: String},
      dragDropPlaceholder: {type: String},
      requiresVoice: {type: Boolean},
      isCollapsible: {
        type: Boolean,
        reflect: true,
      },
      transcript: {type: String},
      receivedSpeech: {type: Boolean},
    };
  }

  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor dragDropPlaceholder: string =
      loadTimeData.getString('composeboxDragAndDropHint');
  accessor requiresVoice: boolean = false;
  accessor isCollapsible: boolean = false;
  accessor transcript: string = '';
  accessor receivedSpeech: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'search-animated-glow': SearchAnimatedGlowElement;
  }
}

customElements.define(SearchAnimatedGlowElement.is, SearchAnimatedGlowElement);
