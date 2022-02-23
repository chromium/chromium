# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import collections
import os

from . import stream, streamname


class NotBootstrappedError(RuntimeError):
  """Raised when the current environment is missing Butler bootstrap variables.
  """


_ButlerBootstrapBase = collections.namedtuple('_ButlerBootstrapBase',
    ('project', 'prefix', 'streamserver_uri', 'coordinator_host',
     'namespace'))


class ButlerBootstrap(_ButlerBootstrapBase):
  """Loads LogDog Butler bootstrap parameters from the environment.

  LogDog Butler adds variables describing the LogDog stream parameters to the
  environment when it bootstraps an application. This class probes the
  environment and identifies those parameters.
  """

  # TODO(iannucci): move all of these to LUCI_CONTEXT
  _ENV_PROJECT = 'LOGDOG_STREAM_PROJECT'
  _ENV_PREFIX = 'LOGDOG_STREAM_PREFIX'
  _ENV_STREAM_SERVER_PATH = 'LOGDOG_STREAM_SERVER_PATH'
  _ENV_COORDINATOR_HOST = 'LOGDOG_COORDINATOR_HOST'
  _ENV_NAMESPACE = 'LOGDOG_NAMESPACE'

  @classmethod
  def probe(cls, env=None):
    """Returns (ButlerBootstrap): The probed bootstrap environment.

    Args:
      env (dict): The environment to probe. If None, `os.getenv` will be used.

    Raises:
      NotBootstrappedError if the current environment is not bootstrapped.
    """
    if env is None:
      env = os.environ

    def _check(kind, val):
      if not val:
        return val
      try:
        streamname.validate_stream_name(val)
        return val
      except ValueError as exp:
        raise NotBootstrappedError('%s (%s) is invalid: %s' % (kind, val, exp))

    streamserver_uri = env.get(cls._ENV_STREAM_SERVER_PATH)
    if not streamserver_uri:
      raise NotBootstrappedError('No streamserver in bootstrap environment.')

    return cls(
        project=env.get(cls._ENV_PROJECT, ''),
        prefix=_check("Prefix", env.get(cls._ENV_PREFIX, '')),
        streamserver_uri=streamserver_uri,
        coordinator_host=env.get(cls._ENV_COORDINATOR_HOST, ''),
        namespace=_check("Namespace", env.get(cls._ENV_NAMESPACE, '')))

  def stream_client(self, reg=None):
    """Returns: (StreamClient) stream client for the bootstrap streamserver URI.

    If the Butler accepts external stream connections, it will export a
    streamserver URI in the environment. This will create a StreamClient
    instance to operate on the streamserver if one is defined.

    Args:
      reg (stream.StreamProtocolRegistry or None): The stream protocol registry
          to use to create the stream. If None, the default global registry will
          be used (recommended).

    Raises:
      ValueError: If no streamserver URI is present in the environment.
    """
    reg = reg or stream._default_registry
    return reg.create(
        self.streamserver_uri,
        project=self.project,
        prefix=self.prefix,
        coordinator_host=self.coordinator_host,
        namespace=self.namespace)
