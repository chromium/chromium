import { ITransport } from '../utils/transport';
import WebSocket from 'ws';

export class WebSocketTransport implements ITransport {
  private _onMessage: ((message: string) => void) | null = null;

  constructor(private _ws: WebSocket) {
    this._ws.on('message', (message: string) => {
      if (this._onMessage) {
        this._onMessage.call(null, message);
      }
    });
  }

  setOnMessage(onMessage: (message: string) => void): void {
    this._onMessage = onMessage;
  }

  async sendMessage(message: string): Promise<void> {
    this._ws.send(message);
  }

  close() {
    this._onMessage = null;
    this._ws.close();
  }
}
