# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import SocketServer
import os
import socket
import ssl
import struct
import tempfile
import threading

from OpenSSL import crypto

class BlackHoleHandler(SocketServer.BaseRequestHandler):
  """This handler consumes all request input and makes no responses.
  """
  def handle(self):
    """Consume the request and then do nothing.
    """
    data = self.request.recv(4096)
    while len(data) > 0:
      data = self.request.recv(4096)

class InvalidTLSHandler(SocketServer.BaseRequestHandler):
  """This handler injects unencrypted TCP after a TLS handshake.
  """
  def handle(self):
    """Do a TLS handshake on the new connection then inject unencrypted bytes.
    """
    # ssl.wrap_socket will automatically do a TLS handshake before returning.
    ssl_conn = ssl.wrap_socket(self.request, server_side=True,
      **_CreateSelfSignedCert())
    self.request.sendall('this is unencrypted. oops')


class TCPResetHandler(SocketServer.BaseRequestHandler):
  """This handler sends TCP RST immediately after the connection is established.
  """
  def handle(self):
    """Reset the socket once connected.
    """
    # Setting these socket options tells Python to TCP RST the connection when
    # socket.close() is called instead of the default TCP FIN.
    self.request.recv(4096)
    self.request.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
      struct.pack('ii', 1, 0))
    self.request.close()


class TLSResetHandler(SocketServer.BaseRequestHandler):
  """This handler sends TCP RST immediately after the TLS handshake.
  """
  def handle(self):
    """Reset the socket after TLS handshake.
    """
    # ssl.wrap_socket will automatically do a TLS handshake before returning.
    ssl_conn = ssl.wrap_socket(self.request, server_side=True,
      **_CreateSelfSignedCert())
    # Allow the request to be sent.
    ssl_conn.recv(4096)
    # Setting these socket options tells the OS to TCP RST the connection when
    # socket.close() is called instead of the default TCP FIN.
    self.request.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
      struct.pack('ii', 1, 0))
    self.request.close()


class LocalEmulationServer:
  """This server is a simple wrapper for building servers with request handlers.

  This class wraps Python's basic network server stack, providing a simple API
  for Chrome-Proxy tests to use.
  Attributes:
    _port: the port to bind and listen to
    _handler_class: the handler class to handle new connections with
    _server: a reference to the underlying server
  """
  def __init__(self, port, handler_class, server_class=SocketServer.TCPServer):
    self._port = port
    self._handler_class = handler_class
    self._server_class = server_class
    self._server = None

  def StartAndReturn(self, timeout=30):
    """Start the server in a new thread and return once the server is running.

    A new server of the given server_class at init is started with the given
    handler. The server will listen forever in a new thread unless Shutdown() is
    called.

    Args:
      timeout: The timeout to start the server.
    """
    self._server = self._server_class(("0.0.0.0", self._port),
      self._handler_class)
    event = threading.Event()

    def StartServer():
      self._server.serve_forever()

    def WaitForRunning(event):
      while not event.is_set():
        try:
          s = socket.create_connection(("127.0.0.1", self._port))
          event.set()
          s.close()
        except:
          pass

    start_thread = threading.Thread(target=StartServer)
    start_thread.daemon = True
    start_thread.start()

    thread = threading.Thread(target=WaitForRunning, args=[event])
    thread.start()
    if not event.wait(timeout=timeout):
      event.set()
      raise Exception("Emulation server didn't start in %d seconds" % timeout)

  def Shutdown(self):
    """Shutdown a running server.

    Calls shutdown() on the underlying server instance, closing the spawned
    thread.
    """
    if self._server:
      self._server.shutdown()
      self._server.server_close()

def _CreateSelfSignedCert():
  """Creates a self-signed certificate and key in the machine's temp directory.

  Returns:
    a dict suitable for expansion to many ssl functions
  """
  temp_dir = tempfile.gettempdir()
  cert_path = os.path.join(temp_dir, "selfsigned.crt")
  pkey_path = os.path.join(temp_dir, "private.key")

  # Create a private key pair.
  pk = crypto.PKey()
  pk.generate_key(crypto.TYPE_RSA, 3072)

  # Create a certificate and sign it.
  cert = crypto.X509()
  cert.get_subject().C = "US"
  cert.get_subject().ST = "California"
  cert.get_subject().L = "Mountain View"
  cert.get_subject().O = "Fake Company Name"
  cert.get_subject().OU = "Fake Company Org Name"
  cert.get_subject().CN = "localhost"
  cert.set_serial_number(1337)
  cert.gmtime_adj_notBefore(0)
  cert.gmtime_adj_notAfter(60*60*24*365) # 1 year
  cert.set_issuer(cert.get_subject())
  cert.set_pubkey(pk)
  cert.sign(pk, 'sha1')

  # Dump to files.
  with open(cert_path, "wt") as cert_f:
    cert_f.write(crypto.dump_certificate(crypto.FILETYPE_PEM, cert))
  with open(pkey_path, "wt") as pkey_f:
    pkey_f.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, pk))

  # Return the filenames in a dict that can be expanded into ssl function args.
  return {"certfile": cert_path, "keyfile": pkey_path}
