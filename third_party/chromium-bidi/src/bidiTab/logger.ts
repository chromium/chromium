/**
 * Copyright 2022 Google LLC.
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
import {type LogPrefix, LogType} from '../utils/log.js';

declare global {
  interface Window {
    // Set from the server side if verbose logging is required.
    sendDebugMessage?: ((message: string) => void) | null;
  }
}

export function log(
  logPrefix: LogPrefix,
): ((...messages: unknown[]) => void) | undefined {
  // Skip sending BiDi logs as they are already logged by ChromeDriver.
  if (
    !globalThis.window?.sendDebugMessage ||
    logPrefix.startsWith(LogType.bidi)
  ) {
    return;
  }

  return (...messages: unknown[]) => {
    globalThis.window?.sendDebugMessage?.(
      JSON.stringify({logType: logPrefix, messages}, null, 2),
    );
  };
}
