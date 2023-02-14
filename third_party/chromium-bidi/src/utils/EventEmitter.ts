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
import mitt, {Emitter, EventType, Handler, WildcardHandler} from 'mitt';

export class EventEmitter<Events extends Record<EventType, unknown>> {
  #emitter: Emitter<Events> = mitt();

  /**
   * Binds an event listener to fire when an event occurs.
   * @param event - the event type you'd like to listen to. Can be a string or symbol.
   * @param handler - the function to be called when the event occurs.
   * @returns `this` to enable you to chain method calls.
   */
  on(type: '*', handler: WildcardHandler<Events>): EventEmitter<Events>;
  on<Key extends keyof Events>(
    type: Key,
    handler: Handler<Events[Key]>
  ): EventEmitter<Events>;
  on(type: any, handler: any): EventEmitter<Events> {
    this.#emitter.on(type, handler);
    return this;
  }

  /**
   * Like `on` but the listener will only be fired once and then it will be removed.
   * @param event - the event you'd like to listen to
   * @param handler - the handler function to run when the event occurs
   * @returns `this` to enable you to chain method calls.
   */
  once(event: EventType, handler: Handler): EventEmitter<Events> {
    const onceHandler: Handler = (eventData) => {
      handler(eventData);
      this.off(event, onceHandler);
    };

    return this.on(event, onceHandler);
  }

  /**
   * Removes an event listener from firing.
   * @param event - the event type you'd like to stop listening to.
   * @param handler - the function that should be removed.
   * @returns `this` to enable you to chain method calls.
   */
  off(type: '*', handler: WildcardHandler<Events>): EventEmitter<Events>;
  off<Key extends keyof Events>(
    type: Key,
    handler: Handler<Events[Key]>
  ): EventEmitter<Events>;
  off(type: any, handler: any): EventEmitter<Events> {
    this.#emitter.off(type, handler);
    return this;
  }

  /**
   * Emits an event and call any associated listeners.
   *
   * @param event - the event you'd like to emit
   * @param eventData - any data you'd like to emit with the event
   * @returns `true` if there are any listeners, `false` if there are not.
   */
  emit<Key extends keyof Events>(event: Key, eventData: Events[Key]): void {
    this.#emitter.emit(event, eventData);
  }
}
