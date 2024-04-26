// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Type definitions for chrome.system.display API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace system {
      export namespace display {
        /**
         * @see https://developer.chrome.com/extensions/system.display#type-Bounds
         */
        export interface Bounds {
          left: number;
          top: number;
          width: number;
          height: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-Insets
         */
        export interface Insets {
          left: number;
          top: number;
          right: number;
          bottom: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-DisplayMode
         */
        export interface DisplayMode {
          width: number;
          height: number;
          widthInNativePixels: number;
          heightInNativePixels: number;
          uiScale?: number;
          deviceScaleFactor: number;
          refreshRate: number;
          isNative: boolean;
          isSelected: boolean;
          isInterlaced?: boolean;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-LayoutPosition
         */
        export enum LayoutPosition {
          TOP = 'top',
          RIGHT = 'right',
          BOTTOM = 'bottom',
          LEFT = 'left',
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-ActiveState
         */
        export enum ActiveState {
          ACTIVE = 'active',
          INACTIVE = 'inactive',
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-DisplayLayout
         */
        export interface DisplayLayout {
          id: string;
          parentId: string;
          position: LayoutPosition;
          offset: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-Edid
         */
        export interface Edid {
          manufacturerId: string;
          productId: string;
          yearOfManufacture: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-DisplayUnitInfo
         */
        export interface DisplayUnitInfo {
          id: string;
          name: string;
          edid?: Edid;
          mirroringSourceId: string;
          mirroringDestinationIds: string[];
          isPrimary: boolean;
          isInternal: boolean;
          activeState: ActiveState;
          isEnabled: boolean;
          isUnified: boolean;
          isAutoRotationAllowed?: boolean;
          dpiX: number;
          dpiY: number;
          rotation: number;
          bounds: Bounds;
          overscan: Insets;
          workArea: Bounds;
          modes: DisplayMode[];
          hasTouchSupport: boolean;
          hasAccelerometerSupport: boolean;
          availableDisplayZoomFactors: number[];
          displayZoomFactor: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-DisplayProperties
         */
        export interface DisplayProperties {
          isUnified?: boolean;
          mirroringSourceId?: string;
          isPrimary?: boolean;
          overscan?: Insets;
          rotation?: number;
          boundsOriginX?: number;
          boundsOriginY?: number;
          displayMode?: DisplayMode;
          displayZoomFactor?: number;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-GetInfoFlags
         */
        export interface GetInfoFlags {
          singleUnified?: boolean;
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-MirrorMode
         */
        export enum MirrorMode {
          OFF = 'off',
          NORMAL = 'normal',
          MIXED = 'mixed',
        }

        /**
         * @see https://developer.chrome.com/extensions/system.display#type-MirrorModeInfo
         */
        export interface MirrorModeInfo {
          mode: MirrorMode;
          mirroringSourceId?: string;
          mirroringDestinationIds?: string[];
        }

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-getDisplayLayout
         */
        export function getDisplayLayout(): Promise<DisplayLayout[]>;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-getInfo
         */
        export function getInfo(flags?: GetInfoFlags):
            Promise<DisplayUnitInfo[]>;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-overscanCalibrationAdjust
         */
        export function overscanCalibrationAdjust(id: string, delta: Insets):
            void;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-overscanCalibrationComplete
         */
        export function overscanCalibrationComplete(id: string): void;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-overscanCalibrationReset
         */
        export function overscanCalibrationReset(id: string): void;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-overscanCalibrationStart
         */
        export function overscanCalibrationStart(id: string): void;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-setDisplayLayout
         */
        export function setDisplayLayout(layouts: DisplayLayout[]):
            Promise<void>;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-setDisplayProperties
         */
        export function setDisplayProperties(
            id: string, info: DisplayProperties): Promise<void>;

        /**
         * @see https://developer.chrome.com/docs/extensions/reference/system_display/#method-setMirrorMode
         */
        export function setMirrorMode(info: MirrorModeInfo): Promise<void>;

        /**
         * @see https://developer.chrome.com/extensions/system.display#method-showNativeTouchCalibration
         */
        export function showNativeTouchCalibration(id: string):
            Promise<boolean>;

        /**
         * @see https://developer.chrome.com/extensions/system.display#event-onDisplayChanged
         */
        export const onDisplayChanged: ChromeEvent<() => void>;
      }
    }
  }
}
