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

import * as chai from 'chai';
import { BrowsingContextParser } from './browsingContextParser';
import { BrowsingContext } from '../bidiProtocolTypes';

const expect = chai.expect;

describe('test BrowsingContextParser', function () {
  describe('CreateCommand', function () {
    describe('parseParams', function () {
      it('window type should be parsed', async function () {
        expect(
          BrowsingContextParser.CreateCommand.parseParams({
            type: 'window',
          })
        ).to.deep.equal({
          type: BrowsingContext.CreateParametersType.window,
        });
      });
      it('tab type should be parsed', async function () {
        expect(
          BrowsingContextParser.CreateCommand.parseParams({
            type: 'tab',
          })
        ).to.deep.equal({
          type: BrowsingContext.CreateParametersType.tab,
        });
      });
      it('unknown type should not be parsed', async function () {
        expect(() =>
          BrowsingContextParser.CreateCommand.parseParams({
            type: 'unknown',
          })
        )
          .to.throw()
          .with.property('error', 'invalid argument');
      });
      it('number type should not be parsed', async function () {
        expect(() =>
          BrowsingContextParser.CreateCommand.parseParams({
            type: 42,
          })
        )
          .to.throw()
          .with.property('error', 'invalid argument');
      });
      it('missing type should not be parsed', async function () {
        expect(() => BrowsingContextParser.CreateCommand.parseParams({}))
          .to.throw()
          .with.property('error', 'invalid argument');
      });
      it('undefined params should not be parsed', async function () {
        expect(() => BrowsingContextParser.CreateCommand.parseParams(undefined))
          .to.throw()
          .with.property('error', 'invalid argument');
      });
    });
  });
  describe('CloseCommand', function () {
    describe('parseParams', function () {
      it('context should be parsed', async function () {
        expect(
          BrowsingContextParser.CloseCommand.parseParams({
            context: 'some_context',
          })
        ).to.deep.equal({
          context: 'some_context',
        });
      });
      it('unknown params should be ignored', async function () {
        expect(
          BrowsingContextParser.CloseCommand.parseParams({
            context: 'some_context',
            unknown_param: 'unknown_value',
          })
        ).to.deep.equal({
          context: 'some_context',
        });
      });
      it('missing context should not be parsed', async function () {
        expect(() => BrowsingContextParser.CloseCommand.parseParams({}))
          .to.throw()
          .with.property('error', 'invalid argument');
      });
      it('undefined params should not be parsed', async function () {
        expect(() => BrowsingContextParser.CreateCommand.parseParams(undefined))
          .to.throw()
          .with.property('error', 'invalid argument');
      });
    });
  });
  describe('BrowsingContext', function () {
    describe('parseOptionalList', function () {
      it('array of strings should be parsed', async function () {
        const arrayOfStrings = [
          'some_browsing_context',
          new String('another_browsing_context'),
        ];
        expect(
          BrowsingContextParser.BrowsingContext.parseOptionalList(
            arrayOfStrings
          )
        ).to.deep.equal(arrayOfStrings);
      });

      it('empty array should be parsed', async function () {
        expect(
          BrowsingContextParser.BrowsingContext.parseOptionalList([])
        ).to.deep.equal([]);
      });

      it('undefined should be parsed', async function () {
        expect(
          BrowsingContextParser.BrowsingContext.parseOptionalList(undefined)
        ).to.equal(undefined);
      });

      it('array of numbers should throw error', async function () {
        expect(() =>
          BrowsingContextParser.BrowsingContext.parseOptionalList([42])
        )
          .to.throw()
          .with.property('error', 'invalid argument');
      });
    });

    describe('parse', function () {
      it('undefined should throw error', async function () {
        expect(() => BrowsingContextParser.BrowsingContext.parse(undefined))
          .to.throw()
          .with.property('error', 'invalid argument');
      });

      it('number should throw error', async function () {
        expect(() => BrowsingContextParser.BrowsingContext.parse(42))
          .to.throw()
          .with.property('error', 'invalid argument');
      });

      it('bool should throw error', async function () {
        expect(() => BrowsingContextParser.BrowsingContext.parse(true))
          .to.throw()
          .with.property('error', 'invalid argument');
      });

      it('object should throw error', async function () {
        expect(() => BrowsingContextParser.BrowsingContext.parse({}))
          .to.throw()
          .with.property('error', 'invalid argument');
      });

      it('array should throw error', async function () {
        expect(() => BrowsingContextParser.BrowsingContext.parse([]))
          .to.throw()
          .with.property('error', 'invalid argument');
      });

      it('`new String()` should be parsed', async function () {
        expect(
          BrowsingContextParser.BrowsingContext.parse(
            new String('some_browsing_context')
          )
        ).to.deep.equal(new String('some_browsing_context'));
      });

      it('string literal should be parsed', async function () {
        expect(
          BrowsingContextParser.BrowsingContext.parse('some_browsing_context')
        ).to.equal('some_browsing_context');
      });
    });
  });
});
