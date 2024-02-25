// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';


/**
 * Events dispatched by concrete banners.
 */
export enum BannerEvent {
  BANNER_DISMISSED = 'banner-dismissed',
  BANNER_DISMISSED_FOREVER = 'banner-dismissed-forever',
}

/**
 * Event source for BANNER_DISMISSED_FOREVER event.
 */
export enum DismissedForeverEventSource {
  EXTRA_BUTTON = 'extra-button',
  DEFAULT_DISMISS_BUTTON = 'default-dismiss-button',
  OVERRIDEN_DISMISS_BUTTON = 'overriden-dismiss-button',
}

/**
 * Helper const to define infinite time showing.
 */
export const BANNER_INFINITE_TIME = 0;

/**
 * An allowed volume defines a `VolumeType` that a banner should be shown on.
 */
interface AllowedVolume {
  id?: string;
  type: VolumeType;
}

/**
 * An allowed root defines a `RootType` that a banner should be shown on.
 */
interface AllowedRoot {
  id?: string;
  root: RootType;
}

/**
 * A union of the two above types to ensure either `root` or `type` is defined.
 */
export type AllowedVolumeOrType = AllowedVolume|AllowedRoot;

/**
 * The minimum free disk space ratio for the `RootType` before a banner should
 * be shown.
 */
interface MinRatioThreshold {
  type: RootType;
  minRatio: number;
}

/**
 * A minimum free disk space size for a RootType before a banner should be
 * shown.
 */
interface MinSizeThreshold {
  type: RootType;
  minSize: number;
}

/**
 * A union of the above type to ensure either `minRatio` or `minSize` is shown.
 */
export type MinDiskThreshold = MinRatioThreshold|MinSizeThreshold;

export interface Banner {
  connectedCallback?(): void;

  getTemplate(): Node;

  /**
   * Returns the volume types or roots where the banner is enabled.
   */
  allowedVolumes(): AllowedVolumeOrType[];

  /**
   * The number of Files app sessions a banner can be shown for. A session is
   * defined as a new window for Files app. If a user opens a window, the same
   * session is maintained until either that window is closed or another
   * window is opened.
   */
  showLimit?(): number|undefined;

  /**
   * The seconds that a banner is to remain visible.
   */
  timeLimit?(): number;

  /**
   * The size threshold to trigger a banner if it goes below.
   */
  diskThreshold?(): MinDiskThreshold|undefined;

  /**
   * The duration (in seconds) to hide the banner after it has been dismissed
   * by the user.
   */
  hideAfterDismissedDurationSeconds?(): number;

  /**
   * Drive connection state to trigger the banner on.
   */
  driveConnectionState?(): chrome.fileManagerPrivate.DriveConnectionState;

  /**
   * Lifecycle method used to notify Banner implementations when they've been
   * shown.
   */
  onShow?(): void;

  /**
   * When a custom filter is registered for a banner and the banner is shown,
   * some context can be passed back to the banner to update.
   */
  onFilteredContext?(context: any): void;
}

export abstract class Banner extends HTMLElement {}

declare global {
  export type BannerDismissedEvent = CustomEvent<{banner: Banner}>;

  interface HTMLElementEventMap {
    [BannerEvent.BANNER_DISMISSED]: BannerDismissedEvent;
    [BannerEvent.BANNER_DISMISSED_FOREVER]: BannerDismissedEvent;
  }
}
