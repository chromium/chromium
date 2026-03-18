// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {InputState} from './composebox_query.mojom-webui.js';

type Constructor<T> = new (...args: any[]) => T;

export const ComposeboxEmbedderMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<ComposeboxEmbedderMixinInterface> => {
      class ComposeboxEmbedderMixin extends superClass implements
          ComposeboxEmbedderMixinInterface {
        static get properties() {
          return {
            activeToolMode: {type: Number, reflect: true},
            addedTabsIds: {type: Object},
            errorMessage: {type: String},
            files: {type: Object},
            input: {type: String},
            inputPlaceholder: {type: String, reflect: true},
            inputState: {type: Object},
          };
        }

        accessor activeToolMode: ComposeboxToolMode =
            ComposeboxToolMode.kUnspecified;
        accessor addedTabsIds: Map<number, UnguessableToken> = new Map();
        accessor errorMessage: string = '';
        accessor files: Map<UnguessableToken, ComposeboxFile> = new Map();
        accessor input: string = '';
        accessor inputPlaceholder: string =
            loadTimeData.getString('searchboxComposePlaceholder');
        accessor inputState: InputState|null = null;
      }

      return ComposeboxEmbedderMixin;
    };

export interface ComposeboxEmbedderMixinInterface {
  activeToolMode: ComposeboxToolMode;
  addedTabsIds: Map<number, UnguessableToken>;
  errorMessage: string;
  files: Map<UnguessableToken, ComposeboxFile>;
  input: string;
  inputPlaceholder: string;
  inputState: InputState|null;
}
