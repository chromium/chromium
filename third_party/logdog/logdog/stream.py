# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import collections
import contextlib
import json
import os
import posixpath
import socket
import sys
import threading
import time

from . import streamname, varint


if sys.platform == "win32":
  from ctypes import GetLastError


_PY2 = sys.version_info[0] == 2
_MAPPING = collections.Mapping if _PY2 else collections.abc.Mapping

_StreamParamsBase = collections.namedtuple(
    '_StreamParamsBase', ('name', 'type', 'content_type', 'tags'))


# Magic number at the beginning of a Butler stream
#
# See "ProtocolFrameHeaderMagic" in:
# <luci-go>/logdog/client/butlerlib/streamproto
BUTLER_MAGIC = b'BTLR1\x1e'


class StreamParams(_StreamParamsBase):
  """Defines the set of parameters to apply to a new stream."""

  # A text content stream.
  TEXT = 'text'
  # A binary content stream.
  BINARY = 'binary'
  # A datagram content stream.
  DATAGRAM = 'datagram'

  @classmethod
  def make(cls, **kwargs):
    """Returns (StreamParams): A new StreamParams instance with supplied values.

    Any parameter that isn't supplied will be set to None.

    Args:
      kwargs (dict): Named parameters to apply.
    """
    return cls(**{f: kwargs.get(f) for f in cls._fields})

  def validate(self):
    """Raises (ValueError): if the parameters are not valid."""
    streamname.validate_stream_name(self.name)

    if self.type not in (self.TEXT, self.BINARY, self.DATAGRAM):
      raise ValueError('Invalid type (%s)' % (self.type,))

    if self.tags is not None:
      if not isinstance(self.tags, _MAPPING):
        raise ValueError('Invalid tags type (%s)' % (self.tags,))
      for k, v in self.tags.items():
        streamname.validate_tag(k, v)

  def to_json(self):
    """Returns (str): The JSON representation of the StreamParams.

    Converts stream parameters to JSON for Butler consumption.

    Raises:
      ValueError: if these parameters are not valid.
    """
    self.validate()

    obj = {
        'name': self.name,
        'type': self.type,
    }

    def _maybe_add(key, value):
      if value is not None:
        obj[key] = value

    _maybe_add('contentType', self.content_type)
    _maybe_add('tags', self.tags)

    # Note that "dumps' will dump UTF-8 by default, which is what Butler wants.
    return json.dumps(obj, sort_keys=True, ensure_ascii=True, indent=None)


class StreamProtocolRegistry(object):
  """Registry of streamserver URI protocols and their client classes.
  """

  def __init__(self):
    self._registry = {}

  def register_protocol(self, protocol, client_cls):
    assert issubclass(client_cls, StreamClient)
    if self._registry.get(protocol) is not None:
      raise KeyError('Duplicate protocol registered.')
    self._registry[protocol] = client_cls

  def create(self, uri, **kwargs):
    """Returns (StreamClient): A stream client for the specified URI.

    This uses the default StreamProtocolRegistry to instantiate a StreamClient
    for the specified URI.

    Args:
      uri (str): The streamserver URI.
      kwargs: keyword arguments to forward to the stream. See
          StreamClient.__init__.

    Raises:
      ValueError: if the supplied URI references an invalid or improperly
          configured streamserver.
    """
    uri = uri.split(':', 1)
    if len(uri) != 2:
      raise ValueError('Invalid stream server URI [%s]' % (uri,))
    protocol, value = uri

    client_cls = self._registry.get(protocol)
    if not client_cls:
      raise ValueError('Unknown stream client protocol (%s)' % (protocol,))
    return client_cls._create(value, **kwargs)


# Default (global) registry.
_default_registry = StreamProtocolRegistry()
create = _default_registry.create


class StreamClient(object):
  """Abstract base class for a streamserver client.
  """

  class _StreamBase(object):
    """ABC for StreamClient streams."""

    def __init__(self, stream_client, params):
      self._stream_client = stream_client
      self._params = params

    @property
    def params(self):
      """Returns (StreamParams): The stream parameters."""
      return self._params

    @property
    def path(self):
      """Returns (streamname.StreamPath): The stream path.

      Raises:
        ValueError: if the stream path is invalid, or if the stream prefix is
            not defined in the client.
      """
      return self._stream_client.get_stream_path(self._params.name)

    def get_viewer_url(self):
      """Returns (str): The viewer URL for this stream.

      Raises:
        KeyError: if information needed to construct the URL is missing.
        ValueError: if the stream prefix or name do not form a valid stream
            path.
      """
      return self._stream_client.get_viewer_url(self._params.name)


  class _BasicStream(_StreamBase):
    """Wraps a basic file descriptor, offering "write" and "close"."""

    def __init__(self, stream_client, params, fd):
      super(StreamClient._BasicStream, self).__init__(stream_client, params)
      self._fd = fd

    @property
    def fd(self):
      return self._fd

    def fileno(self):
      return self._fd.fileno()

    def write(self, data):
      return self._fd.write(data)

    def close(self):
      return self._fd.close()


  class _TextStream(_BasicStream):
    """Extends _BasicStream, ensuring data written is UTF-8 text."""

    def __init__(self, stream_client, params, fd):
      super(StreamClient._TextStream, self).__init__(stream_client, params, fd)
      self._fd = fd

    def write(self, data):
      if _PY2 and isinstance(data, str):
        # byte string is unfortunately accepted in py2 because of
        # undifferentiated usage of `str` and `unicode` but it should be
        # discontinued in py3. User should switch to binary stream instead
        # if there's a need to write bytes.
        return self._fd.write(data)
      elif _PY2 and isinstance(data, unicode):
        return self._fd.write(data.encode('utf-8'))
      elif not _PY2 and isinstance(data, str):
        return self._fd.write(data.encode('utf-8'))
      else:
        raise ValueError(
            'expect str, got %r that is type %s' % (data, type(data),))


  class _DatagramStream(_StreamBase):
    """Wraps a stream object to write length-prefixed datagrams."""

    def __init__(self, stream_client, params, fd):
      super(StreamClient._DatagramStream, self).__init__(stream_client, params)
      self._fd = fd

    def send(self, data):
      varint.write_uvarint(self._fd, len(data))
      self._fd.write(data)

    def close(self):
      return self._fd.close()


  def __init__(self, project=None, prefix=None, coordinator_host=None,
               namespace=''):
    """Constructs a new base StreamClient instance.

    Args:
      project (str or None): If not None, the name of the log stream project.
      prefix (str or None): If not None, the log stream session prefix.
      coordinator_host (str or None): If not None, the name of the Coordinator
          host that this stream client is bound to. This will be used to
          construct viewer URLs for generated streams.
      namespace (str): The prefix to apply to all streams opened by this client.
    """
    self._project = project
    self._prefix = prefix
    self._coordinator_host = coordinator_host
    self._namespace = namespace

    self._name_lock = threading.Lock()
    self._names = set()

  @property
  def project(self):
    """Returns (str or None): The stream project, or None if not configured."""
    return self._project

  @property
  def prefix(self):
    """Returns (str or None): The stream prefix, or None if not configured."""
    return self._prefix

  @property
  def coordinator_host(self):
    """Returns (str or None): The coordinator host, or None if not configured.
    """
    return self._coordinator_host

  @property
  def namespace(self):
    """Returns (str): The namespace for all streams opened by this client.
    Empty if not configured.
    """
    return self._namespace

  def get_stream_path(self, name):
    """Returns (streamname.StreamPath): The stream path.

    Args:
      name (str): The name of the stream.

    Raises:
      KeyError: if information needed to construct the path is missing.
      ValueError: if the stream path is invalid, or if the stream prefix is
          not defined in the client.
    """
    if not self._prefix:
      raise KeyError('Stream prefix is not configured')
    return streamname.StreamPath.make(self._prefix, name)

  def get_viewer_url(self, name):
    """Returns (str): The LogDog viewer URL for the named stream.

    Args:
      name (str): The name of the stream. This can also be a query glob.

    Raises:
      KeyError: if information needed to construct the URL is missing.
      ValueError: if the stream prefix or name do not form a valid stream
          path.
    """
    if not self._coordinator_host:
      raise KeyError('Coordinator host is not configured')
    if not self._project:
      raise KeyError('Stream project is not configured')

    return streamname.get_logdog_viewer_url(
        self._coordinator_host,
        self._project,
        self.get_stream_path(name))

  def _register_new_stream(self, name):
    """Registers a new stream name.

    The Butler will internally reject any duplicate stream names. However, there
    isn't really feedback when this happens except a closed stream client. This
    is a client-side check to provide a more user-friendly experience in the
    event that a user attempts to register a duplicate stream name.

    Note that this is imperfect, as something else could register stream names
    with the same Butler instance and this library has no means of tracking.
    This is a best-effort experience, not a reliable check.

    Args:
      name (str): The name of the stream.

    Raises:
      ValueError if the stream name has already been registered.
    """
    with self._name_lock:
      if name in self._names:
        raise ValueError("Duplicate stream name [%s]" % (name,))
      self._names.add(name)

  @classmethod
  def _create(cls, value, **kwargs):
    """Returns (StreamClient): A new stream client instance.

    Validates the streamserver parameters and creates a new StreamClient
    instance that connects to them.

    Implementing classes must override this.
    """
    raise NotImplementedError()

  def _connect_raw(self):
    """Returns (file): A new file-like stream.

    Creates a new raw connection to the streamserver. This connection MUST not
    have any data written to it past initialization (if needed) when it has been
    returned.

    The file-like object must implement `write`, `fileno`, `flush`, and `close`.

    Implementing classes must override this.
    """
    raise NotImplementedError()

  def new_connection(self, params):
    """Returns (file): A new configured stream.

    The returned object implements (minimally) `write` and `close`.

    Creates a new LogDog stream with the specified parameters.

    Args:
      params (StreamParams): The parameters to use with the new connection.

    Raises:
      ValueError if the stream name has already been used, or if the parameters
      are not valid.
    """
    self._register_new_stream(params.name)
    params_bytes = params.to_json().encode('utf-8')

    fobj = self._connect_raw()
    fobj.write(BUTLER_MAGIC)
    varint.write_uvarint(fobj, len(params_bytes))
    fobj.write(params_bytes)
    return fobj

  @contextlib.contextmanager
  def text(self, name, **kwargs):
    """Context manager to create, use, and teardown a TEXT stream.

    This context manager creates a new butler TEXT stream with the specified
    parameters, yields it, and closes it on teardown.

    Args:
      name (str): the LogDog name of the stream.
      kwargs (dict): Log stream parameters. These may be any keyword arguments
          accepted by `open_text`.

    Returns (file): A file-like object to a Butler UTF-8 text stream supporting
        `write`.
    """
    fobj = None
    try:
      fobj = self.open_text(name, **kwargs)
      yield fobj
    finally:
      if fobj is not None:
        fobj.close()

  def open_text(self, name, content_type=None, tags=None):
    """Returns (file): A file-like object for a single text stream.

    This creates a new butler TEXT stream with the specified parameters.

    Args:
      name (str): the LogDog name of the stream.
      content_type (str): The optional content type of the stream. If None, a
          default content type will be chosen by the Butler.
      tags (dict): An optional key/value dictionary pair of LogDog stream tags.

    Returns (file): A file-like object to a Butler text stream. This object can
        have UTF-8 text content written to it with its `write` method, and must
        be closed when finished using its `close` method.
    """
    params = StreamParams.make(
        name=posixpath.join(self._namespace, name),
        type=StreamParams.TEXT,
        content_type=content_type,
        tags=tags)
    return self._TextStream(self, params, self.new_connection(params))

  @contextlib.contextmanager
  def binary(self, name, **kwargs):
    """Context manager to create, use, and teardown a BINARY stream.

    This context manager creates a new butler BINARY stream with the specified
    parameters, yields it, and closes it on teardown.

    Args:
      name (str): the LogDog name of the stream.
      kwargs (dict): Log stream parameters. These may be any keyword arguments
          accepted by `open_binary`.

    Returns (file): A file-like object to a Butler binary stream supporting
        `write`.
    """
    fobj = None
    try:
      fobj = self.open_binary(name, **kwargs)
      yield fobj
    finally:
      if fobj is not None:
        fobj.close()

  def open_binary(self, name, content_type=None, tags=None):
    """Returns (file): A file-like object for a single binary stream.

    This creates a new butler BINARY stream with the specified parameters.

    Args:
      name (str): the LogDog name of the stream.
      content_type (str): The optional content type of the stream. If None, a
          default content type will be chosen by the Butler.
      tags (dict): An optional key/value dictionary pair of LogDog stream tags.

    Returns (file): A file-like object to a Butler binary stream. This object
        can have UTF-8 content written to it with its `write` method, and must
        be closed when finished using its `close` method.
    """
    params = StreamParams.make(
        name=posixpath.join(self._namespace, name),
        type=StreamParams.BINARY,
        content_type=content_type,
        tags=tags)
    return self._BasicStream(self, params, self.new_connection(params))

  @contextlib.contextmanager
  def datagram(self, name, **kwargs):
    """Context manager to create, use, and teardown a DATAGRAM stream.

    This context manager creates a new butler DATAAGRAM stream with the
    specified parameters, yields it, and closes it on teardown.

    Args:
      name (str): the LogDog name of the stream.
      kwargs (dict): Log stream parameters. These may be any keyword arguments
          accepted by `open_datagram`.

    Returns (_DatagramStream): A datagram stream object. Datagrams can be
        written to it using its `send` method.
    """
    fobj = None
    try:
      fobj = self.open_datagram(name, **kwargs)
      yield fobj
    finally:
      if fobj is not None:
        fobj.close()

  def open_datagram(self, name, content_type=None, tags=None):
    """Creates a new butler DATAGRAM stream with the specified parameters.

    Args:
      name (str): the LogDog name of the stream.
      content_type (str): The optional content type of the stream. If None, a
          default content type will be chosen by the Butler.
      tags (dict): An optional key/value dictionary pair of LogDog stream tags.

    Returns (_DatagramStream): A datagram stream object. Datagrams can be
        written to it using its `send` method. This object must be closed when
        finished by using its `close` method.
    """
    params = StreamParams.make(
        name=posixpath.join(self._namespace, name),
        type=StreamParams.DATAGRAM,
        content_type=content_type,
        tags=tags)
    return self._DatagramStream(self, params, self.new_connection(params))


class _NamedPipeStreamClient(StreamClient):
  """A StreamClient implementation that connects to a Windows named pipe.
  """

  def __init__(self, name, **kwargs):
    r"""Initializes a new Windows named pipe stream client.

    Args:
      name (str): The name of the Windows named pipe to use (e.g., "\\.\name")
    """
    super(_NamedPipeStreamClient, self).__init__(**kwargs)
    self._name = '\\\\.\\pipe\\' + name

  @classmethod
  def _create(cls, value, **kwargs):
    return cls(value, **kwargs)

  ERROR_PIPE_BUSY = 231

  def _connect_raw(self):
    # This is a similar procedure to the one in
    #   https://github.com/microsoft/go-winio/blob/master/pipe.go (tryDialPipe)
    while True:
      try:
        return open(self._name, 'wb+', buffering=0)
      except (OSError, IOError):
        if GetLastError() != self.ERROR_PIPE_BUSY:
          raise
      time.sleep(0.001)  # 1ms


_default_registry.register_protocol('net.pipe', _NamedPipeStreamClient)


class _UnixDomainSocketStreamClient(StreamClient):
  """A StreamClient implementation that uses a UNIX domain socket.
  """

  class SocketFile(object):
    """A write-only file-like object that writes to a UNIX socket."""

    def __init__(self, sock):
      self._sock = sock

    def fileno(self):
      return self._sock

    def write(self, data):
      self._sock.sendall(data)

    def flush(self):
      pass

    def close(self):
      self._sock.close()

  def __init__(self, path, **kwargs):
    """Initializes a new UNIX domain socket stream client.

    Args:
      path (str): The path to the named UNIX domain socket.
    """
    super(_UnixDomainSocketStreamClient, self).__init__(**kwargs)
    self._path = path

  @classmethod
  def _create(cls, value, **kwargs):
    if not os.path.exists(value):
      raise ValueError('UNIX domain socket [%s] does not exist.' % (value,))
    return cls(value, **kwargs)

  def _connect_raw(self):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(self._path)
    return self.SocketFile(sock)

_default_registry.register_protocol('unix', _UnixDomainSocketStreamClient)
