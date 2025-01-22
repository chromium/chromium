/**
 * Copyright 2020 Google LLC.
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
import sinon from 'sinon';

import {EventEmitter} from './EventEmitter.js';

describe('EventEmitter', () => {
  interface Events extends Record<string | symbol, unknown> {
    foo: undefined;
    bar: string;
  }
  let emitter: EventEmitter<Events>;

  beforeEach(() => {
    emitter = new EventEmitter();
  });

  describe('on', () => {
    it(`adds an event listener that is fired when the event is emitted`, () => {
      const listener = sinon.spy();
      emitter.on('foo', listener);
      emitter.emit('foo', undefined);
      expect(listener.callCount).to.equal(1);
    });

    it(`sends the event data to the handler`, () => {
      const listener = sinon.spy();
      const data = 'data';
      emitter.on('bar', listener);
      emitter.emit('bar', data);
      expect(listener.callCount).to.equal(1);
      expect(listener.firstCall.args[0]).to.equal(data);
    });

    it(`supports chaining`, () => {
      const listener = sinon.spy();
      const returnValue = emitter.on('foo', listener);
      expect(returnValue).to.equal(emitter);
    });
  });

  describe('off', () => {
    it(`removes the listener so it is no longer called`, () => {
      const listener = sinon.spy();
      emitter.on('foo', listener);
      emitter.emit('foo', undefined);
      expect(listener.callCount).to.equal(1);
      emitter.off('foo', listener);
      emitter.emit('foo', undefined);
      expect(listener.callCount).to.equal(1);
    });

    it(`supports chaining`, () => {
      const listener = sinon.spy();
      emitter.on('foo', listener);
      const returnValue = emitter.off('foo', listener);
      expect(returnValue).to.equal(emitter);
    });
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
      const data = undefined;

      emitter.emit('foo', data);
      expect(listener.callCount).to.equal(1);
      expect(listener.firstCall.args[0]).to.equal(data);
    });
  });

  describe('removeAllListeners', () => {
    it('for different events', () => {
      const listener1 = sinon.spy();
      const listener2 = sinon.spy();
      const listener3 = sinon.spy();
      emitter.on('foo', listener1).on('foo', listener2).on('bar', listener3);
      emitter.removeAllListeners();

      emitter.emit('foo', undefined);
      expect(listener1.callCount).to.equal(0);
      expect(listener2.callCount).to.equal(0);
      expect(listener3.callCount).to.equal(0);
    });

    it('for a single event', () => {
      const listener1 = sinon.spy();
      const listener2 = sinon.spy();
      const listener3 = sinon.spy();
      emitter.on('foo', listener1).on('foo', listener2).on('bar', listener3);
      emitter.removeAllListeners('bar');

      emitter.emit('foo', undefined);
      expect(listener1.callCount).to.equal(1);
      expect(listener2.callCount).to.equal(1);
      expect(listener3.callCount).to.equal(0);
    });
  });
});
