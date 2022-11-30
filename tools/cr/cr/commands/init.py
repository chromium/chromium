# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the init command."""

from __future__ import print_function

import os

import cr

# The set of variables to store in the per output configuration.
OUT_CONFIG_VARS = [
    'CR_VERSION',
    cr.Platform.SELECTOR,
    cr.BuildType.SELECTOR,
    cr.Arch.SELECTOR,
    cr.PrepareOut.SELECTOR,
    'CR_OUT_BASE',
    'CR_OUT_FULL',
]


class InitCommand(cr.Command):
  """The implementation of the init command.

  The init command builds or updates an output directory.
  It then uses the Prepare and Select commands to get that directory
  ready to use.
  """

  def __init__(self):
    super(InitCommand, self).__init__()
    self.requires_build_dir = False
    self.help = 'Create and configure an output directory'
    self.description = ("""
        If the .cr directory is not present, build it and add
        the specified configuration.
        If the file already exists, update the configuration with any
        additional settings.
        """)
    self._settings = []

  def AddArguments(self, subparsers):
    """Overridden from cr.Command."""
    parser = super(InitCommand, self).AddArguments(subparsers)
    cr.Platform.AddArguments(parser)
    cr.BuildType.AddArguments(parser)
    cr.Arch.AddArguments(parser)
    cr.SelectCommand.AddPrepareArguments(parser)
    cr.PrepareOut.AddArguments(parser)
    parser.add_argument(
        '-s', '--set', dest='_settings', metavar='settings',
        action='append',
        help='Configuration overrides.'
    )
    return parser

  def EarlyArgProcessing(self):
    base_settings = getattr(cr.context.args, '_settings', None)
    if base_settings:
      self._settings.extend(base_settings)
    # Do not call super early processing, we do not want to apply
    # the output arg...
    out = cr.base.client.GetOutArgument()
    if out:
      # Output directory is fully specified
      # We need to deduce other settings from it's name
      base, buildtype = os.path.split(out)
      if not (base and buildtype):
        print('Specified output directory must be two levels')
        exit(1)
      platform = cr.context.args.CR_PLATFORM
      if not platform:
        # Try to guess platform based on output name
        platforms = [p.name for p in cr.Platform.AllPlugins()]
        matches = [p for p in platforms if p in base]
        # Get the longest matching string and check if the others are
        # substrings. This is done to support "linuxchromeos" and "linux".
        platform = max(matches, key=len)
        all_matches_are_substrings = all(p in platform for p in matches)
        if not all_matches_are_substrings or not matches:
          print('Platform is not set, and could not be guessed from', base)
          print('Should be one of', ','.join(platforms))
          if len(matches) > 1:
            print('Matched all of', ','.join(matches))
          exit(1)
      generator = cr.context.args.CR_GENERATOR
      if not generator:
        generator = 'gn'
      cr.context.derived.Set(
          CR_OUT_FULL=out,
          CR_OUT_BASE=base,
          CR_PLATFORM=platform,
          CR_GENERATOR=generator
      )
    if not 'CR_OUT_BASE' in cr.context:
      cr.context.derived['CR_OUT_BASE'] = 'out_{CR_PLATFORM}'
    if not 'CR_OUT_FULL' in cr.context:
      cr.context.derived['CR_OUT_FULL'] = os.path.join(
          '{CR_OUT_BASE}', '{CR_BUILDTYPE}')

  def Run(self):
    """Overridden from cr.Command."""
    src_path = cr.context.Get('CR_SRC')
    if not os.path.isdir(src_path):
      print(cr.context.Substitute('Path {CR_SRC} is not a valid client'))
      exit(1)

    # Ensure we have an output directory override ready to fill in
    # This will only be missing if we are creating a brand new output
    # directory
    build_package = cr.auto.build

    # Collect the old version (and float convert)
    old_version = cr.context.Find('CR_VERSION')
    try:
      old_version = float(old_version)
    except (ValueError, TypeError):
      old_version = 0.0
    is_new = not hasattr(build_package, 'config')
    if is_new:

      class FakeModule(object):
        OVERRIDES = cr.Config('OVERRIDES')

        def __init__(self):
          self.__name__ = 'config'

      old_version = None
      config = FakeModule()
      setattr(build_package, 'config', config)
      cr.plugin.ChainModuleConfigs(config)

    # Force override the version
    build_package.config.OVERRIDES.Set(CR_VERSION=cr.base.client.VERSION)
    # Add all the variables that we always want to have
    for name in OUT_CONFIG_VARS:
      value = cr.context.Find(name)
      build_package.config.OVERRIDES[name] = value
    # Apply the settings from the command line
    for setting in self._settings:
      name, separator, value = setting.partition('=')
      name = name.strip()
      if not separator:
        value = True
      else:
        value = cr.Config.ParseValue(value.strip())
      build_package.config.OVERRIDES[name] = value

    # Run all the output directory init hooks
    for hook in cr.InitHook.Plugins():
      hook.Run(old_version, build_package.config)
    # Redo activations, they might have changed
    cr.plugin.Activate()

    # Write out the new configuration, and select it as the default
    cr.base.client.WriteConfig(
        use_build_dir=True, data=build_package.config.OVERRIDES.exported)
    # Prepare the platform in here, using the updated config
    cr.Platform.Prepare()
    cr.SelectCommand.Select()
