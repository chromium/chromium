/*
 * Copyright 2023 Google LLC.
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
 *
 */
import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {Bluetooth, Speculation} from '../../../protocol/chromium-bidi.js';
import {
  type Browser,
  type BrowsingContext,
  type ChromiumBidi,
  Emulation,
  Session,
  type UAClientHints,
  UnknownErrorException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {LoggerFn} from '../../../utils/log.js';
import {LogType} from '../../../utils/log.js';
import type {Result} from '../../../utils/result.js';
import type {ContextConfig} from '../browser/ContextConfig.js';
import type {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
import {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {LogManager} from '../log/LogManager.js';
import {
  MAX_TOTAL_COLLECTED_SIZE,
  type NetworkStorage,
} from '../network/NetworkStorage.js';
import type {ChannelProxy} from '../script/ChannelProxy.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import type {RealmStorage} from '../script/RealmStorage.js';
import type {EventManager} from '../session/EventManager.js';

interface FetchStages {
  request: boolean;
  response: boolean;
  auth: boolean;
}
export class CdpTarget {
  readonly #id: Protocol.Target.TargetID;
  readonly userContext: Browser.UserContext;
  readonly #cdpClient: CdpClient;
  readonly #browserCdpClient: CdpClient;
  readonly #parentCdpClient: CdpClient;
  readonly #realmStorage: RealmStorage;
  readonly #eventManager: EventManager;

  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;
  readonly contextConfigStorage: ContextConfigStorage;

  readonly #unblocked = new Deferred<Result<void>>();
  // Default user agent for the target. Required, as emulating client hints without user
  // agent is not possible. Cache it to avoid round trips to the browser for every target override.
  readonly #defaultUserAgent: string;
  readonly #logger: LoggerFn | undefined;

  /**
   * Target's window id. Is filled when the CDP target is created and do not reflect
   * moving targets from one window to another. The actual values
   * will be set during `#unblock`.
   * */
  #windowId?: number;

  #deviceAccessEnabled = false;
  #cacheDisableState = false;
  #preloadEnabled = false;
  #fetchDomainStages: FetchStages = {
    request: false,
    response: false,
    auth: false,
  };

  static create(
    targetId: Protocol.Target.TargetID,
    cdpClient: CdpClient,
    browserCdpClient: CdpClient,
    parentCdpClient: CdpClient,
    realmStorage: RealmStorage,
    eventManager: EventManager,
    preloadScriptStorage: PreloadScriptStorage,
    browsingContextStorage: BrowsingContextStorage,
    networkStorage: NetworkStorage,
    configStorage: ContextConfigStorage,
    userContext: Browser.UserContext,
    defaultUserAgent: string,
    logger?: LoggerFn,
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      browserCdpClient,
      parentCdpClient,
      eventManager,
      realmStorage,
      preloadScriptStorage,
      browsingContextStorage,
      configStorage,
      networkStorage,
      userContext,
      defaultUserAgent,
      logger,
    );

    LogManager.create(cdpTarget, realmStorage, eventManager, logger);

    cdpTarget.#setEventListeners();

    // No need to await.
    // Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();

    return cdpTarget;
  }

  constructor(
    targetId: Protocol.Target.TargetID,
    cdpClient: CdpClient,
    browserCdpClient: CdpClient,
    parentCdpClient: CdpClient,
    eventManager: EventManager,
    realmStorage: RealmStorage,
    preloadScriptStorage: PreloadScriptStorage,
    browsingContextStorage: BrowsingContextStorage,
    configStorage: ContextConfigStorage,
    networkStorage: NetworkStorage,
    userContext: Browser.UserContext,
    defaultUserAgent: string,
    logger: LoggerFn | undefined,
  ) {
    this.#defaultUserAgent = defaultUserAgent;
    this.userContext = userContext;
    this.#id = targetId;
    this.#cdpClient = cdpClient;
    this.#browserCdpClient = browserCdpClient;
    this.#parentCdpClient = parentCdpClient;
    this.#eventManager = eventManager;
    this.#realmStorage = realmStorage;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#networkStorage = networkStorage;
    this.#browsingContextStorage = browsingContextStorage;
    this.contextConfigStorage = configStorage;
    this.#logger = logger;
  }

  /** Returns a deferred that resolves when the target is unblocked. */
  get unblocked(): Deferred<Result<void>> {
    return this.#unblocked;
  }

  get id(): Protocol.Target.TargetID {
    return this.#id;
  }

  get cdpClient(): CdpClient {
    return this.#cdpClient;
  }

  get parentCdpClient(): CdpClient {
    return this.#parentCdpClient;
  }

  get browserCdpClient(): CdpClient {
    return this.#browserCdpClient;
  }

  /** Needed for CDP escape path. */
  get cdpSessionId(): Protocol.Target.SessionID {
    // SAFETY we got the client by it's id for creating
    return this.#cdpClient.sessionId!;
  }

  /**
   * Window id the target belongs to. If not known, returns 0.
   */
  get windowId(): number {
    if (this.#windowId === undefined) {
      this.#logger?.(
        LogType.debugError,
        'Getting windowId before it was set, returning 0',
      );
    }
    return this.#windowId ?? 0;
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    const config = this.contextConfigStorage.getActiveConfig(
      this.topLevelId,
      this.userContext,
    );

    const results = await Promise.allSettled([
      this.#cdpClient.sendCommand('Page.enable', {
        enableFileChooserOpenedEvent: true,
      }),
      ...(this.#ignoreFileDialog()
        ? []
        : [
            this.#cdpClient.sendCommand('Page.setInterceptFileChooserDialog', {
              enabled: true,
              // The intercepted dialog should be canceled.
              cancel: true,
            }),
          ]),
      // There can be some existing frames in the target, if reconnecting to an
      // existing browser instance, e.g. via Puppeteer. Need to restore the browsing
      // contexts for the frames to correctly handle further events, like
      // `Runtime.executionContextCreated`.
      // It's important to schedule this task together with enabling domains commands to
      // prepare the tree before the events (e.g. Runtime.executionContextCreated) start
      // coming.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/2282
      this.#cdpClient
        .sendCommand('Page.getFrameTree')
        .then((frameTree) => this.#restoreFrameTreeState(frameTree.frameTree)),
      this.#cdpClient.sendCommand('Runtime.enable'),
      this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
        enabled: true,
      }),
      // Enabling CDP Network domain is required for navigation detection:
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/2856.
      this.#cdpClient
        .sendCommand('Network.enable', {
          // If `googDisableNetworkDurableMessages` flag is set, do not enable durable
          // messages.
          enableDurableMessages: config.disableNetworkDurableMessages !== true,
          maxTotalBufferSize: MAX_TOTAL_COLLECTED_SIZE,
        })
        .then(() => this.toggleNetworkIfNeeded()),
      this.#cdpClient.sendCommand('Target.setAutoAttach', {
        autoAttach: true,
        waitForDebuggerOnStart: true,
        flatten: true,
      }),
      this.#updateWindowId(),
      this.#setUserContextConfig(config),
      this.#initAndEvaluatePreloadScripts(),
      this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger'),
      // Resume tab execution as well if it was paused by the debugger.
      this.#parentCdpClient.sendCommand('Runtime.runIfWaitingForDebugger'),
      this.toggleDeviceAccessIfNeeded(),
      this.togglePreloadIfNeeded(),
    ]);
    for (const result of results) {
      if (result instanceof Error) {
        // Ignore errors during configuring targets, just log them.
        this.#logger?.(
          LogType.debugError,
          'Error happened when configuring a new target',
          result,
        );
      }
    }

    this.#unblocked.resolve({
      kind: 'success',
      value: undefined,
    });
  }

  #restoreFrameTreeState(frameTree: Protocol.Page.FrameTree) {
    const frame = frameTree.frame;
    const maybeContext = this.#browsingContextStorage.findContext(frame.id);
    if (maybeContext !== undefined) {
      // Restoring parent of already known browsing context. This means the target is
      // OOPiF and the BiDi session was connected to already existing browser instance.
      if (
        maybeContext.parentId === null &&
        frame.parentId !== null &&
        frame.parentId !== undefined
      ) {
        maybeContext.parentId = frame.parentId;
      }
    }
    if (maybeContext === undefined && frame.parentId !== undefined) {
      // Restore not yet known nested frames. The top-level frame is created when the
      // target is attached.
      const parentBrowsingContext = this.#browsingContextStorage.getContext(
        frame.parentId,
      );
      BrowsingContextImpl.create(
        frame.id,
        frame.parentId,
        this.userContext,
        parentBrowsingContext.cdpTarget,
        this.#eventManager,
        this.#browsingContextStorage,
        this.#realmStorage,
        this.contextConfigStorage,
        frame.url,
        undefined,
        this.#logger,
      );
    }
    frameTree.childFrames?.map((frameTree) =>
      this.#restoreFrameTreeState(frameTree),
    );
  }

  async toggleFetchIfNeeded() {
    const stages = this.#networkStorage.getInterceptionStages(this.topLevelId);

    if (
      this.#fetchDomainStages.request === stages.request &&
      this.#fetchDomainStages.response === stages.response &&
      this.#fetchDomainStages.auth === stages.auth
    ) {
      return;
    }
    const patterns: Protocol.Fetch.EnableRequest['patterns'] = [];

    this.#fetchDomainStages = stages;
    if (stages.request || stages.auth) {
      // CDP quirk we need request interception when we intercept auth
      patterns.push({
        urlPattern: '*',
        requestStage: 'Request',
      });
    }
    if (stages.response) {
      patterns.push({
        urlPattern: '*',
        requestStage: 'Response',
      });
    }
    if (patterns.length) {
      await this.#cdpClient.sendCommand('Fetch.enable', {
        patterns,
        handleAuthRequests: stages.auth,
      });
    } else {
      const blockedRequest = this.#networkStorage
        .getRequestsByTarget(this)
        .filter((request) => request.interceptPhase);
      void Promise.allSettled(
        blockedRequest.map((request) => request.waitNextPhase),
      )
        .then(async () => {
          const blockedRequest = this.#networkStorage
            .getRequestsByTarget(this)
            .filter((request) => request.interceptPhase);
          if (blockedRequest.length) {
            return await this.toggleFetchIfNeeded();
          }
          return await this.#cdpClient.sendCommand('Fetch.disable');
        })
        .catch((error) => {
          this.#logger?.(LogType.bidi, 'Disable failed', error);
        });
    }
  }

  /**
   * Toggles CDP "Fetch" domain and enable/disable network cache.
   */
  async toggleNetworkIfNeeded(): Promise<void> {
    // Although the Network domain remains active, Fetch domain activation and caching
    // settings should be managed dynamically.
    try {
      await Promise.all([
        this.toggleSetCacheDisabled(),
        this.toggleFetchIfNeeded(),
      ]);
    } catch (err) {
      this.#logger?.(LogType.debugError, err);
      if (!this.#isExpectedError(err)) {
        throw err;
      }
    }
  }

  async toggleSetCacheDisabled(disable?: boolean) {
    const defaultCacheDisabled =
      this.#networkStorage.defaultCacheBehavior === 'bypass';
    const cacheDisabled = disable ?? defaultCacheDisabled;

    if (this.#cacheDisableState === cacheDisabled) {
      return;
    }
    this.#cacheDisableState = cacheDisabled;
    try {
      await this.#cdpClient.sendCommand('Network.setCacheDisabled', {
        cacheDisabled,
      });
    } catch (err) {
      this.#logger?.(LogType.debugError, err);
      this.#cacheDisableState = !cacheDisabled;
      if (!this.#isExpectedError(err)) {
        throw err;
      }
    }
  }

  async toggleDeviceAccessIfNeeded(): Promise<void> {
    const enabled = this.isSubscribedTo(
      Bluetooth.EventNames.RequestDevicePromptUpdated,
    );
    if (this.#deviceAccessEnabled === enabled) {
      return;
    }

    this.#deviceAccessEnabled = enabled;
    try {
      await this.#cdpClient.sendCommand(
        enabled ? 'DeviceAccess.enable' : 'DeviceAccess.disable',
      );
    } catch (err) {
      this.#logger?.(LogType.debugError, err);
      this.#deviceAccessEnabled = !enabled;
      if (!this.#isExpectedError(err)) {
        throw err;
      }
    }
  }

  async togglePreloadIfNeeded(): Promise<void> {
    const enabled = this.isSubscribedTo(
      Speculation.EventNames.PrefetchStatusUpdated,
    );
    if (this.#preloadEnabled === enabled) {
      return;
    }

    this.#preloadEnabled = enabled;
    try {
      await this.#cdpClient.sendCommand(
        enabled ? 'Preload.enable' : 'Preload.disable',
      );
    } catch (err) {
      this.#logger?.(LogType.debugError, err);
      this.#preloadEnabled = !enabled;
      if (!this.#isExpectedError(err)) {
        throw err;
      }
    }
  }

  /**
   * Heuristic checking if the error is due to the session being closed. If so, ignore the
   * error.
   */
  #isExpectedError(err: unknown): boolean {
    const error = err as {code?: unknown; message?: unknown};
    return (
      (error.code === -32001 &&
        error.message === 'Session with given id not found.') ||
      this.#cdpClient.isCloseError(err)
    );
  }

  #setEventListeners() {
    this.#cdpClient.on('*', (event, params) => {
      // We may encounter uses for EventEmitter other than CDP events,
      // which we want to skip.
      if (typeof event !== 'string') {
        return;
      }
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: `goog:cdp.${event}`,
          params: {
            event,
            params,
            session: this.cdpSessionId,
          },
        },
        this.id,
      );
    });
  }

  async #enableFetch(stages: FetchStages) {
    const patterns: Protocol.Fetch.EnableRequest['patterns'] = [];

    if (stages.request || stages.auth) {
      // CDP quirk we need request interception when we intercept auth
      patterns.push({
        urlPattern: '*',
        requestStage: 'Request',
      });
    }
    if (stages.response) {
      patterns.push({
        urlPattern: '*',
        requestStage: 'Response',
      });
    }
    if (patterns.length) {
      const oldStages = this.#fetchDomainStages;
      this.#fetchDomainStages = stages;
      try {
        await this.#cdpClient.sendCommand('Fetch.enable', {
          patterns,
          handleAuthRequests: stages.auth,
        });
      } catch {
        this.#fetchDomainStages = oldStages;
      }
    }
  }

  async #disableFetch() {
    const blockedRequest = this.#networkStorage
      .getRequestsByTarget(this)
      .filter((request) => request.interceptPhase);

    if (blockedRequest.length === 0) {
      this.#fetchDomainStages = {
        request: false,
        response: false,
        auth: false,
      };
      await this.#cdpClient.sendCommand('Fetch.disable');
    }
  }

  async toggleNetwork() {
    // TODO: respect the data collectors once CDP Network domain is enabled on-demand:
    // const networkEnable = this.#networkStorage.getCollectorsForBrowsingContext(this.topLevelId).length > 0;

    const stages = this.#networkStorage.getInterceptionStages(this.topLevelId);
    const fetchEnable = Object.values(stages).some((value) => value);
    const fetchChanged =
      this.#fetchDomainStages.request !== stages.request ||
      this.#fetchDomainStages.response !== stages.response ||
      this.#fetchDomainStages.auth !== stages.auth;

    this.#logger?.(
      LogType.debugInfo,
      'Toggle Network',
      `Fetch (${fetchEnable}) ${fetchChanged}`,
    );

    if (fetchEnable && fetchChanged) {
      await this.#enableFetch(stages);
    }
    if (!fetchEnable && fetchChanged) {
      await this.#disableFetch();
    }
  }

  /**
   * All the ProxyChannels from all the preload scripts of the given
   * BrowsingContext.
   */
  getChannels(): ChannelProxy[] {
    return this.#preloadScriptStorage
      .find()
      .flatMap((script) => script.channels);
  }

  async #updateWindowId() {
    const {windowId} = await this.#browserCdpClient.sendCommand(
      'Browser.getWindowForTarget',
      {targetId: this.id},
    );
    this.#windowId = windowId;
  }

  /** Loads all top-level preload scripts. */
  async #initAndEvaluatePreloadScripts() {
    await Promise.all(
      this.#preloadScriptStorage
        .find({
          // Needed for OOPIF
          targetId: this.topLevelId,
        })
        .map((script) => {
          return script.initInTarget(this, true);
        }),
    );
  }

  async setDeviceMetricsOverride(
    viewport: BrowsingContext.Viewport | null,
    devicePixelRatio: number | null,
    screenOrientation: Emulation.ScreenOrientation | null,
    screenArea: Emulation.ScreenArea | null,
  ) {
    if (
      viewport === null &&
      devicePixelRatio === null &&
      screenOrientation === null &&
      screenArea === null
    ) {
      await this.cdpClient.sendCommand('Emulation.clearDeviceMetricsOverride');
      return;
    }

    const metricsOverride: Protocol.Emulation.SetDeviceMetricsOverrideRequest =
      {
        width: viewport?.width ?? 0,
        height: viewport?.height ?? 0,
        deviceScaleFactor: devicePixelRatio ?? 0,
        screenOrientation:
          this.#toCdpScreenOrientationAngle(screenOrientation) ?? undefined,
        mobile: false,
        screenWidth: screenArea?.width,
        screenHeight: screenArea?.height,
      };

    await this.cdpClient.sendCommand(
      'Emulation.setDeviceMetricsOverride',
      metricsOverride,
    );
  }

  /**
   * Immediately schedules all the required commands to configure user context
   * configuration and waits for them to finish. It's important to schedule them
   * in parallel, so that they are enqueued before any page's scripts.
   */
  async #setUserContextConfig(config: ContextConfig) {
    const promises = [];

    promises.push(
      this.#cdpClient
        .sendCommand('Page.setPrerenderingAllowed', {
          isAllowed: !config.prerenderingDisabled,
        })
        .catch(() => {
          // Ignore CDP errors, as the command is not supported by iframe targets or
          // prerendered pages. Generic catch, as the error can vary between CdpClient
          // implementations: Tab vs Puppeteer.
        }),
    );

    if (
      config.viewport !== undefined ||
      config.devicePixelRatio !== undefined ||
      config.screenOrientation !== undefined ||
      config.screenArea !== undefined
    ) {
      promises.push(
        this.setDeviceMetricsOverride(
          config.viewport ?? null,
          config.devicePixelRatio ?? null,
          config.screenOrientation ?? null,
          config.screenArea ?? null,
        ).catch(() => {
          // Ignore CDP errors, as the command is not supported by iframe targets. Generic
          // catch, as the error can vary between CdpClient implementations: Tab vs
          // Puppeteer.
        }),
      );
    }

    if (config.geolocation !== undefined && config.geolocation !== null) {
      promises.push(this.setGeolocationOverride(config.geolocation));
    }

    if (config.locale !== undefined) {
      promises.push(this.setLocaleOverride(config.locale));
    }

    if (config.timezone !== undefined) {
      promises.push(this.setTimezoneOverride(config.timezone));
    }

    if (config.extraHeaders !== undefined) {
      promises.push(this.setExtraHeaders(config.extraHeaders));
    }

    if (
      config.userAgent !== undefined ||
      config.locale !== undefined ||
      config.clientHints !== undefined
    ) {
      promises.push(
        this.setUserAgentAndAcceptLanguage(
          config.userAgent,
          config.locale,
          config.clientHints,
        ),
      );
    }

    if (config.scriptingEnabled !== undefined) {
      promises.push(this.setScriptingEnabled(config.scriptingEnabled));
    }

    if (config.acceptInsecureCerts !== undefined) {
      promises.push(
        this.cdpClient.sendCommand('Security.setIgnoreCertificateErrors', {
          ignore: config.acceptInsecureCerts,
        }),
      );
    }

    if (config.emulatedNetworkConditions !== undefined) {
      promises.push(
        this.setEmulatedNetworkConditions(config.emulatedNetworkConditions),
      );
    }

    if (config.maxTouchPoints !== undefined) {
      promises.push(this.setTouchOverride(config.maxTouchPoints));
    }

    await Promise.all(promises);
  }

  get topLevelId() {
    return (
      this.#browsingContextStorage.findTopLevelContextId(this.id) ?? this.id
    );
  }

  isSubscribedTo(moduleOrEvent: ChromiumBidi.EventNames): boolean {
    return this.#eventManager.subscriptionManager.isSubscribedTo(
      moduleOrEvent,
      this.topLevelId,
    );
  }

  #ignoreFileDialog(): boolean {
    const config = this.contextConfigStorage.getActiveConfig(
      this.topLevelId,
      this.userContext,
    );

    return (
      (config.userPromptHandler?.file ??
        config.userPromptHandler?.default ??
        Session.UserPromptHandlerType.Ignore) ===
      Session.UserPromptHandlerType.Ignore
    );
  }

  async setGeolocationOverride(
    geolocation:
      | Emulation.GeolocationCoordinates
      | Emulation.GeolocationPositionError
      | null,
  ): Promise<void> {
    if (geolocation === null) {
      await this.cdpClient.sendCommand('Emulation.clearGeolocationOverride');
    } else if ('type' in geolocation) {
      if (geolocation.type !== 'positionUnavailable') {
        // Unreachable. Handled by params parser.
        throw new UnknownErrorException(
          `Unknown geolocation error ${geolocation.type}`,
        );
      }
      // Omitting latitude, longitude or accuracy emulates position unavailable.
      await this.cdpClient.sendCommand('Emulation.setGeolocationOverride', {});
    } else if ('latitude' in geolocation) {
      await this.cdpClient.sendCommand('Emulation.setGeolocationOverride', {
        latitude: geolocation.latitude,
        longitude: geolocation.longitude,
        accuracy: geolocation.accuracy ?? 1,
        // `null` value is treated as "missing".
        altitude: geolocation.altitude ?? undefined,
        altitudeAccuracy: geolocation.altitudeAccuracy ?? undefined,
        heading: geolocation.heading ?? undefined,
        speed: geolocation.speed ?? undefined,
      });
    } else {
      // Unreachable. Handled by params parser.
      throw new UnknownErrorException(
        'Unexpected geolocation coordinates value',
      );
    }
  }

  async setTouchOverride(maxTouchPoints: number | null): Promise<void> {
    const touchEmulationParams: Protocol.Emulation.SetTouchEmulationEnabledRequest =
      {
        enabled: maxTouchPoints !== null,
      };
    if (maxTouchPoints !== null) {
      touchEmulationParams.maxTouchPoints = maxTouchPoints;
    }
    await this.cdpClient.sendCommand(
      'Emulation.setTouchEmulationEnabled',
      touchEmulationParams,
    );
  }

  #toCdpScreenOrientationAngle(
    orientation: Emulation.ScreenOrientation | null,
  ): Protocol.Emulation.ScreenOrientation | null {
    if (orientation === null) {
      return null;
    }
    // https://w3c.github.io/screen-orientation/#the-current-screen-orientation-type-and-angle
    if (orientation.natural === Emulation.ScreenOrientationNatural.Portrait) {
      switch (orientation.type) {
        case 'portrait-primary':
          return {
            angle: 0,
            type: 'portraitPrimary',
          };
        case 'landscape-primary':
          return {
            angle: 90,
            type: 'landscapePrimary',
          };
        case 'portrait-secondary':
          return {
            angle: 180,
            type: 'portraitSecondary',
          };
        case 'landscape-secondary':
          return {
            angle: 270,
            type: 'landscapeSecondary',
          };
        default:
          // Unreachable.
          throw new UnknownErrorException(
            `Unexpected screen orientation type ${orientation.type}`,
          );
      }
    }
    if (orientation.natural === Emulation.ScreenOrientationNatural.Landscape) {
      switch (orientation.type) {
        case 'landscape-primary':
          return {
            angle: 0,
            type: 'landscapePrimary',
          };
        case 'portrait-primary':
          return {
            angle: 90,
            type: 'portraitPrimary',
          };
        case 'landscape-secondary':
          return {
            angle: 180,
            type: 'landscapeSecondary',
          };
        case 'portrait-secondary':
          return {
            angle: 270,
            type: 'portraitSecondary',
          };
        default:
          // Unreachable.
          throw new UnknownErrorException(
            `Unexpected screen orientation type ${orientation.type}`,
          );
      }
    }
    // Unreachable.
    throw new UnknownErrorException(
      `Unexpected orientation natural ${orientation.natural}`,
    );
  }

  async setLocaleOverride(locale: string | null): Promise<void> {
    if (locale === null) {
      await this.cdpClient.sendCommand('Emulation.setLocaleOverride', {});
    } else {
      await this.cdpClient.sendCommand('Emulation.setLocaleOverride', {
        locale,
      });
    }
  }

  async setScriptingEnabled(scriptingEnabled: false | null): Promise<void> {
    await this.cdpClient.sendCommand('Emulation.setScriptExecutionDisabled', {
      value: scriptingEnabled === false,
    });
  }

  async setTimezoneOverride(timezone: string | null): Promise<void> {
    if (timezone === null) {
      await this.cdpClient.sendCommand('Emulation.setTimezoneOverride', {
        // If empty, disables the override and restores default host system timezone.
        timezoneId: '',
      });
    } else {
      await this.cdpClient.sendCommand('Emulation.setTimezoneOverride', {
        timezoneId: timezone,
      });
    }
  }

  async setExtraHeaders(headers: Protocol.Network.Headers): Promise<void> {
    await this.cdpClient.sendCommand('Network.setExtraHTTPHeaders', {
      headers,
    });
  }

  async setUserAgentAndAcceptLanguage(
    userAgent: string | null | undefined,
    acceptLanguage: string | null | undefined,
    clientHints?: UAClientHints.UserAgentClientHints.ClientHintsMetadata | null,
  ): Promise<void> {
    const userAgentMetadata = clientHints
      ? {
          brands: clientHints.brands?.map((b) => ({
            brand: b.brand,
            version: b.version,
          })),
          fullVersionList: clientHints.fullVersionList,
          platform: clientHints.platform ?? '',
          platformVersion: clientHints.platformVersion ?? '',
          architecture: clientHints.architecture ?? '',
          model: clientHints.model ?? '',
          mobile: clientHints.mobile ?? false,
          bitness: clientHints.bitness ?? undefined,
          wow64: clientHints.wow64 ?? undefined,
          formFactors: clientHints.formFactors ?? undefined,
        }
      : undefined;

    await this.cdpClient.sendCommand('Emulation.setUserAgentOverride', {
      // `userAgent` is required if `userAgentMetadata` is provided.
      userAgent: userAgent || (userAgentMetadata ? this.#defaultUserAgent : ''),
      acceptLanguage: acceptLanguage ?? undefined,
      // We need to provide the platform to enable platform emulation.
      // Note that the value might be different from the one expected by the
      // legacy `navigator.platform` (e.g. `Win32` vs `Windows`).
      // https://github.com/w3c/webdriver-bidi/issues/1065
      platform: clientHints?.platform ?? undefined,
      userAgentMetadata,
    });
  }

  async setEmulatedNetworkConditions(
    networkConditions: Emulation.NetworkConditions | null,
  ): Promise<void> {
    if (networkConditions !== null && networkConditions.type !== 'offline') {
      throw new UnsupportedOperationException(
        `Unsupported network conditions ${networkConditions.type}`,
      );
    }

    await Promise.all([
      this.cdpClient.sendCommand('Network.emulateNetworkConditionsByRule', {
        offline: networkConditions?.type === 'offline',
        matchedNetworkConditions: [
          {
            urlPattern: '',
            latency: 0,
            downloadThroughput: -1,
            uploadThroughput: -1,
          },
        ],
      }),
      this.cdpClient.sendCommand('Network.overrideNetworkState', {
        offline: networkConditions?.type === 'offline',
        // TODO: restore the original `latency` value when emulation is removed.
        latency: 0,
        downloadThroughput: -1,
        uploadThroughput: -1,
      }),
    ]);
  }
}
