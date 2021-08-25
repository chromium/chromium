export interface CdpError {
  code: number;
  message: string;
}

export interface CdpMessage {
  sessionId?: string;
  id?: number;
  result?: object;
  error?: CdpError;
  method?: string;
  params?: object;
}
