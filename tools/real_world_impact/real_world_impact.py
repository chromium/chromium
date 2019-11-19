#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool for seeing the real world impact of a patch.
#
# Layout Tests can tell you whether something has changed, but this can help
# you determine whether a subtle/controversial change is beneficial or not.
#
# It dumps the rendering of a large number of sites, both with and without a
# patch being evaluated, then sorts them by greatest difference in rendering,
# such that a human reviewer can quickly review the most impacted sites,
# rather than having to manually try sites to see if anything changes.
#
# In future it might be possible to extend this to other kinds of differences,
# e.g. page load times.

from __future__ import print_function

import argparse
from argparse import RawTextHelpFormatter
from contextlib import closing
import datetime
import errno
from distutils.spawn import find_executable
from operator import itemgetter
import multiprocessing
import os
import re
from cStringIO import StringIO
import subprocess
import sys
import textwrap
import time
from urllib2 import urlopen
from urlparse import urlparse
import webbrowser
from zipfile import ZipFile

from nsfw_urls import nsfw_urls

action = None
allow_js = False
additional_content_shell_flags = ""
chromium_src_root = ""
chromium_out_dir = ""
image_diff = ""
content_shell = ""
output_dir = ""
num_sites = 100
urls = []
print_lock = multiprocessing.Lock()


def MakeDirsIfNotExist(dir):
  try:
    os.makedirs(dir)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def SetupPathsAndOut():
  global chromium_src_root, chromium_out_dir, output_dir
  global image_diff, content_shell
  chromium_src_root = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                   os.pardir,
                                                   os.pardir))
  # Find out directory (might be out_linux for users of cr).
  for out_suffix in ["_linux", ""]:
    out_dir = os.path.join(chromium_src_root, "out" + out_suffix)
    if os.path.exists(out_dir):
      chromium_out_dir = out_dir
      break
  if not chromium_out_dir:
    return False

  this_script_name = "real_world_impact"
  output_dir = os.path.join(chromium_out_dir,
                            "Release",
                            this_script_name)
  MakeDirsIfNotExist(output_dir)

  image_diff = os.path.join(chromium_out_dir, "Release", "image_diff")

  if sys.platform == 'darwin':
    content_shell = os.path.join(chromium_out_dir, "Release",
                    "Content Shell.app/Contents/MacOS/Content Shell")
  elif sys.platform.startswith('linux'):
    content_shell = os.path.join(chromium_out_dir, "Release",
                    "content_shell")
  elif sys.platform.startswith('win'):
    content_shell = os.path.join(chromium_out_dir, "Release",
                    "content_shell.exe")
  return True


def CheckPrerequisites():
  if not find_executable("wget"):
    print("wget not found! Install wget and re-run this.")
    return False
  if not os.path.exists(image_diff):
    print("image_diff not found (%s)!" % image_diff)
    print("Build the image_diff target and re-run this.")
    return False
  if not os.path.exists(content_shell):
    print("Content shell not found (%s)!" % content_shell)
    print("Build Release/content_shell and re-run this.")
    return False
  return True


def PickSampleUrls():
  global urls
  data_dir = os.path.join(output_dir, "data")
  MakeDirsIfNotExist(data_dir)

  # Download Alexa top 1,000,000 sites
  # TODO(johnme): Should probably update this when it gets too stale...
  csv_path = os.path.join(data_dir, "top-1m.csv")
  if not os.path.exists(csv_path):
    print("Downloading list of top 1,000,000 sites from Alexa...")
    csv_url = "http://s3.amazonaws.com/alexa-static/top-1m.csv.zip"
    with closing(urlopen(csv_url)) as stream:
      ZipFile(StringIO(stream.read())).extract("top-1m.csv", data_dir)

  bad_urls_path = os.path.join(data_dir, "bad_urls.txt")
  if os.path.exists(bad_urls_path):
    with open(bad_urls_path) as f:
      bad_urls = set(f.read().splitlines())
  else:
    bad_urls = set()

  # See if we've already selected a sample of size num_sites (this way, if you
  # call this script with arguments "before N" then "after N", where N is the
  # same number, we'll use the same sample, as expected!).
  urls_path = os.path.join(data_dir, "%06d_urls.txt" % num_sites)
  if not os.path.exists(urls_path):
    if action == 'compare':
      print("Error: you must run 'before %d' and 'after %d' before "
            "running 'compare %d'" % (num_sites, num_sites, num_sites))
      return False
    print("Picking %d sample urls..." % num_sites)

    # TODO(johnme): For now this just gets the top num_sites entries. In future
    # this should pick a weighted random sample. For example, it could fit a
    # power-law distribution, which is a good model of website popularity
    # (http://www.useit.com/alertbox/9704b.html).
    urls = []
    remaining_num_sites = num_sites
    with open(csv_path) as f:
      for entry in f:
        if remaining_num_sites <= 0:
          break
        remaining_num_sites -= 1
        hostname = entry.strip().split(',')[1]
        if not '/' in hostname:  # Skip Alexa 1,000,000 entries that have paths.
          url = "http://%s/" % hostname
          if not url in bad_urls:
            urls.append(url)
    # Don't write these to disk yet; we'll do that in SaveWorkingUrls below
    # once we have tried to download them and seen which ones fail.
  else:
    with open(urls_path) as f:
      urls = [u for u in f.read().splitlines() if not u in bad_urls]
  return True


def SaveWorkingUrls():
  # TODO(johnme): Update the list if a url that used to work goes offline.
  urls_path = os.path.join(output_dir, "data", "%06d_urls.txt" % num_sites)
  if not os.path.exists(urls_path):
    with open(urls_path, 'w') as f:
      f.writelines(u + '\n' for u in urls)


def PrintElapsedTime(elapsed, detail=""):
  elapsed = round(elapsed * 10) / 10.0
  m = elapsed / 60
  s = elapsed % 60
  print("Took %dm%.1fs" % (m, s), detail)


def DownloadStaticCopyTask(url):
  url_parts = urlparse(url)
  host_dir = os.path.join(output_dir, "data", url_parts.hostname)
  # Use wget for now, as does a reasonable job of spidering page dependencies
  # (e.g. CSS, JS, images).
  success = True
  try:
    subprocess.check_call(["wget",
                           "--execute", "robots=off",
                           ("--user-agent=Mozilla/5.0 (Macintosh; Intel Mac OS "
                            "X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) C"
                            "hrome/32.0.1700.14 Safari/537.36"),
                           "--page-requisites",
                           "--span-hosts",
                           "--adjust-extension",
                           "--convert-links",
                           "--directory-prefix=" + host_dir,
                           "--force-directories",
                           "--default-page=index.html",
                           "--no-check-certificate",
                           "--timeout=5", # 5s timeout
                           "--tries=2",
                           "--quiet",
                           url])
  except KeyboardInterrupt:
    success = False
  except subprocess.CalledProcessError:
    # Ignoring these for now, as some sites have issues with their subresources
    # yet still produce a renderable index.html
    pass #success = False
  if success:
    download_path = os.path.join(host_dir, url_parts.hostname, "index.html")
    if not os.path.exists(download_path):
      success = False
    else:
      with print_lock:
        print("Downloaded:", url)
  if not success:
    with print_lock:
      print("Failed to download:", url)
    return False
  return True


def DownloadStaticCopies():
  global urls
  new_urls = []
  for url in urls:
    url_parts = urlparse(url)
    host_dir = os.path.join(output_dir, "data", url_parts.hostname)
    download_path = os.path.join(host_dir, url_parts.hostname, "index.html")
    if not os.path.exists(download_path):
      new_urls.append(url)

  if new_urls:
    print("Downloading static copies of %d sites..." % len(new_urls))
    start_time = time.time()

    results = multiprocessing.Pool(20).map(DownloadStaticCopyTask, new_urls)
    failed_urls = [new_urls[i] for i,ret in enumerate(results) if not ret]
    if failed_urls:
      bad_urls_path = os.path.join(output_dir, "data", "bad_urls.txt")
      with open(bad_urls_path, 'a') as f:
        f.writelines(u + '\n' for u in failed_urls)
      failed_urls_set = set(failed_urls)
      urls = [u for u in urls if u not in failed_urls_set]

    PrintElapsedTime(time.time() - start_time)

  SaveWorkingUrls()


def RunDrtTask(url):
  url_parts = urlparse(url)
  host_dir = os.path.join(output_dir, "data", url_parts.hostname)
  html_path = os.path.join(host_dir, url_parts.hostname, "index.html")

  if not allow_js:
    nojs_path = os.path.join(host_dir, url_parts.hostname, "index-nojs.html")
    if not os.path.exists(nojs_path):
      with open(html_path) as f:
        html = f.read()
      if not html:
        return False
      # These aren't intended to be XSS safe :)
      block_tags = (r'<\s*(script|object|video|audio|iframe|frameset|frame)'
                    r'\b.*?<\s*\/\s*\1\s*>')
      block_attrs = r'\s(onload|onerror)\s*=\s*(\'[^\']*\'|"[^"]*|\S*)'
      html = re.sub(block_tags, '', html, flags=re.I|re.S)
      html = re.sub(block_attrs, '', html, flags=re.I)
      with open(nojs_path, 'w') as f:
        f.write(html)
    html_path = nojs_path

  start_time = time.time()

  with open(os.devnull, "w") as fnull:
    p = subprocess.Popen([content_shell,
                          "--run-web-tests",
                          additional_content_shell_flags,
                          html_path
                         ],
                         shell=False,
                         stdout=subprocess.PIPE,
                         stderr=fnull)
  result = p.stdout.read()
  PNG_START = b"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"
  PNG_END = b"\x49\x45\x4E\x44\xAE\x42\x60\x82"
  try:
    start = result.index(PNG_START)
    end = result.rindex(PNG_END) + 8
  except ValueError:
    return False

  png_path = os.path.join(output_dir, action, url_parts.hostname + ".png")
  MakeDirsIfNotExist(os.path.dirname(png_path))
  with open(png_path, 'wb') as f:
    f.write(result[start:end])
  elapsed_time = (time.time() - start_time, url)
  return elapsed_time


def RunDrt():
  print("Taking screenshots of %d pages..." % len(urls))
  start_time = time.time()

  results = multiprocessing.Pool().map(RunDrtTask, urls, 1)

  max_time, url = max(t for t in results if t)
  elapsed_detail = "(slowest: %.2fs on %s)" % (max_time, url)
  PrintElapsedTime(time.time() - start_time, elapsed_detail)


def CompareResultsTask(url):
  url_parts = urlparse(url)
  before_path = os.path.join(output_dir, "before", url_parts.hostname + ".png")
  after_path = os.path.join(output_dir, "after", url_parts.hostname + ".png")
  diff_path = os.path.join(output_dir, "diff", url_parts.hostname + ".png")
  MakeDirsIfNotExist(os.path.join(output_dir, "diff"))

  # TODO(johnme): Don't hardcode "real_world_impact".
  red_path = ("data:image/gif;base64,R0lGODlhAQABAPAAAP8AAP///yH5BAAAAAAALAAAAA"
              "ABAAEAAAICRAEAOw==")

  before_exists = os.path.exists(before_path)
  after_exists = os.path.exists(after_path)
  if not before_exists and not after_exists:
    # TODO(johnme): Make this more informative.
    return (-100, url, red_path)
  if before_exists != after_exists:
    # TODO(johnme): Make this more informative.
    return (200, url, red_path)

  # Get percentage difference.
  p = subprocess.Popen([image_diff, "--histogram",
                        before_path, after_path],
                        shell=False,
                        stdout=subprocess.PIPE)
  output,_ = p.communicate()
  if p.returncode == 0:
    return (0, url, before_path)
  diff_match = re.match(r'histogram diff: (\d+\.\d{2})% (?:passed|failed)\n'
                         'exact diff: (\d+\.\d{2})% (?:passed|failed)', output)
  if not diff_match:
    raise Exception("image_diff output format changed")
  histogram_diff = float(diff_match.group(1))
  exact_diff = float(diff_match.group(2))
  combined_diff = max(histogram_diff + exact_diff / 8, 0.001)

  # Produce diff PNG.
  subprocess.call([image_diff, "--diff", before_path, after_path, diff_path])
  return (combined_diff, url, diff_path)


def CompareResults():
  print("Running image_diff on %d pages..." % len(urls))
  start_time = time.time()

  results = multiprocessing.Pool().map(CompareResultsTask, urls)
  results.sort(key=itemgetter(0), reverse=True)

  PrintElapsedTime(time.time() - start_time)

  now = datetime.datetime.today().strftime("%a %Y-%m-%d %H:%M")
  html_start = textwrap.dedent("""\
  <!DOCTYPE html>
  <html>
  <head>
  <title>Real World Impact report %s</title>
  <script>
    var togglingImg = null;
    var toggleTimer = null;

    var before = true;
    function toggle() {
      var newFolder = before ? "before" : "after";
      togglingImg.src = togglingImg.src.replace(/before|after|diff/, newFolder);
      before = !before;
      toggleTimer = setTimeout(toggle, 300);
    }

    function startToggle(img) {
      before = true;
      togglingImg = img;
      if (!img.origSrc)
        img.origSrc = img.src;
      toggle();
    }
    function stopToggle(img) {
      clearTimeout(toggleTimer);
      img.src = img.origSrc;
    }

    document.onkeydown = function(e) {
      e = e || window.event;
      var keyCode = e.keyCode || e.which;
      var newFolder;
      switch (keyCode) {
        case 49: //'1'
          newFolder = "before"; break;
        case 50: //'2'
          newFolder = "after"; break;
        case 51: //'3'
          newFolder = "diff"; break;
        default:
          return;
      }
      var imgs = document.getElementsByTagName("img");
      for (var i = 0; i < imgs.length; i++) {
        imgs[i].src = imgs[i].src.replace(/before|after|diff/, newFolder);
      }
    };
  </script>
  <style>
    h1 {
      font-family: sans;
    }
    h2 {
      font-family: monospace;
      white-space: pre;
    }
    .nsfw-spacer {
      height: 50vh;
    }
    .nsfw-warning {
      background: yellow;
      border: 10px solid red;
    }
    .info {
      font-size: 1.2em;
      font-style: italic;
    }
    body:not(.details-supported) details {
      display: none;
    }
  </style>
  </head>
  <body>
    <script>
    if ('open' in document.createElement('details'))
      document.body.className = "details-supported";
    </script>
    <!--<div class="nsfw-spacer"></div>-->
    <p class="nsfw-warning">Warning: sites below are taken from the Alexa top %d
    and may be NSFW.</p>
    <!--<div class="nsfw-spacer"></div>-->
    <h1>Real World Impact report %s</h1>
    <p class="info">Press 1, 2 and 3 to switch between before, after and diff
    screenshots respectively; or hover over the images to rapidly alternate
    between before and after.</p>
  """ % (now, num_sites, now))

  html_same_row = """\
  <h2>No difference on <a href="%s">%s</a>.</h2>
  """

  html_diff_row = """\
  <h2>%7.3f%% difference on <a href="%s">%s</a>:</h2>
  <img src="%s" width="800" height="600"
       onmouseover="startToggle(this)" onmouseout="stopToggle(this)">
  """

  html_nsfw_diff_row = """\
  <h2>%7.3f%% difference on <a href="%s">%s</a>:</h2>
  <details>
    <summary>This site may be NSFW. Click to expand/collapse.</summary>
    <img src="%s" width="800" height="600"
         onmouseover="startToggle(this)" onmouseout="stopToggle(this)">
  </details>
  """

  html_end = textwrap.dedent("""\
  </body>
  </html>""")

  html_path = os.path.join(output_dir, "diff.html")
  with open(html_path, 'w') as f:
    f.write(html_start)
    for (diff_float, url, diff_path) in results:
      diff_path = os.path.relpath(diff_path, output_dir)
      if diff_float == 0:
        f.write(html_same_row % (url, url))
      elif url in nsfw_urls:
        f.write(html_nsfw_diff_row % (diff_float, url, url, diff_path))
      else:
        f.write(html_diff_row % (diff_float, url, url, diff_path))
    f.write(html_end)

  webbrowser.open_new_tab("file://" + html_path)


def main(argv):
  global num_sites, action, allow_js, additional_content_shell_flags

  parser = argparse.ArgumentParser(
      formatter_class=RawTextHelpFormatter,
      description="Compare the real world impact of a content shell change.",
      epilog=textwrap.dedent("""\
          Example usage:
            1. Build content_shell in out/Release without any changes.
            2. Run: %s before [num sites to test (default %d)].
            3. Either:
                 a. Apply your controversial patch and rebuild content_shell.
                 b. Pass --additional_flags="--enable_your_flag" in step 4.
            4. Run: %s after [num sites to test (default %d)].
            5. Run: %s compare [num sites to test (default %d)].
               This will open the results in your web browser.
          """ % (argv[0], num_sites, argv[0], num_sites, argv[0], num_sites)))
  parser.add_argument("--allow_js", help="Don't disable Javascript",
                      action="store_true")
  parser.add_argument("--additional_flags",
                      help="Additional flags to pass to content shell")
  parser.add_argument("action",
                      help=textwrap.dedent("""\
                        Action to perform.
                          download - Just download the sites.
                          before - Run content shell and record 'before' result.
                          after - Run content shell and record 'after' result.
                          compare - Compare before and after results.
                      """),
                      choices=["download", "before", "after", "compare"])
  parser.add_argument("num_sites",
                      help="Number of sites (default %s)" % num_sites,
                      type=int, default=num_sites, nargs='?')
  args = parser.parse_args()

  action = args.action

  if (args.num_sites):
    num_sites = args.num_sites

  if (args.allow_js):
    allow_js = args.allow_js

  if (args.additional_flags):
    additional_content_shell_flags = args.additional_flags

  if not SetupPathsAndOut() or not CheckPrerequisites() or not PickSampleUrls():
    return 1

  if action == 'compare':
    CompareResults()
  else:
    DownloadStaticCopies()
    if action != 'download':
      RunDrt()
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
