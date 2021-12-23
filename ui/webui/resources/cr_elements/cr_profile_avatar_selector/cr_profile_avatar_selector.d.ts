// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export type AvatarIcon = {
  url: string,
  label: string,
  index: number,
  isGaiaAvatar: boolean,
  selected: boolean,
};

interface CrProfileAvatarSelectorElement extends PolymerElement {
  avatars: AvatarIcon[];
  selectedAvatar: AvatarIcon|null;
  ignoreModifiedKeyEvents: boolean;
}

export {CrProfileAvatarSelectorElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-profile-avatar-selector': CrProfileAvatarSelectorElement;
  }
}
