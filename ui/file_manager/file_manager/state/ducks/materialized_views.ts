// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ICON_TYPES} from '../../foreground/js/constants.js';
import {Slice} from '../../lib/base_store.js';
import type {MaterializedView, State} from '../../state/state.js';

import {cacheMaterializedViews} from './all_entries.js';


const slice = new Slice<State, State['materializedViews']>('materializedViews');
export {slice as materializedViewsSlice};

export const updateMaterializedViews = slice.addReducer(
    'update-materialized-views', updateMaterializedViewsReducer);

function updateMaterializedViewsReducer(currentState: State, payload: {
  materializedViews: chrome.fileManagerPrivate.MaterializedView[],
}): State {
  const materializedViews: MaterializedView[] = [];
  for (const view of payload.materializedViews) {
    materializedViews.push({
      id: view.viewId.toString(),
      key: `materialized-view://${view.viewId}/`,
      label: view.name,
      icon: ICON_TYPES.STAR,
      isRoot: true,
    });
  }

  cacheMaterializedViews(currentState, materializedViews);
  return {
    ...currentState,
    materializedViews: materializedViews,
  };
}
