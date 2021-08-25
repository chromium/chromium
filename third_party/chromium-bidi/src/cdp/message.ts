export interface CdpError {
  code: number;
  message: string;
}

export interface CdpMessage {
  sessionId?: string;
  id?: number;
  result?: {};
  error?: CdpError;
  method?: string;
  params?: {};
}
