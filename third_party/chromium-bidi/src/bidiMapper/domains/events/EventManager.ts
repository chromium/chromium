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
  private _subscriptions: Map<
    BrowsingContext.BrowsingContext | null,
    Set<string>
  > = new Map();

  constructor(private _bidiServer: IBidiServer) {}

  async sendEvent(
    event: Message.EventMessage,
    contextId: BrowsingContext.BrowsingContext | null
  ): Promise<void> {
    if (
      // Check if the event is allowed globally.
      this._shouldSendEvent(event.method, null) ||
      // Check if the event is allowed for a given context.
      (contextId !== null && this._shouldSendEvent(event.method, contextId))
    ) {
      await this._bidiServer.sendMessage(event);
    }
  }

  private _shouldSendEvent(
    eventMethod: string,
    contextId: BrowsingContext.BrowsingContext | null
  ): boolean {
    return (
      this._subscriptions.has(contextId) &&
      this._subscriptions.get(contextId)!.has(eventMethod)
    );
  }

  async subscribe(
    events: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void> {
    // Global subscription
    for (let event of events) {
      if (contextIds === null) this._subscribe(event, null);
      else {
        for (let contextId of contextIds) {
          this._subscribe(event, contextId);
        }
      }
    }
  }

  private _subscribe(
    event: string,
    contextId: BrowsingContext.BrowsingContext | null
  ): void {
    if (!this._subscriptions.has(contextId))
      this._subscriptions.set(contextId, new Set());
    this._subscriptions.get(contextId)!.add(event);
  }

  async unsubscribe(
    events: string[],
    contextIds: BrowsingContext.BrowsingContext[] | null
  ): Promise<void> {
    throw new Error('EventManager.unsubscribe is not implemented');
  }
}
