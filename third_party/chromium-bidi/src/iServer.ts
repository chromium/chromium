export interface IServer {
    setOnMessage: (handler: ((messageObj: any) => Promise<void>)) => void
    sendMessage: (messageObj: any) => Promise<void>
}
