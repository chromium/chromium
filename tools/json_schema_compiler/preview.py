#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Server for viewing the compiled C++ code from tools/json_schema_compiler.
"""

import cc_generator
import code_util
import cpp_type_generator
import cpp_util
import h_generator
import idl_schema
import json_schema
import model
import optparse
import os
import shlex
import urlparse
from highlighters import (pygments_highlighter, none_highlighter,
                          hilite_me_highlighter)
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
from cpp_namespace_environment import CppNamespaceEnvironment
from namespace_resolver import NamespaceResolver


class CompilerHandler(BaseHTTPRequestHandler):
  """A HTTPRequestHandler that outputs the result of tools/json_schema_compiler.
  """

  def do_GET(self):
    parsed_url = urlparse.urlparse(self.path)
    request_path = self._GetRequestPath(parsed_url)

    chromium_favicon = 'http://codereview.chromium.org/static/favicon.ico'

    head = code_util.Code()
    head.Append('<link rel="icon" href="%s">' % chromium_favicon)
    head.Append('<link rel="shortcut icon" href="%s">' % chromium_favicon)

    body = code_util.Code()

    try:
      if os.path.isdir(request_path):
        self._ShowPanels(parsed_url, head, body)
      else:
        self._ShowCompiledFile(parsed_url, head, body)
    finally:
      self.wfile.write('<html><head>')
      self.wfile.write(head.Render())
      self.wfile.write('</head><body>')
      self.wfile.write(body.Render())
      self.wfile.write('</body></html>')

  def _GetRequestPath(self, parsed_url, strip_nav=False):
    """Get the relative path from the current directory to the requested file.
    """
    path = parsed_url.path
    if strip_nav:
      path = parsed_url.path.replace('/nav', '')
    return os.path.normpath(os.curdir + path)

  def _ShowPanels(self, parsed_url, head, body):
    """Show the previewer frame structure.

    Code panes are populated via XHR after links in the nav pane are clicked.
    """
    (head.Append('<style>') \
         .Append('body {') \
         .Append('  margin: 0;') \
         .Append('}') \
         .Append('.pane {') \
         .Append('  height: 100%;') \
         .Append('  overflow-x: auto;') \
         .Append('  overflow-y: scroll;') \
         .Append('  display: inline-block;') \
         .Append('}') \
         .Append('#nav_pane {') \
         .Append('  width: 20%;') \
         .Append('}') \
         .Append('#nav_pane ul {') \
         .Append('  list-style-type: none;') \
         .Append('  padding: 0 0 0 1em;') \
         .Append('}') \
         .Append('#cc_pane {') \
         .Append('  width: 40%;') \
         .Append('}') \
         .Append('#h_pane {') \
         .Append('  width: 40%;') \
         .Append('}') \
         .Append('</style>')
    )

    body.Append('<div class="pane" id="nav_pane">%s</div>'
                '<div class="pane" id="h_pane"></div>'
                '<div class="pane" id="cc_pane"></div>' %
                self._RenderNavPane(parsed_url.path[1:]))

    # The Javascript that interacts with the nav pane and panes to show the
    # compiled files as the URL or highlighting options change.
    body.Append('''<script type="text/javascript">
// Calls a function for each highlighter style <select> element.
function forEachHighlighterStyle(callback) {
  var highlighterStyles =
      document.getElementsByClassName('highlighter_styles');
  for (var i = 0; i < highlighterStyles.length; ++i)
    callback(highlighterStyles[i]);
}

// Called when anything changes, such as the highlighter or hashtag.
function updateEverything() {
  var highlighters = document.getElementById('highlighters');
  var highlighterName = highlighters.value;

  // Cache in localStorage for when the page loads next.
  localStorage.highlightersValue = highlighterName;

  // Show/hide the highlighter styles.
  var highlighterStyleName = '';
  forEachHighlighterStyle(function(highlighterStyle) {
    if (highlighterStyle.id === highlighterName + '_styles') {
      highlighterStyle.removeAttribute('style')
      highlighterStyleName = highlighterStyle.value;
    } else {
      highlighterStyle.setAttribute('style', 'display:none')
    }

    // Cache in localStorage for when the page next loads.
    localStorage[highlighterStyle.id + 'Value'] = highlighterStyle.value;
  });

  // Populate the code panes.
  function populateViaXHR(elementId, requestPath) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState != 4)
        return;
      if (xhr.status != 200) {
        alert('XHR error to ' + requestPath);
        return;
      }
      document.getElementById(elementId).innerHTML = xhr.responseText;
    };
    xhr.open('GET', requestPath, true);
    xhr.send();
  }

  var targetName = window.location.hash;
  targetName = targetName.substring('#'.length);
  targetName = targetName.split('.', 1)[0]

  if (targetName !== '') {
    var basePath = window.location.pathname;
    var query = 'highlighter=' + highlighterName + '&' +
                'style=' + highlighterStyleName;
    populateViaXHR('h_pane',  basePath + '/' + targetName + '.h?'  + query);
    populateViaXHR('cc_pane', basePath + '/' + targetName + '.cc?' + query);
  }
}

// Initial load: set the values of highlighter and highlighterStyles from
// localStorage.
(function() {
var cachedValue = localStorage.highlightersValue;
if (cachedValue)
  document.getElementById('highlighters').value = cachedValue;

forEachHighlighterStyle(function(highlighterStyle) {
  var cachedValue = localStorage[highlighterStyle.id + 'Value'];
  if (cachedValue)
    highlighterStyle.value = cachedValue;
});
})();

window.addEventListener('hashchange', updateEverything, false);
updateEverything();
</script>''')

  def _ShowCompiledFile(self, parsed_url, head, body):
    """Show the compiled version of a json or idl file given the path to the
    compiled file.
    """
    api_model = model.Model()

    request_path = self._GetRequestPath(parsed_url)
    (file_root, file_ext) = os.path.splitext(request_path)
    (filedir, filename) = os.path.split(file_root)

    namespace_resolver = NamespaceResolver("./", filedir,
                                           self.server.include_rules,
                                           self.server.cpp_namespace_pattern)
    try:
      # Get main file.
      namespace = namespace_resolver.ResolveNamespace(filename)
      type_generator = cpp_type_generator.CppTypeGenerator(
          api_model, namespace_resolver, namespace)

      # Generate code
      if file_ext == '.h':
        cpp_code = (
            h_generator.HGenerator(type_generator).Generate(namespace).Render())
      elif file_ext == '.cc':
        cpp_code = (cc_generator.CCGenerator(type_generator).Generate(
            namespace).Render())
      else:
        self.send_error(404, "File not found: %s" % request_path)
        return

      # Do highlighting on the generated code
      (highlighter_param, style_param) = self._GetHighlighterParams(parsed_url)
      head.Append(
          '<style>' +
          self.server.highlighters[highlighter_param].GetCSS(style_param) +
          '</style>')
      body.Append(self.server.highlighters[highlighter_param].GetCodeElement(
          cpp_code, style_param))
    except IOError:
      self.send_error(404, "File not found: %s" % request_path)
      return
    except (TypeError, KeyError, AttributeError, AssertionError,
            NotImplementedError) as error:
      body.Append('<pre>')
      body.Append('compiler error: %s' % error)
      body.Append('Check server log for more details')
      body.Append('</pre>')
      raise

  def _GetHighlighterParams(self, parsed_url):
    """Get the highlighting parameters from a parsed url.
    """
    query_dict = urlparse.parse_qs(parsed_url.query)
    return (query_dict.get('highlighter', ['pygments'])[0],
            query_dict.get('style', ['colorful'])[0])

  def _RenderNavPane(self, path):
    """Renders an HTML nav pane.

    This consists of a select element to set highlight style, and a list of all
    files at |path| with the appropriate onclick handlers to open either
    subdirectories or JSON files.
    """
    html = code_util.Code()

    # Highlighter chooser.
    html.Append('<select id="highlighters" onChange="updateEverything()">')
    for name, highlighter in self.server.highlighters.items():
      html.Append('<option value="%s">%s</option>' %
                  (name, highlighter.DisplayName()))
    html.Append('</select>')

    html.Append('<br/>')

    # Style for each highlighter.
    # The correct highlighting will be shown by Javascript.
    for name, highlighter in self.server.highlighters.items():
      styles = sorted(highlighter.GetStyles())
      if not styles:
        continue

      html.Append('<select class="highlighter_styles" id="%s_styles" '
                  'onChange="updateEverything()">' % name)
      for style in styles:
        html.Append('<option>%s</option>' % style)
      html.Append('</select>')

    html.Append('<br/>')

    # The files, with appropriate handlers.
    html.Append('<ul>')

    # Make path point to a non-empty directory. This can happen if a URL like
    # http://localhost:8000 is navigated to.
    if path == '':
      path = os.curdir

    # Firstly, a .. link if this isn't the root.
    if not os.path.samefile(os.curdir, path):
      normpath = os.path.normpath(os.path.join(path, os.pardir))
      html.Append('<li><a href="/%s">%s/</a>' % (normpath, os.pardir))

    # Each file under path/
    for filename in sorted(os.listdir(path)):
      full_path = os.path.join(path, filename)
      _, file_ext = os.path.splitext(full_path)
      if os.path.isdir(full_path) and not full_path.endswith('.xcodeproj'):
        html.Append('<li><a href="/%s/">%s/</a>' % (full_path, filename))
      elif file_ext in ['.json', '.idl']:
        # cc/h panes will automatically update via the hash change event.
        html.Append('<li><a href="#%s">%s</a>' % (filename, filename))

    html.Append('</ul>')

    return html.Render()


class PreviewHTTPServer(HTTPServer, object):

  def __init__(self, server_address, handler, highlighters, include_rules,
               cpp_namespace_pattern):
    super(PreviewHTTPServer, self).__init__(server_address, handler)
    self.highlighters = highlighters
    self.include_rules = include_rules
    self.cpp_namespace_pattern = cpp_namespace_pattern


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Runs a server to preview the json_schema_compiler output.',
      usage='usage: %prog [option]...')
  parser.add_option('-p',
                    '--port',
                    default='8000',
                    help='port to run the server on')
  parser.add_option(
      '-n',
      '--namespace',
      default='generated_api_schemas',
      help='C++ namespace for generated files. e.g extensions::api.')
  parser.add_option(
      '-I',
      '--include-rules',
      help='A list of paths to include when searching for referenced objects,'
      ' with the namespace separated by a \':\'. Example: '
      '/foo/bar:Foo::Bar::%(namespace)s')

  (opts, argv) = parser.parse_args()

  def split_path_and_namespace(path_and_namespace):
    if ':' not in path_and_namespace:
      raise ValueError('Invalid include rule "%s". Rules must be of '
                       'the form path:namespace' % path_and_namespace)
    return path_and_namespace.split(':', 1)

  include_rules = []
  if opts.include_rules:
    include_rules = map(split_path_and_namespace,
                        shlex.split(opts.include_rules))

  try:
    print('Starting previewserver on port %s' % opts.port)
    print('The extension documentation can be found at:')
    print('')
    print('  http://localhost:%s/chrome/common/extensions/api' % opts.port)
    print('')

    highlighters = {
        'hilite': hilite_me_highlighter.HiliteMeHighlighter(),
        'none': none_highlighter.NoneHighlighter()
    }
    try:
      highlighters['pygments'] = pygments_highlighter.PygmentsHighlighter()
    except ImportError as e:
      pass

    server = PreviewHTTPServer(('', int(opts.port)), CompilerHandler,
                               highlighters, include_rules, opts.namespace)
    server.serve_forever()
  except KeyboardInterrupt:
    server.socket.close()
