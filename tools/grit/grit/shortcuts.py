# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Stuff to prevent conflicting shortcuts.
'''


from grit import lazy_re


class ShortcutGroup:
  '''Manages a list of cliques that belong together in a single shortcut
  group.  Knows how to detect conflicting shortcut keys.
  '''

  # Matches shortcut keys, e.g. &J
  SHORTCUT_RE = lazy_re.compile('([^&]|^)(&[A-Za-z])')

  def __init__(self, name):
    self.name = name
    # Map of language codes to shortcut keys used (which is a map of
    # shortcut keys to counts).
    self.keys_by_lang = {}
    # List of cliques in this group
    self.cliques = []

  def AddClique(self, c):
    for existing_clique in self.cliques:
      if existing_clique.GetId() == c.GetId():
        # This happens e.g. when we have e.g.
        # <if expr1><structure 1></if> <if expr2><structure 2></if>
        # where only one will really be included in the output.
        return

    self.cliques.append(c)
    for (lang, msg) in c.clique.items():
      if lang not in self.keys_by_lang:
        self.keys_by_lang[lang] = {}
      keymap = self.keys_by_lang[lang]

      content = msg.GetRealContent()
      keys = [groups[1] for groups in self.SHORTCUT_RE.findall(content)]
      for key in keys:
        key = key.upper()
        if key in keymap:
          keymap[key] += 1
        else:
          keymap[key] = 1

  def GenerateWarnings(self, tc_project):
    # For any language that has more than one occurrence of any shortcut,
    # make a list of the conflicting shortcuts.
    problem_langs = {}
    for (lang, keys) in self.keys_by_lang.items():
      for (key, count) in keys.items():
        if count > 1:
          if lang not in problem_langs:
            problem_langs[lang] = []
          problem_langs[lang].append(key)

    warnings = []
    if len(problem_langs):
      warnings.append("WARNING - duplicate keys exist in shortcut group %s" %
                      self.name)
      for (lang,keys) in problem_langs.items():
        warnings.append("  %6s duplicates: %s" % (lang, ', '.join(keys)))
    return warnings


def GenerateDuplicateShortcutsWarnings(uberclique, tc_project):
  '''Given an UberClique and a project name, will print out helpful warnings
  if there are conflicting shortcuts within shortcut groups in the provided
  UberClique.

  Args:
    uberclique: clique.UberClique()
    tc_project: 'MyProjectNameInTheTranslationConsole'

  Returns:
    ['warning line 1', 'warning line 2', ...]
  '''
  warnings = []
  groups = {}
  for c in uberclique.AllCliques():
    for group in c.shortcut_groups:
      if group not in groups:
        groups[group] = ShortcutGroup(group)
      groups[group].AddClique(c)
  for group in groups.values():
    warnings += group.GenerateWarnings(tc_project)
  return warnings
