# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collects pervasive resources from HTTP Archive and generates a pattern list.

This script queries the HTTP Archive dataset in BigQuery to find resources
that are "pervasive" (high occurrence) and "static" (unchanged) across
multiple months. It then generates a set of URL patterns to match these
resources and writes them to a C++ header file.
"""

import calendar
import datetime
import difflib
import json
import logging
import os
import re
import string
from typing import Any, Dict, List, Optional
from urllib.parse import urlparse

from dateutil.relativedelta import relativedelta
from google.cloud import bigquery
import zstandard as zstd

# Minimum number of occurrences per month to be considered "pervasive"
_PERVASIVE_COUNT = 100000

# Longest URL to include
_MAX_URL_LENGTH = 200

# File sizes to consider as "similar" across URLs (percentage)
_SIZE_MATCH_PERCENT = 5

# Number of Months to evaluate
_MONTHS = 6

# Minimum number of path components that must match
_MIN_STABLE_PATH = 2

# difflib.ratio() similarity in filenames when looking for matching candidates
_MIN_FILENAME_RATIO = 0.5

# difflib.find_matching_blocks() maximum number of matching blocks to consider
# filenames "similar" (including the ending dummy block)
_MAX_FILENAME_MATCHING_BLOCKS = 3

# Don't allow for filenames to be similar if their length differs by more than
# X characters
_MAX_FILENAME_LENGTH_DIFFERENCE = 2

# Don't include filenames with these strings
_BLOCKLIST = ['chunk']

# Strings that should be replaced with a wildcard automatically
_WILDCARD_REPLACE = ['en_US']

# Strings to ignore when comparing file names for similarity
_FILENAME_IGNORE = ['.js', '.css', 'bundle', '.min', '.', '-', '[', ']']

_UUID_REGEX = re.compile(r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-'
                         r'[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')

_NAMED_WILDCARD_REGEX = re.compile(r':[a-zA-Z_][a-zA-Z0-9_]*')

# Compression level for the inline zstd-compressed pervasive list
_ZSTD_COMPRESSION_LEVEL = 19

# BigQuery query to fetch resource candidates
_QUERY = """
    #standardSQL
    SELECT
        url,
        ANY_VALUE(dest) as dest,
        ANY_VALUE(size) as size,
        ANY_VALUE(request_headers) as request_headers,
        ANY_VALUE(response_headers) as response_headers,
        body_hash,
        COUNT(*) as num
    FROM (
        SELECT
            url,
            CAST(JSON_VALUE(payload, "$._objectSize") AS INT64) as size,
            JSON_VALUE(payload, "$._body_hash") as body_hash,
            req_h.value as dest,
            request_headers,
            response_headers
        FROM
            `httparchive.crawl.requests`,
            UNNEST (request_headers) as req_h,
            UNNEST (response_headers) as resp_h
        WHERE
            date = @date AND
            JSON_VALUE(payload, "$._body_hash") IS NOT NULL AND
            lower(resp_h.name) = "cache-control" AND
            lower(resp_h.value) LIKE "%public%" AND
            lower(req_h.name) = "sec-fetch-dest" AND
            (lower(req_h.value) = "script" OR
             lower(req_h.value) = "style" OR
             lower(req_h.value) = "empty") AND
            CAST(JSON_VALUE(payload, "$._responseCode") AS INT64) = 200 AND
            CAST(JSON_VALUE(payload, "$._objectSize") AS INT64) > 1000
    ) Hashes
    GROUP BY url, body_hash
    HAVING COUNT(*) > 20000
    ORDER BY num DESC
"""


class PervasiveResourceCollector:
  """Collector for pervasive resources."""

  def __init__(self) -> None:
    self._data_dir = os.path.join(os.path.dirname(__file__), "data")
    if not os.path.exists(self._data_dir):
      os.makedirs(self._data_dir)
    self._dates: List[str] = []

    current_date = datetime.date.today().replace(day=1)
    if not self._is_current_crawl_done():
      current_date -= relativedelta(months=1)

    for _ in range(_MONTHS):
      self._dates.append(current_date.strftime("%Y-%m"))
      current_date -= relativedelta(months=1)
    self._current_date = self._dates[0]
    self._bq_client: Optional[bigquery.Client] = None
    self._origins: Dict[str, Dict[str, Any]] = {}
    self._patterns: List[str] = []
    self._destinations: Dict[str, str] = {}

  def _url_matches_pattern(self, url: str, pattern: str) -> bool:
    """Checks if the given URL matches the provided wildcard pattern.

    This matches the pattern compilation and matching logic of Chrome's
    SimpleUrlPatternMatcher, which implements a restricted subset of the
    URLPattern spec supporting only bare '*' wildcards (matching arbitrary
    depth) and named wildcards (like :v, matching a single path segment).

    Args:
      url: The URL to check.
      pattern: The pattern to match against.

    Returns:
      True if the URL matches the pattern, False otherwise.
    """
    if '*' not in pattern and ':' not in pattern:
      return url == pattern

    assert '__NAMED_WILDCARD__' not in pattern
    assert '__BARE_WILDCARD__' not in pattern
    temp_pattern = re.sub(_NAMED_WILDCARD_REGEX, '__NAMED_WILDCARD__', pattern)
    temp_pattern = temp_pattern.replace('*', '__BARE_WILDCARD__')

    escaped_pattern = re.escape(temp_pattern)

    escaped_pattern = escaped_pattern.replace('__NAMED_WILDCARD__', '[^/]+')
    escaped_pattern = escaped_pattern.replace('__BARE_WILDCARD__', '.*')

    match = re.fullmatch(escaped_pattern, url)
    return match is not None

  def _is_uuid(self, s: str) -> bool:
    """Checks if a string matches the UUID format."""
    return _UUID_REGEX.match(s) is not None

  def _replace_wildcards_with_names(self, pattern: str) -> str:
    """Replaces whole-segment '*' wildcards with unique named wildcards.

    e.g. (:v1, :v2, ...)
    """
    assert not _NAMED_WILDCARD_REGEX.search(pattern)
    parts = pattern.split('/')
    count = 1
    for i, part in enumerate(parts):
      if part == '*':
        parts[i] = f':v{count}'
        count += 1
    if count == 2:
      # Only one wildcard was replaced, use ':v' instead of ':v1'
      parts[parts.index(':v1')] = ':v'
    return '/'.join(parts)


  def _is_current_crawl_done(self) -> bool:
    """Checks if the current month's crawl is expected to be done.

    The crawl starts on the second tuesday of the month.
    It should be done by the third.

    Returns:
      True if the current date is on or after the third Tuesday of the month.
    """
    today = datetime.date.today()
    year = today.year
    month = today.month

    # The third Tuesday will always fall between the 15th and the 21st
    # (inclusive) because the first Tuesday can be as early as the 1st,
    # and the latest the second Tuesday can be is the 14th of the month.
    for day in range(15, 22):
      third_tuesday = datetime.date(year, month, day)
      if third_tuesday.weekday() == calendar.TUESDAY:
        break
    assert third_tuesday is not None
    return today >= third_tuesday

  def _process_headers(self, headers: List[Dict[str, str]]) -> Dict[str, str]:
    """Processes a list of headers into a dictionary, joining duplicates.

    Args:
      headers: A list of dicts with 'name' and 'value' keys.

    Returns:
      A dictionary mapping lowercase header names to values.
    """
    result: Dict[str, str] = {}
    for header in headers:
      name = header['name'].lower()
      value = header['value']
      val = f'{result[name]}, {value}' if name in result else value
      result[name] = val
    return result

  def _query_date(self, date_str: str) -> None:
    """Runs the BigQuery query for a specific date and saves results.

    Args:
      date_str: The date string (YYYY-MM) to query.
    """
    results_file = os.path.join(self._data_dir, f"{date_str}.json")
    if os.path.exists(results_file):
      return

    logging.info("Collecting results for %s...", date_str)
    if self._bq_client is None:
      self._bq_client = bigquery.Client()

    job_config = bigquery.QueryJobConfig(query_parameters=[
        bigquery.ScalarQueryParameter("date", "STRING", f"{date_str}-01"),
    ])
    job = self._bq_client.query(_QUERY, job_config=job_config)

    with open(results_file, "w", encoding="utf-8") as f:
      f.write("[\n")
      is_first = True
      for row in job.result():
        out = dict(row)

        out["request_headers"] = self._process_headers(
            out.get("request_headers", []))
        out["response_headers"] = self._process_headers(
            out.get("response_headers", []))

        if (out["dest"] == "empty"
            and "use-as-dictionary" not in out["response_headers"]):
          continue
        if "?" in out["url"]:
          continue
        if "set-cookie" in out["response_headers"]:
          continue
        if ("cache-control" not in out["response_headers"]
            or "public" not in out["response_headers"]["cache-control"]):
          continue

        if not is_first:
          f.write(",\n")
        json.dump(out, f, separators=(",", ":"))
        is_first = False
      f.write("\n]\n")

  def _collect_raw_data(self) -> None:
    """Runs the raw bigquery queries and stores the results locally."""
    for date_str in self._dates:
      self._query_date(date_str)

  def _load_date(self, date_str: str) -> None:
    """Loads processed JSON data for a given date into memory.

        Args:
          date_str: The date string (YYYY-MM) to load.
        """
    results_file = os.path.join(self._data_dir, f'{date_str}.json')
    with open(results_file, 'r', encoding='utf-8') as f:
      raw = json.load(f)
    for entry in raw:
      url = entry['url']
      body_hash = entry['body_hash']
      num = entry['num']
      parsed_uri = urlparse(url)
      origin_str = f'{parsed_uri.scheme}://{parsed_uri.netloc}'
      path = parsed_uri.path

      # Only consider new origins if they are from the latest crawl
      if origin_str not in self._origins and date_str == self._current_date:
        self._origins[origin_str] = {}
      if origin_str not in self._origins:
        continue

      origin = self._origins[origin_str]
      if path not in origin:
        origin[path] = {}
        self._destinations[url] = entry['dest']
      if date_str not in origin[path]:
        origin[path][date_str] = {}
      if body_hash not in origin[path][date_str]:
        origin[path][date_str][body_hash] = {'count': 0, 'size': entry['size']}
      origin[path][date_str][body_hash]['count'] += num

  def _find_pervasive_urls(self) -> None:
    """Finds URLs that were the same and pervasive for all months."""
    for origin in self._origins:
      for path in list(self._origins[origin].keys()):
        if len(self._origins[origin][path]) == len(self._dates):
          is_pervasive = True
          counts = []
          for date_str in self._dates:
            total_count = 0
            if date_str in self._origins[origin][path]:
              for body_hash in self._origins[origin][path][date_str]:
                total_count += self._origins[origin][path][date_str][body_hash][
                    'count']
            counts.append(total_count)
            if total_count < _PERVASIVE_COUNT:
              is_pervasive = False
              break
          if is_pervasive:
            logging.info('Pervasive static URL %s: %s%s', counts, origin, path)
            url = f'{origin}{path}'
            if url not in self._patterns:
              self._patterns.append(url)
            del self._origins[origin][path]

  def _remove_static_urls(self) -> None:
    """Removes URLs that were present in all months and did not change.

    We do this after extracting the pervasive ones to make sure we don't
    use these URLs when generating patterns for the remaining resources.
    """
    for origin in self._origins:
      for path in list(self._origins[origin].keys()):
        if len(self._origins[origin][path]) == len(self._dates):
          hashes = []
          for date_str in self._origins[origin][path]:
            for body_hash in self._origins[origin][path][date_str]:
              if body_hash not in hashes:
                hashes.append(body_hash)
          if len(hashes) == 1:
            logging.debug('Removed non-pervasive static URL: %s%s', origin,
                          path)
            del self._origins[origin][path]

  def _remove_unversioned_urls(self) -> None:
    """Finds URLs that have multiple hashes since they are updated in-place."""
    for origin in self._origins:
      for path in list(self._origins[origin].keys()):
        hashes = []
        for date_str in self._origins[origin][path]:
          for body_hash in self._origins[origin][path][date_str]:
            if body_hash not in hashes:
              hashes.append(body_hash)
        if len(hashes) > 1:
          logging.debug('Removed unversioned URL: %s%s', origin, path)
          del self._origins[origin][path]

  def _remove_blocked_urls(self) -> None:
    """Removes any URLs that have a blocked string in their file name."""
    for origin in self._origins:
      for path in list(self._origins[origin].keys()):
        filepart = path.split('/')[-1]
        if self._is_blocked(filepart):
          logging.debug('Removed blocked URL: %s%s', origin, path)
          del self._origins[origin][path]

  def _remove_long_urls(self) -> None:
    """Removes any URLs longer than the configured maximum length."""
    for origin in self._origins:
      for path in list(self._origins[origin].keys()):
        if len(f'{origin}{path}') > _MAX_URL_LENGTH:
          logging.debug('Removed long URL: %s%s', origin, path)
          del self._origins[origin][path]

  def _create_filename_pattern(self, path: str,
                               candidates: List[str]) -> Optional[str]:
    """Creates a wildcard that will match only the provided filenames.

    Args:
      path: The base path.
      candidates: List of candidate paths.

    Returns:
      A wildcard pattern string or None if no suitable pattern found.
    """
    common = None
    last = None
    first = None
    file_name = path.split('/')[-1]
    for p in candidates:
      f = p.split('/')[-1]
      if f != file_name:
        matcher = difflib.SequenceMatcher(None, file_name, f)
        matches = matcher.get_matching_blocks()
        if common is None:
          common = []
          for match in matches:
            start, _, size = match
            if size > 0:
              end = start + size
              if first is None or start < first:
                first = start
              if last is None or end > last:
                last = end
              common.append((start, end))
        else:
          # Only include the intersection of both match sets
          intersect = []
          last = None
          first = None
          for match in matches:
            start, _, size = match
            if size > 0:
              end = start + size
              for cmatch in common:
                cstart, cend = cmatch
                intersect_start = max(start, cstart)
                intersect_end = min(end, cend)
                if intersect_start < intersect_end:
                  if first is None or intersect_start < first:
                    first = intersect_start
                  if last is None or intersect_end > last:
                    last = intersect_end
                  intersect.append((intersect_start, intersect_end))
          common = intersect
    if common is None:
      return None
    if not common:
      return '*'
    pattern = ''
    if first != 0:
      pattern += '*'
    is_first = True
    for chunk in common:
      start, end = chunk
      if not is_first and (not pattern or pattern[-1] != '*'):
        pattern += '*'
      is_first = False
      part = file_name[start:end]
      if len(part) > 1:
        pattern += part
    if last != len(file_name):
      pattern += '*'
    return pattern

  def _find_path_pattern(self, path: str,
                         candidates: List[str]) -> Optional[str]:
    """Finds cases where one path segment has the version or hash.

    e.g. /maps/1.2.3/common.js

    Args:
      path: The URL path.
      candidates: List of candidate paths.

    Returns:
      A pattern string or None.
    """
    differences = []
    path_parts = path.split('/')
    for candidate in candidates:
      candidate_parts = candidate.split('/')
      if len(candidate_parts) != len(path_parts):
        return None
      for index in range(len(path_parts)):
        if path_parts[index] != candidate_parts[
            index] and index not in differences:
          differences.append(index)
    differences.sort()

    if not differences:
      return None

    # Do not support more that 5 path segments that need to be wildcards.
    # We normally expect for there to be only one or two.
    if len(differences) > 5:
      return None

    # Defense-in-depth fallback: Reject the pattern if any differing segment in
    # the base path or candidates is a UUID. This prevents generating an overly
    # broad wildcard segment that would match arbitrary tenant/extension UUIDs.
    for diff in differences:
      if self._is_uuid(path_parts[diff]):
        return None
      for candidate in candidates:
        candidate_parts = candidate.split('/')
        if (len(candidate_parts) > diff
            and self._is_uuid(candidate_parts[diff])):
          return None

    # The case where a small number of path segments differ
    # and the filename is the same
    filename_matches = differences[-1] != len(path_parts) - 1
    if len(path_parts) - len(
        differences) > _MIN_STABLE_PATH and filename_matches:
      pattern = path
      for diff in differences:
        pattern = pattern.replace(f'/{path_parts[diff]}/', '/*/')
      return pattern
    # Special-case a single difference
    if len(differences) == 1 and differences[0] != len(path_parts) - 1:
      pattern = path.replace(f'/{path_parts[differences[0]]}/', '/*/')
      return pattern
    # The case where the file name differs and, optionally, a small number of
    # path segments
    if differences[-1] == len(path_parts) - 1 and (
        len(differences) == 2
        or len(path_parts) - len(differences) > _MIN_STABLE_PATH):
      filename_pattern = self._create_filename_pattern(path, candidates)
      if filename_pattern is not None:
        pattern = path
        for diff in differences[:-1]:
          pattern = pattern.replace(f'/{path_parts[diff]}/', '/*/')
        pattern = pattern.replace(f'/{path_parts[differences[-1]]}',
                                  f'/{filename_pattern}')
        return pattern
    return None

  def _matches_existing_pattern(self, url: str) -> bool:
    """See if the given URL matches a pattern we already have."""
    if url in self._patterns:
      return True
    for pattern in self._patterns:
      if self._url_matches_pattern(url, pattern):
        return True
    return False

  def _is_blocked(self, path: str) -> bool:
    """Return true if the file component of the path has any blocked strings."""
    file_name = path.split('/')[-1]
    for block in _BLOCKLIST:
      if block in file_name:
        return True
    return False

  def _find_patterns(self) -> None:
    """Finds patterns for remaining requests.

    Take the remaining requests and see if there are similar urls that
    are pervasive as a set and automate generating a pattern for them.
    """
    for origin in self._origins:
      o = self._origins[origin]
      for path in list(o.keys()):
        url = f'{origin}{path}'
        filepart = path.split('/')[-1]
        for ignore in _FILENAME_IGNORE:
          filepart = filepart.replace(ignore, '')
        if (url in self._destinations and path in o
            and self._current_date in o[path]
            and not self._matches_existing_pattern(url)):
          dest = self._destinations[url]
          body_hash = list(o[path][self._current_date].keys())[0]
          target_size = o[path][self._current_date][body_hash]['size']
          path_parts = path.split('/')
          path_segments = len(path_parts)
          # Find candidate paths that are within 5% of the target size
          # (assume minor changes from version to version) with "similar" urls.
          # UUID segments in the paths must match exactly.
          candidates = []
          target_size_min = target_size
          target_size_max = target_size
          for p in list(o.keys()):
            p_parts = p.split('/')
            if len(p_parts) != path_segments:
              continue
            uuid_mismatch = False
            for idx, part in enumerate(path_parts):
              if self._is_uuid(part) and p_parts[idx] != part:
                uuid_mismatch = True
                break
            if uuid_mismatch:
              continue

            curl = f'{origin}{p}'
            cdest = self._destinations.get(curl)
            cfilepart = p.split('/')[-1]
            for ignore in _FILENAME_IGNORE:
              cfilepart = cfilepart.replace(ignore, '')
            s = difflib.SequenceMatcher(None, filepart, cfilepart)
            similarity = s.ratio()
            block_count = len(s.get_matching_blocks())
            if (p != path and p not in candidates and cdest == dest
                and similarity >= _MIN_FILENAME_RATIO
                and block_count <= _MAX_FILENAME_MATCHING_BLOCKS
                and abs(len(filepart) - len(cfilepart))
                <= _MAX_FILENAME_LENGTH_DIFFERENCE):
              date_str = list(o[p].keys())[0]
              body_hash = list(o[p][date_str].keys())[0]
              size = o[p][date_str][body_hash]['size']
              size_delta = 0
              if size < target_size_min:
                size_delta = (target_size_min - size) * 100
              elif size > target_size_max:
                size_delta = (size - target_size_max) * 100
              if size_delta / target_size <= _SIZE_MATCH_PERCENT:
                target_size_min = min(target_size_min, size)
                target_size_max = max(target_size_max, size)
                candidates.append(p)
          if candidates:
            pattern = self._find_path_pattern(path, candidates)
            if pattern:
              for sub in _WILDCARD_REPLACE:
                pattern = pattern.replace(sub, '*')
              pattern = self._replace_wildcards_with_names(pattern)
              # Make sure the aggregate of all of the candidates meet the
              # pervasive threshold
              matched_urls = []
              counts = []
              is_pervasive = True
              for date_str in self._dates:
                total_count = 0
                for p in o:
                  if self._url_matches_pattern(p, pattern):
                    matched_urls.append(p)
                    if date_str in o[p]:
                      body_hash = list(o[p][date_str].keys())[0]
                      total_count += o[p][date_str][body_hash]['count']
                counts.append(total_count)
                if total_count < _PERVASIVE_COUNT:
                  is_pervasive = False
              if is_pervasive:
                logging.info('Pattern %s: %s%s', counts, origin, pattern)
                logging.info('             URL: %s%s', origin, path)
                for path2 in sorted(candidates):
                  logging.info('       Candidate: %s%s', origin, path2)
                for path2 in sorted(matched_urls):
                  logging.info('         Matched: %s%s', origin, path2)
                self._patterns.append(f'{origin}{pattern}')
              # clean up all of the paths that were used with the pattern
              for path2 in matched_urls:
                if path2 in o:
                  del o[path2]
              continue

  def _show_unmatched(self) -> None:
    """Display the remaining unmatched URLs."""
    for origin in self._origins:
      for path in sorted(list(self._origins[origin].keys())):
        if self._current_date in self._origins[origin][path]:
          counts = []
          for date_str in self._dates:
            total_count = 0
            if date_str in self._origins[origin][path]:
              for body_hash in self._origins[origin][path][date_str]:
                total_count += self._origins[origin][path][date_str][body_hash][
                    'count']
            counts.append(total_count)
          logging.info('Unmatched URL %s: %s%s', counts, origin, path)

  def _write_patterns(self) -> None:
    """Writes the patterns to disk."""
    patterns_file = os.path.join(os.path.dirname(__file__), 'patterns.txt')
    # Write them out to a straight text file
    with open(patterns_file, 'w', encoding='utf-8') as f:
      for pattern in self._patterns:
        f.write(pattern)
        f.write('\n')
    # Write them out formatted for Chrome's compiled list, including expiration
    template_file = os.path.join(os.path.dirname(__file__),
                                 'shared_resource_checker_patterns.template')
    cc_file = os.path.join(os.path.dirname(__file__),
                           'shared_resource_checker_patterns.h')
    with open(template_file, 'r', encoding='utf-8') as f:
      template_string = f.read()
    template = string.Template(template_string)
    expires = datetime.date.today() + relativedelta(years=1)
    patterns_comment = ''
    for pattern in self._patterns:
      patterns_comment += f'// {pattern}\n'
    patterns_comment = patterns_comment.strip()
    patterns = '\n'.join(self._patterns).strip()
    cctx = zstd.ZstdCompressor(level=_ZSTD_COMPRESSION_LEVEL)
    compressed_data = cctx.compress(patterns.encode('utf-8'))
    patterns_zstd = ''
    # Generate a c++ hex byte array with 12 bytes per line
    is_first = True
    row_index = 0
    for b in compressed_data:
      if is_first:
        patterns_zstd += '    '
        is_first = False
      elif row_index == 0:
        patterns_zstd += ',\n    '
      else:
        patterns_zstd += ', '
      patterns_zstd += f'0x{b:02x}'
      row_index += 1
      if row_index == 12:
        row_index = 0

    out = template.substitute(year=expires.year,
                              month=expires.month,
                              day=expires.day,
                              patterns_comment=patterns_comment,
                              patterns_zstd=patterns_zstd)
    with open(cc_file, "w", encoding="utf-8") as f:
      f.write(out)

  def _remove_duplicate_patterns(self) -> None:
    """Remove patterns that are already covered by a more general pattern."""
    to_remove = set()
    for pattern1 in self._patterns:
      for pattern2 in self._patterns:
        if pattern1 != pattern2 and len(pattern1) != len(pattern2):
          shorter = pattern1 if len(pattern1) < len(pattern2) else pattern2
          longer = pattern1 if len(pattern1) > len(pattern2) else pattern2
          if self._url_matches_pattern(longer, shorter):
            to_remove.add(longer)

    for pattern in to_remove:
      logging.info("Removed pattern %s as a duplicate", pattern)
      self._patterns.remove(pattern)

  def _aggregate_urls(self) -> None:
    """Loads the raw results and group them by origin."""
    for date_str in self._dates:
      self._load_date(date_str)
    self._find_pervasive_urls()
    self._remove_long_urls()
    self._remove_static_urls()
    self._remove_unversioned_urls()
    self._remove_blocked_urls()
    self._find_patterns()
    self._remove_duplicate_patterns()
    self._show_unmatched()
    self._patterns.sort()
    self._write_patterns()

  def run(self) -> None:
    """Main execution entry point."""
    self._collect_raw_data()
    self._aggregate_urls()


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO,
                      format='%(asctime)s.%(msecs)03d - %(message)s',
                      datefmt='%H:%M:%S')
  collector = PervasiveResourceCollector()
  collector.run()
