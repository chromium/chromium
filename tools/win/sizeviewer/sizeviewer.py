# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import base64
import codecs
import json
import os
import string
import subprocess
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def Run(*args):
    p = subprocess.Popen(args,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    out, err = p.communicate()
    if p.returncode != 0:
        raise SystemExit(out)


def FindNode(node, component):
    for child in node['children']:
        if child['name'] == component:
            return child
    return None


def InsertIntoTree(tree, source_name, size):
    components = source_name[3:].split('\\')
    node = tree
    for index, component in enumerate(components):
        data = FindNode(node, component)
        if not data:
            data = {'name': source_name, 'name': component}
            if index == len(components) - 1:
                data['size'] = size
            else:
                data['children'] = []
            node['children'].append(data)
        node = data


def FlattenTree(tree):
    result = [['Path', 'Parent', 'Size', 'Value']]

    def Flatten(node, parent):
        name = node['name']
        if parent and parent != '/':
            name = parent + '/' + name
        if 'children' in node:
            result.append([name, parent, -1, -1])
            for c in node['children']:
                Flatten(c, name)
        else:
            result.append([name, parent, node['size'], node['size']])

    Flatten(tree, '')
    return result


def GetAsset(filename):
    with open(os.path.join(BASE_DIR, filename), 'rb') as f:
        return f.read()


def AppendAsScriptBlock(f, value, var=None):
    f.write('<script type="text/javascript">\n')
    if var:
        f.write('var ' + var + ' = ')
    f.write(value)
    if var:
        f.write(';\n')
    f.write('</script>\n')


def main():
    jsons = []
    if len(sys.argv) > 1:
        dlls = sys.argv[1:]
    else:
        out_dir = os.path.join(BASE_DIR, '..', '..', '..', 'out', 'Release')
        dlls = [
            os.path.normpath(os.path.join(out_dir, dll))
            for dll in ('chrome.dll', 'chrome_child.dll')
        ]
    for dll_path in dlls:
        if os.path.exists(dll_path):
            print('Tallying %s...' % dll_path)
            json_path = dll_path + '.json'
            Run(
                os.path.join(BASE_DIR, '..', '..', '..', 'third_party',
                             'syzygy', 'binaries', 'exe', 'experimental',
                             'code_tally.exe'), '--input-image=' + dll_path,
                '--input-pdb=' + dll_path + '.pdb',
                '--output-file=' + json_path)
            jsons.append(json_path)
    if not jsons:
        print('Couldn\'t find dlls.')
        print('Pass fully qualified dll name(s) if you want to use something ')
        print('other than out\\Release\\chrome.dll and chrome_child.dll.')
        return 1

    # Munge the code_tally json format into an easier-to-view format.
    for json_name in jsons:
        with open(json_name, 'r') as jsonf:
            all_data = json.load(jsonf)
        html_path = os.path.splitext(json_name)[0] + '.html'
        print('Generating %s... (standlone)' % html_path)
        by_source = {}
        symbols_index = {}
        symbols = []
        for obj_name, obj_data in all_data['objects'].iteritems():
            for symbol, symbol_data in obj_data.iteritems():
                size = int(symbol_data['size'])
                # Sometimes there's symbols with no source file, we just ignore
                # those.
                if 'contribs' in symbol_data:
                    i = 0
                    while i < len(symbol_data['contribs']):
                        src_index = symbol_data['contribs'][i]
                        i += 1
                        per_line = symbol_data['contribs'][i]
                        i += 1
                        source = all_data['sources'][int(src_index)]
                        if source not in by_source:
                            by_source[source] = {'lines': {}, 'total_size': 0}
                        size = 0
                        # per_line is [line, size, line, size, line, size, ...]
                        for j in range(0, len(per_line), 2):
                            line_number = per_line[j]
                            size += per_line[j + 1]
                            # Save some time/space in JS by using an array here.
                            # 0 == size,
                            # 1 == symbol list.
                            by_source[source]['lines'].setdefault(
                                line_number, [0, []])
                            by_source[source]['lines'][line_number][
                                0] += per_line[j + 1]
                            if symbol in symbols_index:
                                symindex = symbols_index[symbol]
                            else:
                                symbols.append(symbol)
                                symbols_index[symbol] = symindex = len(
                                    symbols) - 1
                            by_source[source]['lines'][line_number][1].append(
                                symindex)
                        by_source[source]['total_size'] += size
        binary_name = all_data['executable']['name']
        data = {}
        data['name'] = '/'
        data['children'] = []
        file_contents = {}
        line_data = {}
        for source, file_data in by_source.iteritems():
            InsertIntoTree(data, source, file_data['total_size'])

            store_as = source[3:].replace('\\', '/')
            try:
                with codecs.open(source, 'rb', encoding='latin1') as f:
                    file_contents[store_as] = f.read()
            except IOError:
                file_contents[store_as] = '// Unable to load source.'

            line_data[store_as] = file_data['lines']
            # code_tally attempts to assign fractional bytes when code is shared
            # across multiple symbols. Round off here for display after summing
            # above.
            for per_line in line_data[store_as].values():
                per_line[0] = round(per_line[0])

        flattened = FlattenTree(data)
        maxval = 0
        for i in flattened[1:]:
            maxval = max(i[2], maxval)
        flattened_str = json.dumps(flattened)

        to_write = GetAsset('template.html')
        # Save all data and what would normally be external resources into the
        # one html so that it's a standalone report.
        with open(html_path, 'w') as f:
            f.write(to_write)
            # These aren't subbed in as a silly workaround for 32-bit python.
            # The end result is only ~100M, but while substituting these into a
            # template, it otherwise raises a MemoryError, I guess due to
            # fragmentation. So instead, we just append them as variables to the
            # file and then refer to the variables in the main script.
            filedata_str = json.dumps(file_contents).replace(
                '</script>', '</scr"+"ipt>')
            AppendAsScriptBlock(f, filedata_str, var='g_file_contents')
            AppendAsScriptBlock(f, json.dumps(line_data), var='g_line_data')
            AppendAsScriptBlock(f, json.dumps(symbols), var='g_symbol_list')
            favicon_str = json.dumps(base64.b64encode(GetAsset('favicon.png')))
            AppendAsScriptBlock(f, favicon_str, var='g_favicon')
            AppendAsScriptBlock(f, flattened_str, var='g_raw_data')
            AppendAsScriptBlock(f, str(maxval), var='g_maxval')
            dllname_str = binary_name + ' ' + all_data['executable']['version']
            AppendAsScriptBlock(f, json.dumps(dllname_str), var='g_dllname')
            AppendAsScriptBlock(f, GetAsset('codemirror.js'))
            AppendAsScriptBlock(f, GetAsset('clike.js'))
            AppendAsScriptBlock(f, GetAsset('main.js'))
            f.write('</html>')

    return 0


if __name__ == '__main__':
    sys.exit(main())
