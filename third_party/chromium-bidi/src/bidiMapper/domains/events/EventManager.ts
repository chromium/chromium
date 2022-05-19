import { BrowsingContext, Message } from '../protocol/bidiProtocolTypes';
import { IBidiServer } from '../../utils/bidiServer';

export interface IEventManager {
  sendEvent(
    event: Message.EventMessage,
    contextId: BrowsingContext.BrowsingContext | null
  ): Promise<void>;

  subscribe(
    events: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void>;

  unsubscribe(
    event: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void>;
}

export class EventManager implements IEventManager {
  // `null` means the event has a global subscription.
  #subscriptions: Map<BrowsingContext.BrowsingContext | null, Set<string>> =
    new Map();

  #bidiServer: IBidiServer;

  constructor(bidiServer: IBidiServer) {
    this.#bidiServer = bidiServer;
  }

  async sendEvent(
    event: Message.EventMessage,
    contextId: BrowsingContext.BrowsingContext | null
  ): Promise<void> {
    if (
      // Check if the event is allowed globally.
      this.#shouldSendEvent(event.method, null) ||
      // Check if the event is allowed for a given context.
      (contextId !== null && this.#shouldSendEvent(event.method, contextId))
    ) {
      await this.#bidiServer.sendMessage(event);
    }
  }

  #shouldSendEvent(
    eventMethod: string,
    contextId: BrowsingContext.BrowsingContext | null
  ): boolean {
    return (
      this.#subscriptions.has(contextId) &&
      this.#subscriptions.get(contextId)!.has(eventMethod)
    );
  }

  async subscribe(
    events: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void> {
    // Global subscription
    for (let event of events) {
      if (contextIds === null) this.#subscribe(event, null);
      else {
        for (let contextId of contextIds) {
          this.#subscribe(event, contextId);
        }
      }
    }
  }

  #subscribe(
    event: string,
    contextId: BrowsingContext.BrowsingContext | null
  ): void {
    if (!this.#subscriptions.has(contextId))
      this.#subscriptions.set(contextId, new Set());
    this.#subscriptions.get(contextId)!.add(event);
  }

  async unsubscribe(
    events: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void> {
    for (let event of events) {
      if (contextIds === null) this.#unsubscribe(event, null);
      else {
        for (let contextId of contextIds) {
          this.#unsubscribe(event, contextId);
        }
      }
    }
  }

  #unsubscribe(
    event: string,
    contextId: BrowsingContext.BrowsingContext | null
  ): void {
    const subscription = this.#subscriptions.get(contextId);
    subscription?.delete(event);
    if (subscription?.size === 0) {
      this.#subscriptions.delete(contextId);
    }
  }
}
