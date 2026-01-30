# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from datetime import date
from dateutil.relativedelta import relativedelta
from google.cloud import bigquery
from urllib.parse import urlparse
import calendar
import difflib
import logging
import os
import pandas as pd
import re
import string
import zstandard as zstd
try:
  import ujson as json
except BaseException:
  import json

# Minimum number of ovvurrences per month to be considered "pervasive"
PERVASIVE_COUNT = 100000

# Longest URL to include
MAX_URL_LENGTH = 200

# File sizes to consider as "similar" across URLs
SIZE_MATCH_PERCENT = 5

# Number of Months to evaluate
MONTHS = 6

# Minimum number of path components that must match
MIN_STABLE_PATH = 2

# difflib.ratio() similarity in filenames when looking for matching candidates
MIN_FILENAME_RATIO = 0.5

# difflib.find_matching_blocks() maximum number of matching blocks to consider
# filenames "similar" (including the ending dummy block)
MAX_FILENAME_MATCHING_BLOCKS = 3

# Don't allow for filenames to be similar if their length differs by more than
# X characters
MAX_FILENAME_LENGTH_DIFFERENCE = 2

# Don't include filenames with these strings
BLOCKLIST = ['chunk']

# Strings that should be replaced with a wildcard automatically
WILDCARD_REPLACE = ['en_US']

# Strings to ignore when comparing file names for similarity
FILENAME_IGNORE = ['.js', '.css', 'bundle', '.min', '.', '-', '[', ']']

# Compression level for the inline zstd-compressed pervasive list
ZSTD_COMPRESSION_LEVEL = 19


class Collect(object):

  def __init__(self):
    self.query = """
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
                PARSE_NUMERIC(JSON_VALUE(payload, "$._objectSize")) as size,
                JSON_VALUE(payload, "$._body_hash") as body_hash,
                req_h.value as dest,
                request_headers,
                response_headers
            FROM
                `httparchive.crawl.requests`,
                UNNEST (request_headers) as req_h,
                UNNEST (response_headers) as resp_h
            WHERE
                date = "{}-01" AND
                JSON_VALUE(payload, "$._body_hash") IS NOT NULL AND
                lower(resp_h.name) = "cache-control" AND
                lower(resp_h.value) LIKE "%public%" AND
                lower(req_h.name) = "sec-fetch-dest" AND
                (lower(req_h.value) = "script" OR
                 lower(req_h.value) = "style" OR
                 lower(req_h.value) = "empty") AND
                PARSE_NUMERIC(JSON_VALUE(payload, "$._responseCode")) = 200 AND
                PARSE_NUMERIC(JSON_VALUE(payload, "$._objectSize")) > 1000
        ) Hashes
        GROUP BY url, body_hash
        HAVING COUNT(*) > 20000
        ORDER BY num DESC
    """
    self.data_dir = os.path.join(os.path.dirname(__file__), 'data')
    if not os.path.exists(self.data_dir):
      os.makedirs(self.data_dir)
    self.dates = []
    today = date.today()
    year = today.year
    month = today.month
    # Start with the current month after the 20th of the month
    if not self.is_current_crawl_done():
      month -= 1
    for _ in range(MONTHS):
      if month == 0:
        month = 12
        year -= 1
      self.dates.append('{}-{:02d}'.format(year, month))
      month -= 1
    self.current_date = self.dates[0]
    self.bq_client = None
    self.origins = {}
    self.patterns = []
    self.destinations = {}

  def url_matches_pattern(self, url, pattern):
    """ See if the given URL matches the provided wildcard pattern. """
    if "*" not in pattern:
      return url == pattern
    pat = re.escape(pattern)
    pat = pat.replace("\\*", ".*")
    match = re.fullmatch(pat, url)
    return match is not None

  def is_current_crawl_done(self):
    """
    The crawl starts on the second tuesday of the month.
    It should be done by the third.
    """
    today = date.today()
    year = today.year
    month = today.month

    # The third Tuesday will always fall between the 15th and the 21st
    # (inclusive) because the first Tuesday can be as early as the 1st,
    # and the latest the second Tuesday can be is the 14th of the month.
    for day in range(15, 22):
      third_tuesday = date(year, month, day)
      if third_tuesday.weekday() == calendar.TUESDAY:
        break
    return today <= third_tuesday

  def process_headers(self, headers):
    result = {}
    for header in headers:
      name = header['name'].lower()
      value = header['value']
      val = '{}, {}'.format(result[name], value) if name in result else value
      result[name] = val
    return result

  def query_date(self, date):
    results_file = os.path.join(self.data_dir, '{}.json'.format(date))
    if not os.path.exists(results_file):
      logging.info("Collecting results for %s...", date)
      query = self.query.format(date)
      if self.bq_client is None:
        self.bq_client = bigquery.Client()
      job = self.bq_client.query(query)
      # Convert query results to a Pandas DataFrame
      df = job.to_dataframe()
      results = json.loads(df.to_json(orient="records", date_format='iso'))
      with open(results_file, 'w', encoding='utf-8') as f:
        f.write('[')
        is_first = True
        for result in results:
          out = dict(result)
          out['request_headers'] = self.process_headers(
              result['request_headers'])
          out['response_headers'] = self.process_headers(
              result['response_headers'])
          # only allow "empty" dest if it is a compression dictionary
          if out['dest'] == 'empty' and 'use-as-dictionary' not in out[
              'response_headers']:
            continue
          # Exclude requests with query parameters
          if '?' in out['url']:
            continue
          # Exclude any responses with a set-cookie response header
          if 'set-cookie' in out['response_headers']:
            continue
          # Exclude any responses that are not "cache-control: public"
          if 'cache-control' not in out[
              'response_headers'] or 'public' not in out['response_headers'][
                  'cache-control']:
            continue
          # write the candidate
          if is_first:
            is_first = False
            f.write('\n')
          else:
            f.write(',\n')
          json.dump(out, f)
        f.write('\n]\n')

  def collect_raw_data(self):
    """ Run the raw bigquery queries and store the results locally """
    for date in self.dates:
      self.query_date(date)

  def load_date(self, date):
    results_file = os.path.join(self.data_dir, '{}.json'.format(date))
    raw = []
    with open(results_file, 'r', encoding='utf-8') as f:
      raw = json.load(f)
    for entry in raw:
      url = entry['url']
      hash = entry['body_hash']
      num = entry['num']
      parsed_uri = urlparse(url)
      origin_str = '{uri.scheme}://{uri.netloc}'.format(uri=parsed_uri)
      path = parsed_uri.path
      # Only consider new origins if they are from the latest crawl
      if origin_str not in self.origins and date == self.current_date:
        self.origins[origin_str] = {}
      if origin_str not in self.origins:
        continue
      origin = self.origins[origin_str]
      if path not in origin:
        origin[path] = {}
        self.destinations[url] = entry['dest']
      if date not in origin[path]:
        origin[path][date] = {}
      if hash not in origin[path][date]:
        origin[path][date][hash] = {'count': 0, 'size': entry['size']}
      origin[path][date][hash]['count'] += num

  def find_pervasive_urls(self):
    """ Find URLs that were the same and pervasive for all months """
    for origin in self.origins:
      for path in list(self.origins[origin].keys()):
        if len(self.origins[origin][path]) == len(self.dates):
          is_pervasive = True
          counts = []
          for date in self.dates:
            total_count = 0
            if date in self.origins[origin][path]:
              for hash in self.origins[origin][path][date]:
                total_count += self.origins[origin][path][date][hash]['count']
            counts.append(total_count)
            if total_count < PERVASIVE_COUNT:
              is_pervasive = False
              break
          if is_pervasive:
            logging.info(f"Pervasive static URL {counts}: {origin}{path}")
            url = f"{origin}{path}"
            if url not in self.patterns:
              self.patterns.append(url)
            del self.origins[origin][path]

  def remove_static_urls(self):
    """
    Find URLs that were present in all months and did not change.
    We do this after extracting the pervasive ones to make sure we don't
    use these URLs when generating patterns for the remaining resources.
    """
    for origin in self.origins:
      for path in list(self.origins[origin].keys()):
        if len(self.origins[origin][path]) == len(self.dates):
          hashes = []
          for date in self.origins[origin][path]:
            for hash in self.origins[origin][path][date]:
              if hash not in hashes:
                hashes.append(hash)
          if len(hashes) == 1:
            logging.debug(f"Removed non-pervasive static URL: {origin}{path}")
            del self.origins[origin][path]

  def remove_unversioned_urls(self):
    """ Find URLs that have multiple hashes since they are updated in-place """
    for origin in self.origins:
      for path in list(self.origins[origin].keys()):
        hashes = []
        for date in self.origins[origin][path]:
          for hash in self.origins[origin][path][date]:
            if hash not in hashes:
              hashes.append(hash)
        if len(hashes) > 1:
          logging.debug(f"Removed unversioned URL: {origin}{path}")
          del self.origins[origin][path]

  def remove_blocked_urls(self):
    """ Remove any URLs that have a blocked string in their file name """
    for origin in self.origins:
      for path in list(self.origins[origin].keys()):
        filepart = path.split("/")[-1]
        if self.is_blocked(filepart):
          logging.debug(f"Removed blocked URL: {origin}{path}")
          del self.origins[origin][path]

  def remove_long_urls(self):
    """ Remove any URLs longer than 200 characters """
    for origin in self.origins:
      for path in list(self.origins[origin].keys()):
        if len(f"{origin}{path}") > MAX_URL_LENGTH:
          logging.debug(f"Removed long URL: {origin}{path}")
          del self.origins[origin][path]

  def find_first_difference(self, str1, str2):
    for index, (char1, char2) in enumerate(zip(str1, str2)):
      if char1 != char2:
        return index
    return None

  def create_filename_pattern(self, path, candidates):
    """ Create a wildcard that will match only the provided filenames """
    common = None
    last = None
    first = None
    file = path.split('/')[-1]
    for p in candidates:
      f = p.split('/')[-1]
      if f != file:
        s = difflib.SequenceMatcher(None, file, f)
        matches = s.get_matching_blocks()
        if common == None:
          common = []
          for match in matches:
            start, _, size = match
            if size > 0:
              end = start + size
              if first == None or start < first:
                first = start
              if last == None or end > last:
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
                s = max(start, cstart)
                e = min(end, cend)
                if s < e:
                  if first == None or s < first:
                    first = s
                  if last == None or e > last:
                    last = e
                  intersect.append((s, e))
          common = intersect
    if common is None:
      return None
    if not common:
      return "*"
    pattern = ""
    if first != 0:
      pattern += "*"
    is_first = True
    for chunk in common:
      start, end = chunk
      if not is_first and (not pattern or pattern[-1] != "*"):
        pattern += "*"
      is_first = False
      #part = file[start:end].strip("1234567890.-")
      part = file[start:end]
      if len(part) > 1:
        pattern += part
    if last != len(file):
      pattern += "*"
    return pattern

  def find_path_pattern(self, origin, path, candidates):
    """
    Find the cases where one path segment has the version or hash
    i.e. /maps/1.2.3/common.js
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

    # The case where a small number of path segments differ
    # and the filename is the same
    filename_matches = differences[-1] != len(path_parts) - 1
    if len(path_parts) - len(
        differences) > MIN_STABLE_PATH and filename_matches:
      pattern = path
      for diff in differences:
        pattern = pattern.replace(f"/{path_parts[diff]}/", "/*/")
      return pattern
    # Special-case a single difference
    if len(differences) == 1 and differences[0] != len(path_parts) - 1:
      pattern = path.replace(f"/{path_parts[differences[0]]}/", "/*/")
      return pattern
    # The case where the file name differs and, optionally, a small number of
    # path segments
    if differences[-1] == len(path_parts) - 1 and (
        len(differences) == 2
        or len(path_parts) - len(differences) > MIN_STABLE_PATH):
      filename_pattern = self.create_filename_pattern(path, candidates)
      if filename_pattern is not None:
        pattern = path
        for diff in differences[:-1]:
          pattern = pattern.replace(f"/{path_parts[diff]}/", "/*/")
        pattern = pattern.replace(f"/{path_parts[differences[-1]]}",
                                  f"/{filename_pattern}")
        return pattern
    return None

  def matches_existing_pattern(self, url):
    """ See if the given URL matches a pattern we already have """
    if url in self.patterns:
      return True
    for pattern in self.patterns:
      if self.url_matches_pattern(url, pattern):
        return True
    return False

  def is_blocked(self, path):
    """
    Check to see if the file component of the path has any of the
    blocked strings in it.
    """
    file = path.split('/')[-1]
    for block in BLOCKLIST:
      if block in file:
        return True
    return False

  def find_patterns(self):
    """
    Take the remaining requests and see if there are similar urls that
    are pervasive as a set and automate generating a pattern for them.
    """
    for origin in self.origins:
      o = self.origins[origin]
      for path in list(o.keys()):
        url = f"{origin}{path}"
        filepart = path.split('/')[-1]
        for ignore in FILENAME_IGNORE:
          filepart = filepart.replace(ignore, "")
        if url in self.destinations and path in o and self.current_date in o[
            path] and not self.matches_existing_pattern(url):
          dest = self.destinations[url]
          hash = list(o[path][self.current_date].keys())[0]
          target_size = o[path][self.current_date][hash]['size']
          path_segments = len(path.split('/'))
          # Find candidate paths that are within 5% of the target size
          # (assume minor changes from version to version)
          # with "similar" urls
          candidates = []
          target_size_min = target_size
          target_size_max = target_size
          for p in list(o.keys()):
            curl = f"{origin}{p}"
            cdest = self.destinations[
                curl] if curl in self.destinations else None
            cfilepart = p.split('/')[-1]
            for ignore in FILENAME_IGNORE:
              cfilepart = cfilepart.replace(ignore, "")
            s = difflib.SequenceMatcher(None, filepart, cfilepart)
            similarity = s.ratio()
            block_count = len(s.get_matching_blocks())
            if p != path and \
                    p not in candidates and \
                    cdest == dest and \
                    len(p.split('/')) == path_segments and \
                    similarity >= MIN_FILENAME_RATIO and \
                    block_count <= MAX_FILENAME_MATCHING_BLOCKS and \
                    abs(len(filepart) - len(cfilepart)) <= \
                        MAX_FILENAME_LENGTH_DIFFERENCE:
              date = list(o[p].keys())[0]
              hash = list(o[p][date].keys())[0]
              size = o[p][date][hash]['size']
              size_delta = 0
              if size < target_size_min:
                size_delta = (target_size_min - size) * 100
              elif size > target_size_max:
                size_delta = (size - target_size_max) * 100
              if size_delta / target_size <= SIZE_MATCH_PERCENT:
                target_size_min = min(target_size_min, size)
                target_size_max = max(target_size_max, size)
                candidates.append(p)
          if candidates:
            pattern = self.find_path_pattern(origin, path, candidates)
            if pattern:
              for sub in WILDCARD_REPLACE:
                pattern = pattern.replace(sub, "*")
              while "/*/*/" in pattern:
                pattern = pattern.replace("/*/*/", "/*/")
              # Make sure the aggregate of all of the candidates meet the
              # pervasive threshold
              matched_urls = []
              counts = []
              is_pervasive = True
              for date in self.dates:
                total_count = 0
                for p in o:
                  if self.url_matches_pattern(p, pattern):
                    matched_urls.append(p)
                    if date in o[p]:
                      hash = list(o[p][date].keys())[0]
                      total_count += o[p][date][hash]['count']
                counts.append(total_count)
                if total_count < PERVASIVE_COUNT:
                  is_pervasive = False
              if is_pervasive:
                logging.info(f"Pattern {counts}: {origin}{pattern}")
                logging.info(f"             URL: {origin}{path}")
                for path2 in sorted(candidates):
                  logging.info(f"       Candidate: {origin}{path2}")
                for path2 in sorted(matched_urls):
                  logging.info(f"         Matched: {origin}{path2}")
                self.patterns.append(f"{origin}{pattern}")
              # clean up all of the paths that were used with the pattern
              for path2 in matched_urls:
                if path2 in o:
                  del o[path2]
              continue

  def show_unmatched(self):
    """ Display the remaining unmatched URLs """
    for origin in self.origins:
      for path in sorted(list(self.origins[origin].keys())):
        if self.current_date in self.origins[origin][path]:
          counts = []
          for date in self.dates:
            total_count = 0
            if date in self.origins[origin][path]:
              for hash in self.origins[origin][path][date]:
                total_count += self.origins[origin][path][date][hash]['count']
            counts.append(total_count)
          logging.info(f"Unmatched URL {counts}: {origin}{path}")

  def write_patterns(self):
    """ Write the patterns to disk """
    patterns_file = os.path.join(os.path.dirname(__file__), 'patterns.txt')
    # Write them out to a straight text file
    with open(patterns_file, "w", encoding="utf-8") as f:
      for pattern in self.patterns:
        f.write(pattern)
        f.write("\n")
    # Write them out formatted for Chrome's compiled list, including expiration
    template_file = os.path.join(os.path.dirname(__file__),
                                 'shared_resource_checker_patterns.template')
    cc_file = os.path.join(os.path.dirname(__file__),
                           'shared_resource_checker_patterns.h')
    with open(template_file, "r", encoding="utf-8") as f:
      template_string = f.read()
    template = string.Template(template_string)
    expires = date.today() + relativedelta(years=1)
    patterns_comment = ""
    for pattern in self.patterns:
      patterns_comment += f"// {pattern}\n"
    patterns_comment = patterns_comment.strip()
    patterns = "\n".join(self.patterns).strip()
    cctx = zstd.ZstdCompressor(level=ZSTD_COMPRESSION_LEVEL)
    compressed_data = cctx.compress(patterns.encode('utf-8'))
    patterns_zstd = ""
    # Generate a c++ hex byte array with 12 bytes per line
    is_first = True
    row_index = 0
    for b in compressed_data:
      if is_first:
        patterns_zstd += "    "
        is_first = False
      elif row_index == 0:
        patterns_zstd += ",\n    "
      else:
        patterns_zstd += ", "
      patterns_zstd += f"0x{b:02x}"
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

  def remove_duplicate_patterns(self):
    """ Remove patterns that are already covered by a more general pattern """
    for pattern1 in list(self.patterns):
      for pattern2 in list(self.patterns):
        if pattern1 != pattern2 and len(pattern1) != len(pattern2):
          shorter = pattern1 if len(pattern1) < len(pattern2) else pattern2
          longer = pattern1 if len(pattern1) > len(pattern2) else pattern2
          if self.url_matches_pattern(longer, shorter):
            logging.info(f"Removed pattern {longer} as duplicate of {shorter}")
            self.patterns.remove(longer)

  def aggregate_urls(self):
    """ Load the raw results and group them by origin """
    for date in self.dates:
      self.load_date(date)
    self.find_pervasive_urls()
    self.remove_long_urls()
    self.remove_static_urls()
    self.remove_unversioned_urls()
    self.remove_blocked_urls()
    self.find_patterns()
    self.remove_duplicate_patterns()
    self.show_unmatched()
    self.patterns.sort()
    self.write_patterns()

  def run(self):
    self.collect_raw_data()
    self.aggregate_urls()


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO,
                      format="%(asctime)s.%(msecs)03d - %(message)s",
                      datefmt="%H:%M:%S")
  collect = Collect()
  collect.run()
