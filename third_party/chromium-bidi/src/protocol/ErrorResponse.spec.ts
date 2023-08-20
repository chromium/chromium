/**
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
 */
import {expect} from 'chai';

import * as exceptionClasses from './ErrorResponse.js';
import {ErrorCode} from './webdriver-bidi.js';

describe('Exception', () => {
  const errors: {
    [key in ErrorCode]: undefined;
  } = {
    // keep-sorted start
    [ErrorCode.InvalidArgument]: undefined,
    [ErrorCode.InvalidSessionId]: undefined,
    [ErrorCode.MoveTargetOutOfBounds]: undefined,
    [ErrorCode.NoSuchAlert]: undefined,
    [ErrorCode.NoSuchElement]: undefined,
    [ErrorCode.NoSuchFrame]: undefined,
    [ErrorCode.NoSuchHandle]: undefined,
    [ErrorCode.NoSuchIntercept]: undefined,
    [ErrorCode.NoSuchNode]: undefined,
    [ErrorCode.NoSuchRequest]: undefined,
    [ErrorCode.NoSuchScript]: undefined,
    [ErrorCode.SessionNotCreated]: undefined,
    [ErrorCode.UnableToCaptureScreen]: undefined,
    [ErrorCode.UnableToCloseBrowser]: undefined,
    [ErrorCode.UnknownCommand]: undefined,
    [ErrorCode.UnknownError]: undefined,
    [ErrorCode.UnsupportedOperation]: undefined,
    // keep-sorted end
  };

  // XXX: Simplify with `Object.keys(ErrorCode)` once we switch to non-const enums.
  Object.keys(errors).forEach((errorCode) => {
    // Exception class name should be in PascalCase, e.g. InvalidArgumentException
    const exceptionClassName = `${errorCode
      .split(' ')
      .map((word) => word.charAt(0).toUpperCase() + word.slice(1))
      .join('')}Exception`;

    it(`should have an exception class for ErrorCode.${exceptionClassName}`, () => {
      expect(exceptionClasses).to.have.property(exceptionClassName);
      const ExceptionClass = (exceptionClasses as any)[exceptionClassName];
      expect(ExceptionClass.name.endsWith('Exception')).to.be.true;

      const instance = new ExceptionClass('my message', 'my stacktrace');
      expect(instance).to.have.property('error', errorCode);
      expect(instance).to.have.property('message', 'my message');
      expect(instance).to.have.property('stacktrace', 'my stacktrace');
    });
  });
});
