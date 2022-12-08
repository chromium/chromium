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

import {Message} from './types';

export class ErrorResponseClass implements Message.ErrorResult {
  protected constructor(
    error: Message.ErrorCode,
    message: string,
    stacktrace?: string
  ) {
    this.error = error;
    this.message = message;
    this.stacktrace = stacktrace;
  }

  readonly error: Message.ErrorCode;
  readonly message: string;
  readonly stacktrace?: string;

  toErrorResponse(commandId: number): Message.CommandResponse {
    return {
      id: commandId,
      error: this.error,
      message: this.message,
      stacktrace: this.stacktrace,
    };
  }
}

export class UnknownException extends ErrorResponseClass {
  constructor(message: string, stacktrace?: string) {
    super('unknown error', message, stacktrace);
  }
}

export class UnknownCommandException extends ErrorResponseClass {
  constructor(message: string, stacktrace?: string) {
    super('unknown command', message, stacktrace);
  }
}

export class InvalidArgumentException extends ErrorResponseClass {
  constructor(message: string, stacktrace?: string) {
    super('invalid argument', message, stacktrace);
  }
}

export class NoSuchFrameException extends ErrorResponseClass {
  constructor(message: string) {
    super('no such frame', message);
  }
}
