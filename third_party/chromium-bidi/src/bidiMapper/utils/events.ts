// A simple polyfill for Node's EventEmitter class that is usable in the browser.
// Add new functionality to this class as needed.
export class EventEmitter {
  private _handlers: Map<string, Set<Function>>;

  constructor() {
    this._handlers = new Map();
  }

  public on(event: string, handler: Function): void {
    if (!this._handlers.has(event)) {
      this._handlers.set(event, new Set());
    }
    this._handlers.get(event).add(handler);
  }

  public emit(event: string, ...args: any[]): void {
    const handlers = this._handlers.get(event);
    if (handlers) {
      for (const handler of handlers) {
        try {
          handler.apply(null, args);
        } catch (e) {
          continue;
        }
      }
    }
  }
}
