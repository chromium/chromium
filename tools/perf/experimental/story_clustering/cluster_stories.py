# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import division

import heapq


class Cluster(object):
  def __init__(self, members):
    """Initializes the cluster instance.

    Args:
      members: Set of story names which belong to this cluster.
    """
    self._members = frozenset(members)
    self._representative = None

  def __len__(self):
    return len(self._members)

  def GetDistanceFrom(self, other_cluster, distance_matrix):
    """Calculates the distance of two clusters.

    The maximum distance between any story of first cluster to any story of
    the second cluster is used as the distance between clusters._members

    Args:
      other_cluster: Cluster object to calculate distance from.
      distance_matrix: A dataframe containing the distances between any
      two stories.

    Returns:
      A float number representing the distacne between two clusters.
    """
    matrix_slice = distance_matrix.loc[self.members, other_cluster.members]
    return matrix_slice.max().max()

  @property
  def members(self):
    return self._members

  def GetRepresentative(self, distance_matrix=None):
    """Finds and sets the representative of cluster.

    The story which its max distance to all other members is minimum is
    used as the representative.

    Args:
      distance_matrix: A dataframe containing the distances between any
      two stories.

    Returns:
      A story which is the representative of cluster
    """
    if self._representative:
      return self._representative

    if distance_matrix is None:
      raise Exception('Distance matrix is not set.')

    self._representative = distance_matrix.loc[
      self._members, self._members].sum().idxmin()
    return self._representative

  def Merge(self, other_cluster):
    """Merges two clusters.

    Returns:
      A new cluster object which is a result of merging two clusters.
    """
    return Cluster(self.members | other_cluster.members)

  def AsDict(self):
    """Creates a dictionary which describes cluster object.

    Returns:
      A dictionary containing the members of the cluster and its
      representative. The representative will not be listed in members
      list.
    """
    representative = self.GetRepresentative()
    members_list = list(self.members.difference([representative]))
    return {
      'members': members_list,
      'representative': self.GetRepresentative()
    }


def RunHierarchicalClustering(
  distance_matrix,
  max_cluster_count,
  min_cluster_size):
  """Clusters stories.

  Runs a hierarchical clustering algorithm based on the similarity measures.

  Args:
    distance_matrix: A dataframe containing distance matrix of stories.
    max_cluster_count: number representing the maximum number of clusters
      needed per metric.
    min_cluster_size: number representing the least number of members needed
      to make the cluster valid.

  Returns:
    A tuple containing:
    clusters: A list of cluster objects
    coverage: Ratio(float) of stories covered using this clustering
  """
  stories = distance_matrix.index.values
  remaining_clusters = set([])
  for story in stories:
    remaining_clusters.add(Cluster([story]))

  # The hierarchical clustering relies on a sorted list of possible
  # cluster merges ordered by the distance between them.
  heap = []

  # Initially each story is a cluster on it's own. And story pairs are
  # added all possible merges.
  for cluster1 in remaining_clusters:
    for cluster2 in remaining_clusters:
      if cluster1 == cluster2:
        break
      heapq.heappush(heap,
        (cluster1.GetDistanceFrom(cluster2, distance_matrix),
         cluster1, cluster2))

  # At each step the two clusters will be merged together.
  while (len(remaining_clusters) > max_cluster_count and len(heap) > 0):
    _, cluster1, cluster2 = heapq.heappop(heap)
    if (cluster1 not in remaining_clusters or
      cluster2 not in remaining_clusters):
      continue
    new_cluster = cluster1.Merge(cluster2)
    remaining_clusters.discard(cluster1)
    remaining_clusters.discard(cluster2)

    # Adding all possible merges to the heap
    for cluster in remaining_clusters:
      distance = new_cluster.GetDistanceFrom(cluster, distance_matrix)
      heapq.heappush(heap, (distance, new_cluster, cluster))

    remaining_clusters.add(new_cluster)

  final_clusters = []
  number_of_stories_covered = 0
  for cluster in remaining_clusters:
    cluster.GetRepresentative(distance_matrix)
    if len(cluster) >= min_cluster_size:
      final_clusters.append(cluster)
      number_of_stories_covered += len(cluster)
  coverage = number_of_stories_covered / len(stories)

  return final_clusters, coverage
