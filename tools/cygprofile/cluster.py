# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clustering for function call-graph.

See the Clustering class for a detailed description.
"""

import collections
import itertools
import logging

Neighbor = collections.namedtuple('Neighbor', ('src', 'dst', 'dist'))
CalleeInfo = collections.namedtuple('CalleeInfo',
                                    ('index', 'callee_symbol',
                                     'misses', 'caller_and_count'))
CallerInfo = collections.namedtuple('CallerInfo', ('caller_symbol', 'count'))


class Clustering(object):
  """Cluster symbols.

  We are given a list of the first function calls, ordered by
  time. There are multiple lists: different benchmarks run multiple
  times, as well as list from startup and then a second list after
  startup (5 seconds) that runs until the benchmark memory dump.

  We have evidence (see below) that this simple ordering of code from a
  single profiling run (a load of a website) improves performance,
  presumably by improving code locality. To reconstruct this ordering
  using profiling information from multiple files, we cluster. Doing
  this clustering over multiple runs on the speedometer benchmark
  recovered speedometer performance compared with the legacy benchmark.

  For each offset list, we record the distances between each symbol and
  its neighborhood of the following k symbols (k=19, chosen
  arbitrarily). For example, if we have an offset list of symbols
  'abcdef', we add the neighbors (a->b, 1), (a->c, 2), (b->c, 1), (b->e,
  3), etc. Then we average distances of a given neighbor pair over all
  seen symbol lists. If we see an inversion (for example, (b->a, 3), we
  use this as a distance of -3). For each file that a given pair does
  not appear, that is, if the pair does not appear in that file or they
  are separated by 20 symbols, we use a large distance D (D=1000). The
  distances are then averages over all files. If the average is
  negative, the neighbor pair is inverted and the distance flipped. The
  idea is that if two symbols appear near each other in all profiling
  runs, there is high confidence that they are usually called
  together. If they don't appear near in some runs, there is less
  confidence that they should be colocated. Symbol distances are taken
  only as following distances to avoid confusing double-counting
  possibilities as well as to give a clear ordering to combining
  clusters.

  Neighbors are sorted, and starting with the shortest distance, symbols
  are coalesced into clusters. If the neighbor pair is (a->b), the
  clusters containing a and b are combined in that order. If a and b are
  already in the same cluster, nothing happens. After processing all
  neighbors there is usually only one cluster; if there are multiple
  clusters they are combined in order from largest to smallest (although
  that choice may not matter).

  Cluster merging may optionally be halted if they get above the size
  of an android page. As of November 2018 this slightly reduces
  performance and should not be used (1.7% decline in speedometer2,
  450K native library memory regression).
  """
  NEIGHBOR_DISTANCE = 20
  FAR_DISTANCE = 1000
  MAX_CLUSTER_SIZE = 4096  # 4k pages on android.

  class _Cluster(object):
    def __init__(self, syms, size):
      assert len(set(syms)) == len(syms), 'Duplicated symbols in cluster'
      self._syms = syms
      self._size = size

    @property
    def syms(self):
      return self._syms

    @property
    def binary_size(self):
      return self._size

  @classmethod
  def ClusteredSymbolLists(cls, sym_lists, size_map):
    c = cls()
    c.AddSymbolLists(sym_lists)
    return c.ClusterToList(size_map)

  @classmethod
  def ClusterSymbolCallGraph(cls, call_graph, whitelist):
    c = cls()
    c.AddSymbolCallGraph(call_graph, whitelist)
    return c.ClusterToList()

  def __init__(self):
    self._num_lists = None
    self._neighbors = None
    self._cluster_map = {}
    self._symbol_size = lambda _: 0  # Maps a symbol to a size.

  def _MakeCluster(self, syms):
    c = self._Cluster(syms, sum(self._symbol_size(s) for s in syms))
    for s in syms:
      self._cluster_map[s] = c
    return c

  def ClusterOf(self, s):
    if isinstance(s, self._Cluster):
      assert self._cluster_map[s.syms[0]] == s
      return s
    if s in self._cluster_map:
      return self._cluster_map[s]
    return self._MakeCluster([s])

  def Combine(self, a, b):
    """Combine clusters.

    Args:
      a, b: Clusters or str. The canonical cluster (ClusterOf) will be
        used to do the combining.

    Returns:
      A merged cluster from a and b, or None if a and b are in the same cluster.
    """
    canonical_a = self.ClusterOf(a)
    canonical_b = self.ClusterOf(b)
    if canonical_a == canonical_b:
      return None
    return self._MakeCluster(canonical_a._syms + canonical_b._syms)

  def AddSymbolLists(self, sym_lists):
    self._num_lists = len(sym_lists)
    self._neighbors = self._CoalesceNeighbors(
        self._ConstructNeighbors(sym_lists))

  def AddSymbolCallGraph(self, call_graph, whitelist):
    self._num_lists = len(call_graph)
    self._neighbors = self._ConstructNeighborsFromGraph(call_graph, whitelist)

  def _ConstructNeighborsFromGraph(self, call_graph, whitelist):
    neighbors = []
    pairs = collections.defaultdict()
    # Each list item is a list of dict.
    for process_items in call_graph:
      for callee_info in process_items:
        callee = callee_info.callee_symbol
        for caller_info in callee_info.caller_and_count:
          caller = caller_info.caller_symbol
          if caller in whitelist or callee == caller:
            continue

          # Multiply by -1, the bigger the count the smaller the distance
          # should be.
          dist = caller_info.count * -1
          if (caller, callee) in pairs:
            pairs[(caller, callee)] += dist
          elif (callee, caller) in pairs:
            pairs[(callee, caller)] += dist
          else:
            pairs[(caller, callee)] = dist

    for (s, t) in pairs:
      assert s != t and (t, s) not in pairs, ('Unexpected shuffled pair:'
                                              ' ({}, {})'.format(s, t))
      neighbors.append(Neighbor(s, t, pairs[(s, t)]))

    return neighbors

  def _ConstructNeighbors(self, sym_lists):
    neighbors = []
    for sym_list in sym_lists:
      for i, s in enumerate(sym_list):
        for j in xrange(i + 1, min(i + self.NEIGHBOR_DISTANCE, len(sym_list))):
          if s == sym_list[j]:
            # Free functions that are static inline seem to be the only
            # source of these duplicates.
            continue
          neighbors.append(Neighbor(s, sym_list[j], j - i))
    logging.info('Constructed %s symbol neighbors', len(neighbors))
    return neighbors

  def _CoalesceNeighbors(self, neighbors):
    pairs = collections.defaultdict(list)
    for n in neighbors:
      pairs[(n.src, n.dst)].append(n.dist)
    coalesced = []
    logging.info('Will coalesce over %s neighbor pairs', len(pairs))
    count = 0
    for (s, t) in pairs:
      assert s != t, '{} != {}'.format(s, t)
      if (t, s) in pairs and t < s:
        # Only process each unordered pair once.
        continue
      count += 1
      if not (count % 1e6):
        logging.info('tick')
      distances = []
      if (s, t) in pairs:
        distances.extend(pairs[(s, t)])
      if (t, s) in pairs:
        distances.extend(-d for d in pairs[(t, s)])
      if distances:
        num_missing = self._num_lists - len(distances)
        avg_distance = (float(sum(distances)) +
                        self.FAR_DISTANCE * num_missing) / self._num_lists
        if avg_distance > 0:
          coalesced.append(Neighbor(s, t, avg_distance))
        else:
          coalesced.append(Neighbor(t, s, avg_distance))
    return coalesced

  def ClusterToList(self, size_map=None):
    """Merge the clusters with the smallest distances.

    Args:
      size_map ({symbol: size} or None): Map symbol names to their size. Cluster
        growth will be stopped at MAX_CLUSTER_SIZE. If None, sizes are taken to
        be zero and cluster growth is not stopped.

    Returns:
      An ordered list of symbols from AddSymbolLists, appropriately clustered.
    """
    if size_map:
      self._symbol_size = lambda s: size_map[s]
    if not self._num_lists or not self._neighbors:
      # Some sort of trivial set of symbol lists, such as all being
      # length 1. Return an empty ordering.
      return []
    logging.info('Sorting %s neighbors', len(self._neighbors))
    self._neighbors.sort(key=lambda n: (-n.dist, n.src, n.dst))
    logging.info('Clustering...')
    count = 0
    while self._neighbors:
      count += 1
      if not (count % 1e6):
        logging.info('tock')
      neighbor = self._neighbors.pop()
      src = self.ClusterOf(neighbor.src)
      dst = self.ClusterOf(neighbor.dst)
      if (src == dst or
          src.binary_size + dst.binary_size > self.MAX_CLUSTER_SIZE):
        continue
      self.Combine(src, dst)
    if size_map:
      clusters_by_size = sorted(list(set(self._cluster_map.values())),
                                key=lambda c: -c.binary_size)
    else:
      clusters_by_size = sorted(list(set(self._cluster_map.values())),
                                key=lambda c: -len(c.syms))
    logging.info('Produced %s clusters', len(clusters_by_size))
    logging.info('Top sizes: %s', ['{}/{}'.format(len(c.syms), c.binary_size)
                                   for c in clusters_by_size[:4]])
    logging.info('Bottom sizes: %s', ['{}/{}'.format(len(c.syms), c.binary_size)
                                      for c in clusters_by_size[-4:]])
    ordered_syms = []
    for c in clusters_by_size:
      ordered_syms.extend(c.syms)
    assert len(ordered_syms) == len(set(ordered_syms)), 'Duplicated symbols!'
    return ordered_syms

def _GetOffsetSymbolName(processor, dump_offset):
  dump_offset_to_symbol_info = \
      processor.GetDumpOffsetToSymboInfolIncludingWhitelist()
  offset_to_primary = processor.OffsetToPrimaryMap()
  idx = dump_offset / 2
  assert dump_offset >= 0 and idx < len(dump_offset_to_symbol_info), (
      'Dump offset out of binary range')
  symbol_info = dump_offset_to_symbol_info[idx]
  assert symbol_info, ('A return address (offset = 0x{:08x}) does not map '
                       'to any symbol'.format(dump_offset))
  assert symbol_info.offset in offset_to_primary, (
      'Offset not found in primary map!')
  return offset_to_primary[symbol_info.offset].name

def _GetSymbolsCallGraph(profiles, processor):
  """Maps each offset in the call graph to the corresponding symbol name.

  Args:
    profiles (ProfileManager) Manager of the profile dump files.
    processor (SymbolOffsetProcessor) Symbol table processor for the dumps.

  Returns:
    A dict that maps each process type (ex: browser, renderer, etc.) to a list
    of processes of that type. Each process is a list that contains the
    call graph information. The call graph is represented by a list where each
    item is a dict that contains: callee, 3 caller-count pairs, misses.
  """
  offsets_graph = profiles.GetProcessOffsetGraph();
  process_symbols_graph = collections.defaultdict(list)

  # |process_type| can be : browser, renderer, gpu-process, etc.
  for process_type in offsets_graph:
    for process in offsets_graph[process_type]:
      process = sorted(process, key=lambda k: long(k['index']))
      graph_list = []
      for el in process:
        index = long(el['index'])
        callee_symbol = _GetOffsetSymbolName(processor,
                                             long(el['callee_offset']))
        misses = 0
        caller_and_count = []
        for bucket in el['caller_and_count']:
          caller_offset = long(bucket['caller_offset'])
          count = long(bucket['count'])
          if caller_offset == 0:
            misses += count
            continue

          caller_symbol_name = _GetOffsetSymbolName(processor, caller_offset)
          caller_info = CallerInfo(caller_symbol=caller_symbol_name,
                                   count=count)
          caller_and_count.append(caller_info)

        callee_info = CalleeInfo(index=index,
                                 callee_symbol=callee_symbol,
                                 misses=misses,
                                 caller_and_count=caller_and_count)
        graph_list.append(callee_info)
      process_symbols_graph[process_type].append(graph_list)
  return process_symbols_graph

def _ClusterOffsetsFromCallGraph(profiles, processor):
  symbols_call_graph = _GetSymbolsCallGraph(profiles, processor)
  # Process names from the profile dumps that are treated specially.
  _RENDERER = 'renderer'
  _BROWSER = 'browser'

  assert _RENDERER in symbols_call_graph
  assert _BROWSER in symbols_call_graph
  whitelist = processor.GetWhitelistSymbols()
  renderer_clustering = Clustering.ClusterSymbolCallGraph(
      symbols_call_graph[_RENDERER], whitelist)
  browser_clustering = Clustering.ClusterSymbolCallGraph(
      symbols_call_graph[_BROWSER], whitelist)
  other_lists = []
  for process in symbols_call_graph:
    if process not in (_RENDERER, _BROWSER):
      other_lists.extend(symbols_call_graph[process])
  if other_lists:
    other_clustering = Clustering.ClusterSymbolCallGraph(other_lists, whitelist)
  else:
    other_clustering = []

  # Start with the renderer cluster to favor rendering performance.
  final_ordering = [s for s in renderer_clustering]
  seen = set(final_ordering)
  final_ordering.extend(s for s in browser_clustering if s not in seen)
  seen |= set(browser_clustering)
  final_ordering.extend(s for s in other_clustering if s not in seen)

  return final_ordering

def _ClusterOffsetsLists(profiles, processor, limit_cluster_size=False):
  raw_offsets = profiles.GetProcessOffsetLists()
  process_symbols = collections.defaultdict(list)
  seen_symbols = set()
  for p in raw_offsets:
    for offsets in raw_offsets[p]:
      symbol_names = processor.GetOrderedSymbols(
          processor.GetReachedOffsetsFromDump(offsets))
      process_symbols[p].append(symbol_names)
      seen_symbols |= set(symbol_names)
  if limit_cluster_size:
    name_map = processor.NameToSymbolMap()
    size_map = {name: name_map[name].size for name in seen_symbols}
  else:
    size_map = None

  # Process names from the profile dumps that are treated specially.
  _RENDERER = 'renderer'
  _BROWSER = 'browser'

  assert _RENDERER in process_symbols
  assert _BROWSER in process_symbols

  renderer_clustering = Clustering.ClusteredSymbolLists(
      process_symbols[_RENDERER], size_map)
  browser_clustering = Clustering.ClusteredSymbolLists(
      process_symbols[_BROWSER], size_map)
  other_lists = []
  for process, syms in process_symbols.items():
    if process not in (_RENDERER, _BROWSER):
      other_lists.extend(syms)
  if other_lists:
    other_clustering = Clustering.ClusteredSymbolLists(other_lists, size_map)
  else:
    other_clustering = []

  # Start with the renderer cluster to favor rendering performance.
  final_ordering = [s for s in renderer_clustering]
  seen = set(final_ordering)
  final_ordering.extend(s for s in browser_clustering if s not in seen)
  seen |= set(browser_clustering)
  final_ordering.extend(s for s in other_clustering if s not in seen)

  return final_ordering

def ClusterOffsets(profiles, processor, limit_cluster_size=False,
                   call_graph=False):
  """Cluster profile offsets.

  Args:
    profiles (ProfileManager) Manager of the profile dump files.
    processor (SymbolOffsetProcessor) Symbol table processor for the dumps.
    call_graph (bool) whether the call graph instrumentation was used.

  Returns:
    A list of clustered symbol offsets.
"""
  if not call_graph:
    return _ClusterOffsetsLists(profiles, processor, limit_cluster_size)
  else:
    return _ClusterOffsetsFromCallGraph(profiles, processor)
