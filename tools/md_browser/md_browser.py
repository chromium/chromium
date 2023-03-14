#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple Markdown browser for a Git checkout."""

import http.server
import socketserver
import argparse
import codecs
import html
import os
import re
import socket
import sys
import threading
import time
import urllib.request, urllib.parse, urllib.error
import webbrowser
from xml.etree import ElementTree

import markdown

THIS_DIR = os.path.realpath(os.path.dirname(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(THIS_DIR))

def main(argv):
  parser = argparse.ArgumentParser(prog='md_browser')
  parser.add_argument('-p', '--port', type=int, default=8080,
                      help='port to run on (default = %(default)s)')
  parser.add_argument('-d', '--directory', type=str, default=SRC_DIR)
  parser.add_argument('-e', '--external', action='store_true',
                      help='whether to bind to external port')
  parser.add_argument('file', nargs='?',
                      help='open file in browser')
  args = parser.parse_args(argv)

  top_level = os.path.realpath(args.directory)
  hostname = '0.0.0.0' if args.external else 'localhost'
  server_address = (hostname, args.port)
  s = Server(server_address, top_level)

  origin = 'http://' + hostname
  if args.port != 80:
    origin += ':%s' % args.port
  print('Listening on %s/' % origin)

  thread = None
  if args.file:
    path = os.path.realpath(args.file)
    if not path.startswith(top_level):
      print('%s is not under %s' % (args.file, args.directory))
      return 1
    rpath = os.path.relpath(path, top_level)
    url = '%s/%s' % (origin, rpath)
    print('Opening %s' % url)
    thread = threading.Thread(target=_open_url, args=(url,))
    thread.start()

  elif os.path.isfile(os.path.join(top_level, 'docs', 'README.md')):
    print(' Try loading %s/docs/README.md' % origin)
  elif os.path.isfile(os.path.join(args.directory, 'README.md')):
    print(' Try loading %s/README.md' % origin)

  retcode = 1
  try:
    s.serve_forever()
  except KeyboardInterrupt:
    retcode = 130
  except Exception as e:
    print('Exception raised: %s' % str(e))

  s.shutdown()
  if thread:
    thread.join()
  return retcode


def _open_url(url):
  time.sleep(1)
  webbrowser.open(url)


def _gitiles_slugify(value, _separator):
  """Convert a string (representing a section title) to URL anchor name.

  This function is passed to "toc" extension as an extension option, so we
  can emulate the way how Gitiles converts header titles to URL anchors.

  Gitiles' official documentation about the conversion is at:

  https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md#Named-anchors

  Args:
    value: The name of a section that is to be converted.
    _separator: Unused. This is actually a configurable string that is used
        as a replacement character for spaces in the title, typically set to
        '-'. Since we emulate Gitiles' way of slugification here, it makes
        little sense to have the separator charactor configurable.
  """

  # TODO(yutak): Implement accent removal. This does not seem easy without
  # some library. For now we just make accented characters turn into
  # underscores, just like other non-ASCII characters.

  def decode_escaped_chars(regex_match):
    # Python-Markdown encodes escaped sequences (ex. "\_") as "\x02 (integer
    # ascii code) \x03". We decode the integer ascii code to align with Gitiles
    # behavior (ex. 95 -> '_').
    return chr(int(regex_match.group(1)))

  # Non-ASCII turns into '?'.
  value = value.encode('ascii', 'replace').decode('ascii')
  value = re.sub('\x02(\\d+)\x03', decode_escaped_chars, value)
  value = re.sub(r'[^- a-zA-Z0-9]', '_', value)  # Non-alphanumerics to '_'.
  value = value.replace(' ', '-')
  value = re.sub(r'([-_])[-_]+', r'\1', value)  # Fold hyphens and underscores.
  return value


class Server(socketserver.TCPServer):
  def __init__(self, server_address, top_level):
    socketserver.TCPServer.__init__(self, server_address, Handler)
    self.top_level = top_level

  def server_bind(self):
    self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.socket.bind(self.server_address)


class Handler(http.server.SimpleHTTPRequestHandler):
  def do_GET(self):
    self.path = urllib.parse.unquote(self.path)
    path = self.path

    # strip off the repo and branch info, if present, for compatibility
    # with gitiles.
    if path.startswith('/chromium/src/+/master'):
      path = path[len('/chromium/src/+/master'):]

    full_path = os.path.normpath(os.path.join(self.server.top_level, path[1:]))

    if not full_path.startswith(self.server.top_level):
      self._DoUnknown()
    elif path in ('/base.css', '/doc.css', '/prettify.css'):
      self._DoCSS(path[1:])
    elif not os.path.exists(full_path):
      self._DoNotFound()
    elif path.lower().endswith('.md'):
      self._DoMD(path)
    elif os.path.exists(full_path + '/README.md'):
      separator = '/'
      if path.endswith('/'):
        separator = ''
      self._DoMD(path + separator + 'README.md')
    elif path.lower().endswith('.png'):
      self._DoImage(full_path, 'image/png')
    elif path.lower().endswith('.jpg'):
      self._DoImage(full_path, 'image/jpeg')
    elif path.lower().endswith('.svg'):
      self._DoImage(full_path, 'image/svg+xml')
    elif os.path.isdir(full_path):
      self._DoDirListing(full_path)
    elif os.path.exists(full_path):
      self._DoRawSourceFile(full_path)
    else:
      self._DoUnknown()

  def _DoMD(self, path):
    extensions = [
        'markdown.extensions.def_list',
        'markdown.extensions.fenced_code',
        'markdown.extensions.tables',
        'markdown.extensions.toc',
        'gitiles_autolink',
        'gitiles_ext_blocks',
        'gitiles_smart_quotes',
    ]
    extension_configs = {
        'markdown.extensions.toc': {
            'slugify': _gitiles_slugify
        },
    }

    contents = self._Read(path[1:])

    md = markdown.Markdown(extensions=extensions,
                           extension_configs=extension_configs,
                           tab_length=4,
                           output_format='html4')

    has_a_single_h1 = (len([line for line in contents.splitlines()
                            if (line.startswith('#') and
                                not line.startswith('##'))]) == 1)

    md.treeprocessors.register(_AdjustTOC(has_a_single_h1), 'adjust_toc', 4)

    md_fragment = md.convert(contents)

    try:
      self._WriteHeader('text/html')
      self._WriteTemplate('header.html')
      self._Write('<div class="doc">')
      self._Write(md_fragment)
      self._Write('</div>')
      self._WriteTemplate('footer.html')
    except:
      raise

  def _DoRawSourceFile(self, full_path):
    self._WriteHeader('text/html')
    self._WriteTemplate('header.html')

    self._Write('<table class="FileContents">')
    with open(full_path) as fp:
      # Escape html over the entire file at once.
      data = fp.read().replace(
          '&', '&amp;').replace(
          '<', '&lt;').replace(
          '>', '&gt;').replace(
          '"', '&quot;')
      for i, line in enumerate(data.splitlines(), start=1):
        self._Write(
            ('<tr class="u-pre u-monospace FileContents-line">'
             '<td class="u-lineNum u-noSelect FileContents-lineNum">'
             '<a name="%(num)s" '
             'onclick="window.location.hash=%(quot)s#%(num)s%(quot)s">'
             '%(num)s</a></td>'
             '<td class="FileContents-lineContents">%(line)s</td></tr>') % {
                 'num': i,
                 'quot': "'",
                 'line': line
             })
    self._Write('</table>')

    self._WriteTemplate('footer.html')

  def _DoCSS(self, template):
    self._WriteHeader('text/css')
    self._WriteTemplate(template)

  def _DoNotFound(self):
    self._WriteHeader('text/html', status_code=404)
    self._Write('<html><body>%s not found</body></html>' %
                html.escape(self.path))

  def _DoUnknown(self):
    self._WriteHeader('text/html', status_code=501)
    self._Write('<html><body>I do not know how to serve %s.</body>'
                '</html>' % html.escape(self.path))

  def _DoDirListing(self, full_path):
    self._WriteHeader('text/html')
    self._WriteTemplate('header.html')
    self._Write('<div class="doc">')

    self._Write('<div class="Breadcrumbs">\n')
    self._Write('<a class="Breadcrumbs-crumb">%s</a>\n' %
                html.escape(self.path))
    self._Write('</div>\n')

    escaped_dir = html.escape(self.path.rstrip('/'), quote=True)

    for _, dirs, files in os.walk(full_path):
      for f in sorted(files):
        if f.startswith('.'):
          continue
        f = html.escape(f, quote=True)
        if f.endswith('.md'):
          bold = ('<b>', '</b>')
        else:
          bold = ('', '')
        self._Write('<a href="%s/%s">%s%s%s</a><br/>\n' %
                    (escaped_dir, f, bold[0], f, bold[1]))

      self._Write('<br/>\n')

      for d in sorted(dirs):
        if d.startswith('.'):
          continue
        d = html.escape(d, quote=True)
        self._Write('<a href="%s/%s">%s/</a><br/>\n' % (escaped_dir, d, d))

      break

    self._Write('</div>')
    self._WriteTemplate('footer.html')

  def _DoImage(self, full_path, mime_type):
    self._WriteHeader(mime_type)
    with open(full_path, 'rb') as f:
      self.wfile.write(f.read())

  def _Read(self, relpath, relative_to=None):
    if relative_to is None:
      relative_to = self.server.top_level
    assert not relpath.startswith(os.sep)
    path = os.path.join(relative_to, relpath)
    with codecs.open(path, encoding='utf-8') as fp:
      return fp.read()

  def _Write(self, contents):
    self.wfile.write(contents.encode('utf-8'))

  def _WriteHeader(self, content_type='text/plain', status_code=200):
    self.send_response(status_code)
    self.send_header('Content-Type', content_type)
    self.end_headers()

  def _WriteTemplate(self, template):
    contents = self._Read(os.path.join('tools', 'md_browser', template),
                          relative_to=SRC_DIR)
    self._Write(contents)


class _AdjustTOC(markdown.treeprocessors.Treeprocessor):
  def __init__(self, has_a_single_h1):
    super(_AdjustTOC, self).__init__()
    self.has_a_single_h1 = has_a_single_h1

  def run(self, tree):
    # Given
    #
    #     # H1
    #
    #     [TOC]
    #
    #     ## first H2
    #
    #     ## second H2
    #
    # the markdown.extensions.toc extension generates:
    #
    #     <div class='toc'>
    #       <ul><li><a>H1</a>
    #               <ul><li>first H2
    #                   <li>second H2</li></ul></li><ul></div>
    #
    # for [TOC]. But, we want the TOC to have its own subheading, so
    # we rewrite <div class='toc'><ul>...</ul></div> to:
    #
    #     <div class='toc'>
    #        <h2>Contents</h2>
    #        <div class='toc-aux'>
    #          <ul>...</ul></div></div>
    #
    # In addition, if the document only has a single H1, it is usually the
    # title, and we don't want the title to be in the TOC. So, we remove it
    # and shift all of the title's children up a level, leaving:
    #
    #     <div class='toc'>
    #       <h2>Contents</h2>
    #       <div class='toc-aux'>
    #       <ul><li>first H2
    #           <li>second H2</li></ul></div></div>

    for toc_node in tree.findall(".//*[@class='toc']"):
      toc_ul = toc_node[0]
      if self.has_a_single_h1:
        toc_ul_li = toc_ul[0]
        ul_with_the_desired_toc_entries = toc_ul_li[1]
      else:
        ul_with_the_desired_toc_entries = toc_ul

      toc_node.remove(toc_ul)
      contents = ElementTree.SubElement(toc_node, 'h2')
      contents.text = 'Contents'
      contents.tail = '\n'
      toc_aux = ElementTree.SubElement(toc_node, 'div', {'class': 'toc-aux'})
      toc_aux.text = '\n'
      toc_aux.append(ul_with_the_desired_toc_entries)
      toc_aux.tail = '\n'


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
