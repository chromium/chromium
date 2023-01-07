# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test server for generating nested iframes with different sites.

Very simple python server for creating a bunch of iframes. The page generation
is randomized based on query parameters.  See the __init__ function of the
Params class for a description of the parameters.

This server relies on gevent. On Ubuntu, install it via:

  sudo apt-get install python-gevent

Run the server using

  python iframe_server.py

To use the server, run chrome as follows:

  google-chrome --host-resolver-rules='map *.invalid 127.0.0.1'

Change 127.0.0.1 to be the IP of the machine this server is running on. Then
in this chrome instance, navigate to any domain in .invalid
(eg., http://1.invalid:8090) to run this test.

"""

import colorsys
import copy
import random
import urllib
import urlparse

from gevent import pywsgi # pylint: disable=F0401

MAIN_PAGE = """
<html>
  <head>
    <style>
      body {
        background-color: %(color)s;
      }
    </style>
  </head>
  <body>
    <center>
      <h1><a href="%(url)s">%(site)s</a></h1>
      <p><small>%(url)s</small>
    </center>
    <br />
    %(iframe_html)s
  </body>
</html>
"""

IFRAME_FRAGMENT = """
<iframe src="%(src)s" width="%(width)s" height="%(height)s">
</iframe>
"""

class Params(object):
  """Simple object for holding parameters"""
  def __init__(self, query_dict):
    # Basic params:
    #  nframes is how many frames per page.
    #  nsites is how many sites to random choose out of.
    #  depth is how deep to make the frame tree
    #  pattern specifies how the sites are layed out per depth. An empty string
    #      uses a random N = [0, nsites] each time to generate a N.invalid URL.
    #      Otherwise sepcify with single letters like 'ABCA' and frame
    #      A.invalid will embed B.invalid will embed C.invalid will embed A.
    #  jitter is the amount of randomness applied to nframes and nsites.
    #      Should be from [0,1]. 0.0 means no jitter.
    #  size_jitter is like jitter, but for width and height.
    self.nframes = int(query_dict.get('nframes', [4] )[0])
    self.nsites = int(query_dict.get('nsites', [10] )[0])
    self.depth = int(query_dict.get('depth', [1] )[0])
    self.jitter = float(query_dict.get('jitter', [0] )[0])
    self.size_jitter = float(query_dict.get('size_jitter', [0.5] )[0])
    self.pattern = query_dict.get('pattern', [''] )[0]
    self.pattern_pos = int(query_dict.get('pattern_pos', [0] )[0])

    # Size parameters. Values are percentages.
    self.width = int(query_dict.get('width', [60])[0])
    self.height = int(query_dict.get('height', [50])[0])

    # Pass the random seed so our pages are reproduceable.
    self.seed = int(query_dict.get('seed',
                                   [random.randint(0, 2147483647)])[0])


def get_site(urlpath):
  """Takes a urlparse object and finds its approximate site.

  Site is defined as registered domain name + scheme. We approximate
  registered domain name by preserving the last 2 elements of the DNS
  name. This breaks for domains like co.uk.
  """
  no_port = urlpath.netloc.split(':')[0]
  host_parts = no_port.split('.')
  site_host = '.'.join(host_parts[-2:])
  return '%s://%s' % (urlpath.scheme, site_host)


def generate_host(rand, params):
  """Generates the host to be used as an iframes source.

  Uses the .invalid domain to ensure DNS will not resolve to any real
  address.
  """
  if params.pattern:
    host = params.pattern[params.pattern_pos]
    params.pattern_pos = (params.pattern_pos + 1) % len(params.pattern)
  else:
    host = rand.randint(1, apply_jitter(rand, params.jitter, params.nsites))
  return '%s.invalid' % host


def apply_jitter(rand, jitter, n):
  """Reduce n by random amount from [0, jitter]. Ensures result is >=1."""
  if jitter <= 0.001:
    return n
  v = n - int(n * rand.uniform(0, jitter))
  if v:
    return v
  else:
    return 1


def get_color_for_site(site):
  """Generate a stable (and pretty-ish) color for a site."""
  val = hash(site)
  # The constants below are arbitrary chosen emperically to look "pretty."
  # HSV is used because it is easier to control the color than RGB.
  # Reducing the H to 0.6 produces a good range of colors. Preserving
  # > 0.5 saturation and value means the colors won't be too washed out.
  h = (val % 100)/100.0 * 0.6
  s = 1.0 - (int(val/100) % 100)/200.
  v = 1.0 - (int(val/10000) % 100)/200.0
  (r, g, b) = colorsys.hsv_to_rgb(h, s, v)
  return 'rgb(%d, %d, %d)' % (int(r * 255), int(g * 255), int(b * 255))


def make_src(scheme, netloc, path, params):
  """Constructs the src url that will recreate the given params."""
  if path == '/':
    path = ''
  return '%(scheme)s://%(netloc)s%(path)s?%(params)s' % {
      'scheme': scheme,
      'netloc': netloc,
      'path': path,
      'params': urllib.urlencode(params.__dict__),
      }


def make_iframe_html(urlpath, params):
  """Produces the HTML fragment for the iframe."""
  if (params.depth <= 0):
    return ''
  # Ensure a stable random number per iframe.
  rand = random.Random()
  rand.seed(params.seed)

  netloc_paths = urlpath.netloc.split(':')
  netloc_paths[0] = generate_host(rand, params)

  width = apply_jitter(rand, params.size_jitter, params.width)
  height = apply_jitter(rand, params.size_jitter, params.height)
  iframe_params = {
      'src': make_src(urlpath.scheme, ':'.join(netloc_paths),
                      urlpath.path, params),
      'width': '%d%%' % width,
      'height': '%d%%' % height,
      }
  return IFRAME_FRAGMENT % iframe_params


def create_html(environ):
  """Creates the current HTML page. Also parses out query parameters."""
  urlpath = urlparse.urlparse('%s://%s%s?%s' % (
      environ['wsgi.url_scheme'],
      environ['HTTP_HOST'],
      environ['PATH_INFO'],
      environ['QUERY_STRING']))
  site = get_site(urlpath)
  params = Params(urlparse.parse_qs(urlpath.query))

  rand = random.Random()
  rand.seed(params.seed)

  iframe_htmls = []
  for frame in xrange(0, apply_jitter(rand, params.jitter, params.nframes)):
    # Copy current parameters into iframe and make modifications
    # for the recursive generation.
    iframe_params = copy.copy(params)
    iframe_params.depth = params.depth - 1
    # Base the new seed off the current seed, but have it skip enough that
    # different frame trees are unlikely to collide. Numbers and skips
    # not chosen in any scientific manner at all.
    iframe_params.seed = params.seed + (frame + 1) * (
        1000000 + params.depth + 333)
    iframe_htmls.append(make_iframe_html(urlpath, iframe_params))
  template_params = dict(params.__dict__)
  template_params.update({
      'color': get_color_for_site(site),
      'iframe_html': '\n'.join(iframe_htmls),
      'site': site,
      'url': make_src(urlpath.scheme, urlpath.netloc, urlpath.path, params),
      })
  return MAIN_PAGE % template_params


def application(environ, start_response):
  start_response('200 OK', [('Content-Type', 'text/html')])
  if environ['PATH_INFO'] == '/favicon.ico':
    yield ''
  else:
    yield create_html(environ)


server = pywsgi.WSGIServer(('', 8090), application)

server.serve_forever()
