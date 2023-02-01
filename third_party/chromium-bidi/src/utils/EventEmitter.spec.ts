/**
 * Copyright 2020 Google Inc. All rights reserved.
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

import {EventEmitter} from './EventEmitter.js';
import {expect} from 'chai';
import sinon from 'sinon';

describe('EventEmitter', () => {
  type Events = {
    foo: undefined;
    bar: undefined;
  };
  let emitter: EventEmitter<Events>;

  beforeEach(() => {
    emitter = new EventEmitter();
  });

  describe('on', () => {
    const onTests = (methodName: 'on'): void => {
      it(`${methodName}: adds an event listener that is fired when the event is emitted`, () => {
        const listener = sinon.spy();
        emitter[methodName]('foo', listener);
        emitter.emit('foo', undefined);
        expect(listener.callCount).to.equal(1);
      });

      it(`${methodName} sends the event data to the handler`, () => {
        const listener = sinon.spy();
        const data = {};
        emitter[methodName]('foo', listener);
        emitter.emit('foo', data);
        expect(listener.callCount).to.equal(1);
        expect(listener.firstCall.args[0]!).to.equal(data);
      });

      it(`${methodName}: supports chaining`, () => {
        const listener = sinon.spy();
        const returnValue = emitter[methodName]('foo', listener);
        expect(returnValue).to.equal(emitter);
      });
    };
    onTests('on');
  });

  describe('off', () => {
    const offTests = (methodName: 'off'): void => {
      it(`${methodName}: removes the listener so it is no longer called`, () => {
        const listener = sinon.spy();
        emitter.on('foo', listener);
        emitter.emit('foo', undefined);
        expect(listener.callCount).to.equal(1);
        emitter.off('foo', listener);
        emitter.emit('foo', undefined);
        expect(listener.callCount).to.equal(1);
      });

      it(`${methodName}: supports chaining`, () => {
        const listener = sinon.spy();
        emitter.on('foo', listener);
        const returnValue = emitter.off('foo', listener);
        expect(returnValue).to.equal(emitter);
      });
    };
    offTests('off');
  });

  describe('once', () => {
    it('only calls the listener once and then removes it', () => {
      const listener = sinon.spy();
      emitter.once('foo', listener);
      emitter.emit('foo', undefined);
      expect(listener.callCount).to.equal(1);
      emitter.emit('foo', undefined);
      expect(listener.callCount).to.equal(1);
    });

    it('supports chaining', () => {
      const listener = sinon.spy();
      const returnValue = emitter.once('foo', listener);
      expect(returnValue).to.equal(emitter);
    });
  });

  describe('emit', () => {
    it('calls all the listeners for an event', () => {
      const listener1 = sinon.spy();
      const listener2 = sinon.spy();
      const listener3 = sinon.spy();
      emitter.on('foo', listener1).on('foo', listener2).on('bar', listener3);

      emitter.emit('foo', undefined);

      expect(listener1.callCount).to.equal(1);
      expect(listener2.callCount).to.equal(1);
      expect(listener3.callCount).to.equal(0);
    });

    it('passes data through to the listener', () => {
      const listener = sinon.spy();
      emitter.on('foo', listener);
      const data = {};

      emitter.emit('foo', data);
      expect(listener.callCount).to.equal(1);
      expect(listener.firstCall.args[0]!).to.equal(data);
    });
  });
});
