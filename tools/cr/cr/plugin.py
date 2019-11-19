# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The plugin management system for the cr tool.

This holds the Plugin class and supporting code, that controls how plugins are
found and used.
The module registers a scan hook with the cr.loader system to enable it to
discover plugins as they are loaded.
"""

from __future__ import print_function

from operator import attrgetter

import cr
import cr.loader


def _PluginConfig(name, only_enabled=False, only_active=False):
  config = cr.Config(name)
  config.only_active = only_active
  config.only_enabled = only_enabled or config.only_active
  config.property_name = name.lower() + '_config'
  return config

_selectors = cr.Config('PRIORITY')
CONFIG_TYPES = [
    # Lowest priority, always there default values.
    _PluginConfig('DEFAULT').AddChild(_selectors),
    # Only turned on if the plugin is enabled.
    _PluginConfig('ENABLED', only_enabled=True),
    # Only turned on while the plugin is the active one.
    _PluginConfig('ACTIVE', only_active=True),
    # Holds detected values for active plugins.
    _PluginConfig('DETECTED', only_active=True),
    # Holds overrides, used in custom setup plugins.
    _PluginConfig('OVERRIDES'),
]

cr.config.GLOBALS.extend(CONFIG_TYPES)
_plugins = {}


# Actually a decorator, so pylint: disable=invalid-name
class classproperty(object):
  """This adds a property to a class.

  This is like a simple form of @property except it is for the class, rather
  than instances of the class. Only supports readonly properties.
  """

  def __init__(self, getter):
    self.getter = getter

  def __get__(self, instance, owner):
    return self.getter(owner)


class DynamicChoices(object):
  """Manages the list of active plugins for command line options.

  Looks like a simple iterable, but it can change as the underlying plugins
  arrive and enable/disable themselves. This allows it to be used as the
  set of valid choices for the argparse command line options.
  """

  # If this is True, all DynamicChoices only return active plugins.
  # If false, all plugins are included.
  only_active = True

  def __init__(self, cls):
    self.cls = cls

  def __contains__(self, name):
    return self.cls.FindPlugin(name, self.only_active) is not None

  def __iter__(self):
    return [p.name for p in self.cls.Plugins()].__iter__()


def _FindRoot(cls):
  if Plugin.Type in cls.__bases__:
    return cls
  for base in cls.__bases__:
    result = _FindRoot(base)
    if result is not None:
      return result
  return None


class Plugin(cr.loader.AutoExport):
  """Base class for managing registered plugin types."""

  class Type(object):
    """Base class that tags a class as an abstract plugin type."""

  class activemethod(object):
    """A decorator that delegates a static method to the active plugin.

    Makes a static method that delegates to the equivalent method on the
    active instance of the plugin type.
    """

    def __init__(self, method):
      self.method = method

    def __get__(self, instance, owner):
      def unbound(*args, **kwargs):
        active = owner.GetActivePlugin()
        if not active:
          print('No active', owner.__name__)
          exit(1)
        method = getattr(active, self.method.__name__, None)
        if not method:
          print(owner.__name__, 'does not support', self.method.__name__)
          exit(1)
        return method(*args, **kwargs)

      def bound(*args, **kwargs):
        return self.method(instance, *args, **kwargs)

      if instance is None:
        return unbound
      return bound

  def __init__(self):
    # Default the name to the lowercased class name.
    self._name = self.__class__.__name__.lower()
    # Strip the common suffix if present.
    self._root = _FindRoot(self.__class__)
    rootname = self._root.__name__.lower()
    if self._name.endswith(rootname) and self.__class__ != self._root:
      self._name = self._name[:-len(rootname)]
    for config_root in CONFIG_TYPES:
      config = cr.Config()
      setattr(self, config_root.property_name, config)
    self._is_active = False

  def Init(self):
    """Post plugin registration initialisation method."""
    for config_root in CONFIG_TYPES:
      config = getattr(self, config_root.property_name)
      config.name = self.name
      if config_root.only_active and not self.is_active:
        config.enabled = False
      if config_root.only_enabled and not self.enabled:
        config.enabled = False
      child = getattr(self.__class__, config_root.name, None)
      if child is not None:
        child.name = self.__class__.__name__
        config.AddChild(child)
      config_root.AddChild(config)

  @property
  def name(self):
    return self._name

  @property
  def priority(self):
    return 0

  @property
  def enabled(self):
    # By default all non type classes are enabled.
    return Plugin.Type not in self.__class__.__bases__

  @property
  def is_active(self):
    return self._is_active

  def Activate(self):
    assert not self._is_active
    self._is_active = True
    for config_root in CONFIG_TYPES:
      if config_root.only_active:
        getattr(self, config_root.property_name).enabled = True

  def Deactivate(self):
    assert self._is_active
    self._is_active = False
    for config_root in CONFIG_TYPES:
      if config_root.only_active:
        getattr(self, config_root.property_name).enabled = False

  @classmethod
  def ClassInit(cls):
    pass

  @classmethod
  def GetInstance(cls):
    """Gets an instance of this plugin.

    This looks in the plugin registry, and if an instance is not found a new
    one is built and registered.

    Returns:
        The registered plugin instance.
    """
    plugin = _plugins.get(cls, None)
    if plugin is None:
      # Run delayed class initialization
      cls.ClassInit()
      # Build a new instance of cls, and register it as the main instance.
      plugin = cls()
      _plugins[cls] = plugin
      # Wire up the hierarchy for Config objects.
      for name, value in cls.__dict__.items():
        if isinstance(value, cr.Config):
          for base in cls.__bases__:
            child = getattr(base, name, None)
            if child is not None:
              value.AddChild(child)
      plugin.Init()
    return plugin

  @classmethod
  def AllPlugins(cls):
    # Don't yield abstract roots, just children. We detect roots as direct
    # sub classes of Plugin.Type
    if Plugin.Type not in cls.__bases__:
      yield cls.GetInstance()
    for child in cls.__subclasses__():
      for p in child.AllPlugins():
        yield p

  @classmethod
  def UnorderedPlugins(cls):
    """Returns all enabled plugins of type cls, in undefined order."""
    plugin = cls.GetInstance()
    if plugin.enabled:
      yield plugin
    for child in cls.__subclasses__():
      for p in child.UnorderedPlugins():
        yield p

  @classmethod
  def Plugins(cls):
    """Return all enabled plugins of type cls in priority order."""
    return sorted(cls.UnorderedPlugins(),
                  key=attrgetter('priority'), reverse=True)

  @classmethod
  def Choices(cls):
    return DynamicChoices(cls)

  @classmethod
  def FindPlugin(cls, name, only_active=True):
    if only_active:
      plugins = cls.UnorderedPlugins()
    else:
      plugins = cls.AllPlugins()
    for plugin in plugins:
      if plugin.name == name or plugin.__class__.__name__ == name:
        return plugin
    return None

  @classmethod
  def GetPlugin(cls, name):
    result = cls.FindPlugin(name)
    if result is None:
      raise KeyError(name)
    return result

  @classmethod
  def GetAllActive(cls):
    return [plugin for plugin in cls.UnorderedPlugins() if plugin.is_active]

  @classmethod
  def GetActivePlugin(cls):
    """Gets the active plugin of type cls.

    This method will select a plugin to be the active one, and will activate
    the plugin if needed.
    Returns:
      the plugin that is currently active.
    """
    plugin, _ = _GetActivePlugin(cls)
    return plugin

  @classproperty
  def default(cls):
    """Returns the plugin that should be used if the user did not choose one."""
    result = None
    for plugin in cls.UnorderedPlugins():
      if not result or plugin.priority > result.priority:
        result = plugin
    return result

  @classmethod
  def Select(cls):
    """Called to determine which plugin should be the active one."""
    plugin = cls.default
    selector = getattr(cls, 'SELECTOR', None)
    if selector:
      if plugin is not None:
        _selectors[selector] = plugin.name
      name = cr.context.Find(selector)
      if name is not None:
        plugin = cls.FindPlugin(name)
    return plugin


def ChainModuleConfigs(module):
  """Detects and connects the default Config objects from a module."""
  for config_root in CONFIG_TYPES:
    if hasattr(module, config_root.name):
      config = getattr(module, config_root.name)
      config.name = module.__name__
      config_root.AddChild(config)


cr.loader.scan_hooks.append(ChainModuleConfigs)


def _GetActivePlugin(cls):
  activated = False
  actives = cls.GetAllActive()
  plugin = cls.Select()
  for active in actives:
    if active != plugin:
      active.Deactivate()
  if plugin and not plugin.is_active:
    activated = True
    plugin.Activate()
  return plugin, activated


def Activate():
  """Activates a plugin for all known plugin types."""
  types = Plugin.Type.__subclasses__()
  modified = True
  while modified:
    modified = False
    for child in types:
      _, activated = _GetActivePlugin(child)
      if activated:
        modified = True
