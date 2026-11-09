// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReachedCase} from '//resources/js/assert.js';

export enum AnimationState {
  FADE_IN,
  HOLD,
  FADE_OUT,
}

export interface AnimationDetails {
  startOpacity: number;
  endOpacity: number;
  duration: number;
  nextAnimationState: AnimationState;
}

export interface PlaceholderTextCyclerOptions {
  animationTarget?: HTMLElement;
  onPlaceholderChange?: (placeholder: string) => void;
}

/**
 * Responsible for cycling placeholder text animations on a searchbox input.
 */
export class PlaceholderTextCycler {
  private input_: HTMLInputElement|HTMLTextAreaElement;
  private animationTarget_: HTMLElement;
  private animation_: Animation|null = null;
  private placeholderTexts_: string[] = [];
  private placeholderTextsCurrentIndex_: number = 0;
  private changePlaceholderTextIntervalMs_: number = 4000;
  private fadePlaceholderTextDurationMs_: number = 250;
  private onPlaceholderChange_?: (placeholder: string) => void;

  constructor(
      animatedPlaceholderContainer: HTMLInputElement|HTMLTextAreaElement,
      placeholderTexts: string[], changeTextAnimationIntervalMs: number,
      fadeTextAnimationDurationMs: number,
      options?: PlaceholderTextCyclerOptions) {
    assert(placeholderTexts.length > 1);

    this.input_ = animatedPlaceholderContainer;
    this.animationTarget_ =
        options?.animationTarget ?? animatedPlaceholderContainer;
    this.placeholderTexts_ = placeholderTexts;
    this.changePlaceholderTextIntervalMs_ = changeTextAnimationIntervalMs;
    this.fadePlaceholderTextDurationMs_ = fadeTextAnimationDurationMs;
    this.onPlaceholderChange_ = options?.onPlaceholderChange;
  }

  start() {
    this.stop();

    this.placeholderTextsCurrentIndex_ = 0;
    this.animate_(AnimationState.HOLD);
  }

  stop() {
    if (this.animation_) {
      this.animation_.cancel();
      this.animation_ = null;
    }

    this.placeholderTextsCurrentIndex_ = 0;
    this.input_.placeholder =
        this.placeholderTexts_[this.placeholderTextsCurrentIndex_]!;
    this.onPlaceholderChange_?.(this.input_.placeholder);
  }

  private animate_(state: AnimationState) {
    let animationDetails: AnimationDetails|null = null;
    switch (state) {
      case AnimationState.FADE_IN:
        this.input_.placeholder =
            this.placeholderTexts_[this.placeholderTextsCurrentIndex_]!;
        this.onPlaceholderChange_?.(this.input_.placeholder);
        animationDetails = {
          startOpacity: 0,
          endOpacity: 1,
          duration: this.fadePlaceholderTextDurationMs_,
          nextAnimationState: AnimationState.HOLD,
        };
        break;
      case AnimationState.HOLD:
        animationDetails = {
          startOpacity: 1,
          endOpacity: 1,
          duration: this.changePlaceholderTextIntervalMs_,
          nextAnimationState: AnimationState.FADE_OUT,
        };
        break;
      case AnimationState.FADE_OUT:
        this.placeholderTextsCurrentIndex_ =
            (this.placeholderTextsCurrentIndex_ + 1) %
            this.placeholderTexts_.length;
        animationDetails = {
          startOpacity: 1,
          endOpacity: 0,
          duration: this.fadePlaceholderTextDurationMs_,
          nextAnimationState: AnimationState.FADE_IN,
        };
        break;
      default:
        assertNotReachedCase(state);
    }

    this.animation_ = this.animationTarget_.animate(
        [
          {'--placeholder-opacity': animationDetails.startOpacity},
          {'--placeholder-opacity': animationDetails.endOpacity},
        ],
        {duration: animationDetails.duration},
    );
    this.animation_.onfinish = () => {
      if (this.animation_) {
        this.animate_(animationDetails.nextAnimationState);
      }
    };
  }
}
