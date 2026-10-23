/**
 * Copyright 2025 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {
  InvalidArgumentException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {
  EmptyResult,
  Emulation,
  UAClientHints,
} from '../../../protocol/protocol.js';
import type {ContextConfigStorage} from '../browser/ContextConfigStorage.js';

// Media features supported by Blink's MediaFeatureOverrides:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/css/media_feature_overrides.cc;l=124
const SUPPORTED_MEDIA_FEATURES = new Set([
  'color-gamut',
  'forced-colors',
  'prefers-color-scheme',
  'prefers-contrast',
  'prefers-reduced-data',
  'prefers-reduced-motion',
  'prefers-reduced-transparency',
]);

export class EmulationProcessor {
  #contextConfigStorage: ContextConfigStorage;

  constructor(contextConfigStorage: ContextConfigStorage) {
    this.#contextConfigStorage = contextConfigStorage;
  }

  async setForcedColorsModeThemeOverride(
    params: Emulation.SetForcedColorsModeThemeOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          forcedColorsModeTheme: params.theme,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setEmulatedMedia(
          config.mediaFeatures ?? null,
          config.forcedColorsModeTheme ?? null,
        );
      }),
    );
    return {};
  }

  async setGeolocationOverride(
    params: Emulation.SetGeolocationOverrideParameters,
  ): Promise<EmptyResult> {
    if ('coordinates' in params && 'error' in params) {
      // Unreachable. Handled by params parser.
      throw new InvalidArgumentException(
        'Coordinates and error cannot be set at the same time',
      );
    }

    let geolocation:
      | Emulation.GeolocationCoordinates
      | Emulation.GeolocationPositionError
      | null = null;

    if ('coordinates' in params) {
      if (
        (params.coordinates?.altitude ?? null) === null &&
        (params.coordinates?.altitudeAccuracy ?? null) !== null
      ) {
        throw new InvalidArgumentException(
          'Geolocation altitudeAccuracy can be set only with altitude',
        );
      }

      geolocation = params.coordinates;
    } else if ('error' in params) {
      if (params.error.type !== 'positionUnavailable') {
        // Unreachable. Handled by params parser.
        throw new InvalidArgumentException(
          `Unknown geolocation error ${params.error.type}`,
        );
      }
      geolocation = params.error;
    } else {
      // Unreachable. Handled by params parser.
      throw new InvalidArgumentException(`Coordinates or error should be set`);
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          geolocation,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setGeolocationOverride(config.geolocation ?? null);
      }),
    );
    return {};
  }

  async setLocaleOverride(
    params: Emulation.SetLocaleOverrideParameters,
  ): Promise<EmptyResult> {
    const locale = params.locale ?? null;

    if (locale !== null && !isValidLocale(locale)) {
      throw new InvalidArgumentException(`Invalid locale "${locale}"`);
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          locale,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await Promise.all([
          context.setLocaleOverride(config.locale ?? null),
          // Set `AcceptLanguage` to locale.
          context.setUserAgentAndAcceptLanguage(
            config.userAgent,
            config.locale,
            config.clientHints,
          ),
        ]);
      }),
    );
    return {};
  }

  async setMediaFeaturesOverride(
    params: Emulation.SetMediaFeaturesOverrideParameters,
  ): Promise<EmptyResult> {
    const unsupportedFeatures = Object.keys(params.features ?? {}).filter(
      (feature) => !SUPPORTED_MEDIA_FEATURES.has(feature),
    );
    if (unsupportedFeatures.length > 0) {
      throw new UnsupportedOperationException(
        `Media feature(s) not supported: ${unsupportedFeatures.map((f) => `"${f}"`).join(', ')}`,
      );
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          mediaFeatures: params.features,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setEmulatedMedia(
          config.mediaFeatures ?? null,
          config.forcedColorsModeTheme ?? null,
        );
      }),
    );
    return {};
  }

  async setScriptingEnabled(
    params: Emulation.SetScriptingEnabledParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          scriptingEnabled: params.enabled,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setScriptingEnabled(config.scriptingEnabled ?? null);
      }),
    );
    return {};
  }

  async setScrollbarTypeOverride(
    params: Emulation.SetScrollbarTypeOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          scrollbarType: params.scrollbarType,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setScrollbarTypeOverride(config.scrollbarType ?? null);
      }),
    );
    return {};
  }

  async setScreenOrientationOverride(
    params: Emulation.SetScreenOrientationOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          screenOrientation: params.screenOrientation,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setViewport(
          config.viewport ?? null,
          config.devicePixelRatio ?? null,
          config.screenOrientation ?? null,
        );
      }),
    );
    return {};
  }

  async setScreenSettingsOverride(
    params: Emulation.SetScreenSettingsOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          screenArea: params.screenArea,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setViewport(
          config.viewport ?? null,
          config.devicePixelRatio ?? null,
          config.screenOrientation ?? null,
        );
      }),
    );
    return {};
  }

  async setTimezoneOverride(
    params: Emulation.SetTimezoneOverrideParameters,
  ): Promise<EmptyResult> {
    let timezone = params.timezone ?? null;

    if (timezone !== null && !isValidTimezone(timezone)) {
      throw new InvalidArgumentException(`Invalid timezone "${timezone}"`);
    }

    if (timezone !== null && isTimeZoneOffsetString(timezone)) {
      // CDP supports offset timezone with `GMT` prefix.
      timezone = `GMT${timezone}`;
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          timezone,
        },
        params.contexts,
        params.userContexts,
        false,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );
        await context.setTimezoneOverride(config.timezone ?? null);
      }),
    );
    return {};
  }

  async setTouchOverride(
    params: Emulation.SetTouchOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          maxTouchPoints: params.maxTouchPoints,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );
        await context.setTouchOverride(config.maxTouchPoints ?? null);
      }),
    );
    return {};
  }

  async setUserAgentOverrideParams(
    params: Emulation.SetUserAgentOverrideParameters,
  ): Promise<EmptyResult> {
    if (params.userAgent === '') {
      throw new UnsupportedOperationException(
        'empty user agent string is not supported',
      );
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          userAgent: params.userAgent,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );
        await context.setUserAgentAndAcceptLanguage(
          config.userAgent,
          config.locale,
          config.clientHints,
        );
      }),
    );
    return {};
  }

  async setClientHintsOverride(
    params: UAClientHints.UserAgentClientHints.SetClientHintsOverrideCommand['params'],
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          clientHints: params.clientHints ?? null,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );
        await context.setUserAgentAndAcceptLanguage(
          config.userAgent,
          config.locale,
          config.clientHints,
        );
      }),
    );
    return {};
  }

  async setNetworkConditions(
    params: Emulation.SetNetworkConditionsParameters,
  ): Promise<EmptyResult> {
    if (
      params.networkConditions !== null &&
      params.networkConditions.type !== 'offline'
    ) {
      throw new UnsupportedOperationException(
        `Unsupported network conditions ${params.networkConditions.type}`,
      );
    }

    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          emulatedNetworkConditions: params.networkConditions,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setEmulatedNetworkConditions(
          config.emulatedNetworkConditions ?? null,
        );
      }),
    );
    return {};
  }

  async setViewportMetaOverride(
    params: Emulation.SetViewportMetaOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          viewportMeta: params.viewportMeta,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setViewportMetaOverride(config.viewportMeta ?? null);
      }),
    );
    return {};
  }

  async setTextLayoutModeOverride(
    params: Emulation.SetTextLayoutModeOverrideParameters,
  ): Promise<EmptyResult> {
    const relatedBrowsingContexts =
      await this.#contextConfigStorage.updateConfig(
        {
          textLayoutMode: params.textLayoutMode,
        },
        params.contexts,
        params.userContexts,
      );

    await Promise.all(
      relatedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing more granular setting.
        const config = this.#contextConfigStorage.getActiveConfig(
          context.id,
          context.userContext,
        );

        await context.setTextLayoutModeOverride(config.textLayoutMode ?? null);
      }),
    );
    return {};
  }
}

// Export for testing.
export function isValidLocale(locale: string): boolean {
  try {
    new Intl.Locale(locale);
    return true;
  } catch (e) {
    if (e instanceof RangeError) {
      return false;
    }
    // Re-throw other errors
    throw e;
  }
}

// Export for testing.
export function isValidTimezone(timezone: string): boolean {
  try {
    Intl.DateTimeFormat(undefined, {timeZone: timezone});
    return true;
  } catch (e) {
    if (e instanceof RangeError) {
      return false;
    }
    // Re-throw other errors
    throw e;
  }
}

// Export for testing.
export function isTimeZoneOffsetString(timezone: string): boolean {
  return /^[+-](?:2[0-3]|[01]\d)(?::[0-5]\d)?$/.test(timezone);
}
