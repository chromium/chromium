'use strict';

direct_sockets_test(async (t, mockDirectSocketsService) => {
  const socket = new TCPSocket('127.0.0.1', 53);
  await promise_rejects_dom(t, "NetworkError", socket.opened);
}, "direct sockets (TCP) do not get blocked on permissions policy direct-sockets=(self)");

direct_sockets_test(async (t, mockDirectSocketsService) => {
  const socket = new UDPSocket({ remoteAddress: '127.0.0.1', remotePort: 53 });
  await promise_rejects_dom(t, "NetworkError", socket.opened);
}, "direct sockets (UDP) do not get blocked on permissions policy direct-sockets=(self)");
