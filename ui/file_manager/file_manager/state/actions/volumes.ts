// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeId} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';


/** Action to add single volume into the store. */
export interface AddVolumeAction extends BaseAction {
  type: ActionType.ADD_VOLUME;
  payload: {
    volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata,
    volumeInfo: VolumeInfo,
  };
}


/** Action to remove single volume from the store. */
export interface RemoveVolumeAction extends BaseAction {
  type: ActionType.REMOVE_VOLUME;
  payload: {
    volumeId: VolumeId,
  };
}

/** Action factory to add single volume into the store. */
export function addVolume(
    {volumeMetadata, volumeInfo}: AddVolumeAction['payload']): AddVolumeAction {
  return {
    type: ActionType.ADD_VOLUME,
    payload: {
      volumeMetadata,
      volumeInfo,
    },
  };
}

/** Action factory to remove single volume from the store. */
export function removeVolume({volumeId}: RemoveVolumeAction['payload']):
    RemoveVolumeAction {
  return {
    type: ActionType.REMOVE_VOLUME,
    payload: {volumeId},
  };
}
