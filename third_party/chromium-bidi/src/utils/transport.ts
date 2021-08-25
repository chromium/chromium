/**
 * Represents a low-level transport mechanism for raw text messages like
 * a WebSocket, pipe, or Window binding.
 */
export interface ITransport {
  setOnMessage: (handler: (message: string) => Promise<void>) => void;
  sendMessage: (message: string) => Promise<void>;
  close(): void;
}
