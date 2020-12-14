#!/usr/bin/env python
#
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess


def get_file_lines(file_path):
    '''Returns list of lines from a file. '\n' at the end of lines are
    removed.'''
    with open(file_path, 'r') as f:
        file_lines = f.readlines()
    return [l.rstrip() for l in file_lines]


def save_file_lines(file_path, file_lines):
    '''Saves file_lines in file pointed by `file_path`, '\n' at the end of
    lines are added back.'''
    with open(file_path, 'w') as f:
        for line in file_lines:
            f.write(line + '\n')


def get_relative_dependency(path, dir_path):
    '''Given a file path, returns formatted BUILD.gn dependency.

    Parameters:
        path: file path to format as relative dependency.
        dir_path: directory from which the relative dependency is calculated.

    Returns:
        Formatted dependency.
        The 3 cases illustrated below are handled:
        - ":file_type.m" if returned if file_type.js is in dir_path.
        - "metadata:metadata_model.m" is returned if metadata/metadata_model.js
        is in dir_path.
        - "//ui/file_manager/externs:volume_manager.m" is returned if
        ui/file_manager/externs is not included in dir_path.
    '''
    split_path = path.split('/')
    file_name = split_path.pop().replace('.js', '.m')
    split_dir_path = dir_path.split('/')
    while (len(split_path) > 0 and len(split_dir_path) > 0
           and split_path[0] == split_dir_path[0]):
        del split_path[0]
        del split_dir_path[0]
    if len(split_dir_path) == 0:
        # The dependency is within dir_path.
        return '/'.join(split_path) + ':' + file_name
    else:
        return '//' + re.sub(r"\/[a-zA-Z0-9_]+\.js", ":", path) + file_name


def get_index_substr(file_lines, substr):
    '''Finds first occurence of `substr` and returns its index in
    `file_lines`.'''
    for i, line in enumerate(file_lines):
        if substr in line:
            return i
    return -1


def get_end_of_copyright(file_lines):
    '''Get index of last line of copyright (after checking that the section
    exists).'''
    index = get_index_substr(file_lines, 'Copyright 20')
    if index < 0:
        return -1
    while index < len(file_lines) and file_lines[index].startswith('// '):
        index += 1
    return index - 1


def get_end_of_file_overview(file_lines):
    '''Get index of last line of file_overview (after checking that the section
    exists).'''
    index = get_index_substr(file_lines, '@fileoverview')
    if index < 0:
        return -1
    while index < len(file_lines) and file_lines[index] != ' */':
        index += 1
    return index


def get_last_index_of_line_starting_with(file_lines, substr):
    '''Get last line starting with the given `substr`.'''
    last_index = -1
    for i, line in enumerate(file_lines):
        if line.startswith(substr):
            last_index = i
    return last_index


def get_end_of_build_imports(file_lines):
    '''In BUILD.gn, gets index of last 'import' line at the beginning of the
    file.'''
    index = get_last_index_of_line_starting_with(file_lines, 'import("//')
    if index < 0:
        return get_end_of_copyright(file_lines)
    return index


def add_js_library(file_lines, file_name, dir_path):
    '''Adds js_library target in BUILD.gn.'''
    i = file_lines.index('js_library("%s") {' % (file_name))

    # Check that the library does exist and hasn't already been converted.
    if i < 0:
        print 'Unable to find js_library for {}'.format(file_name)
        return False
    if 'js_library("%s.m") {' % (file_name) in file_lines:
        print 'js_library for {}.m already added'.format(file_name)
        return False

    # Find the end of the library definition.
    while i < len(file_lines) and file_lines[i] != '}':
        i += 1
    i += 1
    if i == len(file_lines):
        print 'reached end of file'
        return False
    new_lines = '''
js_library("%s.m") {
  sources = [ "$root_gen_dir/%s/%s.m.js" ]

  extra_deps = [ ":modulize" ]
}''' % (file_name, dir_path, file_name)
    file_lines[i:i] = new_lines.split('\n')
    return True


def add_import_line(file_lines, variable, relative_path, is_unittest):
    '''Adds import line (import {...} from '...') in JS file.'''
    # Construct import line.
    import_line = 'import ' if is_unittest else '// #import '

    # Check: existing relative path.
    i = get_index_substr(file_lines, relative_path)
    if i >= 0 and '}' in file_lines[i]:
        if variable + '}' in file_lines[i] or variable + ',' in file_lines[i]:
            return
        split_line = file_lines[i].split('}')
        file_lines[i] = '%s, %s}%s' % (split_line[0], variable, split_line[1])
        return
    import_line += "{%s} from '%s';" % (variable, relative_path)

    if import_line in file_lines:
        return

    # Add clang-format off/on if necessary.
    index = 0
    if '// clang-format off' in file_lines:
        index = file_lines.index('// clang-format off')
    else:
        # Go to the end of copyright and fileoverview.
        index = get_end_of_file_overview(file_lines)
        if index < 0:
            index = get_end_of_copyright(file_lines)
            if (index < 0):
                index = 0
        index += 1
        if len(import_line) > 80:
            index += 1
            file_lines.insert(index, '// clang-format off')

            # Go to the last existing import line.
            last_import_line_index = get_last_index_of_line_starting_with(
                file_lines, 'import ')
            if last_import_line_index >= 0:
                index = last_import_line_index
            else:
                file_lines.insert(index + 1, '')
            file_lines.insert(index + 1, '// clang-format on')
        elif 'import ' not in file_lines[index + 1]:
            file_lines.insert(index, '')

    # Add import line.
    file_lines.insert(index + 1, import_line)


def add_namespace_rewrite(build_gn_path):
    build_file_lines = get_file_lines(build_gn_path)

    # Add import("//ui/webui/resources/js/cr.gni").
    cr_gni = 'import("//ui/webui/resources/js/cr.gni")'
    modulizer_gni = 'import("//ui/webui/resources/tools/js_modulizer.gni")'
    if not cr_gni in build_file_lines:
        if not modulizer_gni in build_file_lines:
            raise ValueError('"js_modulizer.gni" not found')
        index = build_file_lines.index(modulizer_gni)
        build_file_lines.insert(index, cr_gni)

    # Add namespace_rewrites = cr_namespace_rewrites.
    namespace_rewrite = '  namespace_rewrites = cr_namespace_rewrites'
    if not namespace_rewrite in build_file_lines:
        index = get_index_substr(build_file_lines,
                                 'js_modulizer("modulize") {')
        if index < 0:
            print 'No modulize rule found'
            return
        while index < len(build_file_lines) and build_file_lines[index] != '':
            index += 1
        index -= 1
        build_file_lines.insert(index, '')
        build_file_lines.insert(index + 1, namespace_rewrite)

    save_file_lines(build_gn_path, build_file_lines)


def add_hide_third_party(build_gn_path):
    build_file_lines = get_file_lines(build_gn_path)

    hide_third_party = '    "hide_warnings_for=third_party/",'
    prefix_replacement_line = 'browser_resolver_prefix_replacements='
    if not hide_third_party in build_file_lines:
        index = get_index_substr(build_file_lines, prefix_replacement_line)
        if index < 0:
            print 'prefix replacement not found in "js_test_gen_html_modules"'
            return
        build_file_lines.insert(index + 1, hide_third_party)

    save_file_lines(build_gn_path, build_file_lines)


def add_dependency(file_lines, rule_first_line, list_name, dependency_line):
    '''
    Add dependency in BUILD.gn.
    Parameters:
        file_lines: lines of BUILD.gn file.
        rule_first_line: opening line of target to update.
        list_name: name of the dependency list, deps', 'input_files' etc...
        dependency_line: line to add to the dependency list to update.
    '''
    # Find a line that starts with deps.
    if not rule_first_line in file_lines:
        print 'Unable to find ' + rule_first_line
        return False

    # Find index of `list_name`. Get index of 'sources = [' in case `list_name`
    # is not defined.
    rule_index = file_lines.index(rule_first_line)
    sources_index = -1
    insertion_index = -1
    single_line_dependency_list = False

    for i in range(rule_index, len(file_lines)):
        if 'sources = [' in file_lines[i]:
            # Jump to the end of the 'sources' list.
            while not ']' in file_lines[i]:
                i += 1
            sources_index = i
        if '  {} = '.format(list_name) in file_lines[i]:
            # Dependency line found.
            if file_lines[i].endswith(']'):
                single_line_dependency_list = True

            # Go to the end of the dependency list.
            while not ']' in file_lines[i]:
                if dependency_line == file_lines[i]:
                    # Dependency already found.
                    return False
                i += 1
            insertion_index = i
            break
        if file_lines[i] == '}':
            # End of build rule.
            break

    if insertion_index == -1:
        # Add dependency line, after sources if possible.
        index = sources_index + 1 if sources_index > 0 else rule_index + 1

        # Define new list over 2 lines: 'list_name = [\n]'
        file_lines.insert(index, '  {} = ['.format(list_name))
        file_lines.insert(index + 1, '  ]')
        insertion_index = index + 1

    if single_line_dependency_list:
        # Use regex to find characters between [].
        result = re.search('\[(.*)\]', file_lines[insertion_index])
        existing_dependency = result.group(1).strip()
        new_lines = '''\
  {} = [
    {},
  ]'''.format(list_name, existing_dependency)

        # Rewrite single-line dependency list.
        file_lines[insertion_index:insertion_index + 1] = new_lines.split('\n')
        insertion_index += 1

        # Check for already imported dependency after reformatting.
        if file_lines[insertion_index] == dependency_line:
            return False

        # If there was no existing dependency, remove appropriate line.
        if existing_dependency == '':
            del file_lines[insertion_index]

    # Insert dependency.
    file_lines.insert(insertion_index, dependency_line)
    return True


def update_build_gn_dependencies(dir_path, file_name, build_gn_path):
    print 'Updating BUILD.gn dependencies for ' + file_name

    # Get file contents.
    file_lines = get_file_lines(build_gn_path)

    # Edit file with modules-related targets.
    import_gni = 'import("//ui/webui/resources/tools/js_modulizer.gni")'
    if not import_gni in file_lines:
        index = get_end_of_build_imports(file_lines) + 1
        file_lines.insert(index, import_gni)
        new_lines = '''
js_modulizer("modulize") {
  input_files = [
  ]
}'''
        file_lines.extend(new_lines.split('\n'))
        if not add_dependency(file_lines, 'group("closure_compile") {', 'deps',
                              '    ":closure_compile_jsmodules",'):
            return

        # Add closure_compile_jsmodules rule.
        index = get_index_substr(file_lines,
                                 'js_type_check("closure_compile_module") {')
        if index < 0:
            print 'js_type_check("closure_compile_module") not found'
            return
        new_lines = '''\
js_type_check("closure_compile_jsmodules") {
  uses_js_modules = true
  deps = [
  ]
}
'''
        file_lines[index:index] = new_lines.split('\n')
    if not add_js_library(file_lines, file_name, dir_path):
        return

    # Add closure dependency.
    if not add_dependency(
            file_lines, 'js_type_check("closure_compile_jsmodules") {', 'deps',
            '    ":{}.m",'.format(file_name)):
        return

    # Add 'modulize' dependency.
    if not add_dependency(file_lines, 'js_modulizer("modulize") {',
                          'input_files', '    "{}.js",'.format(file_name)):
        return

    # Save file contents.
    save_file_lines(build_gn_path, file_lines)


def update_buid_gn_unittest_dependencies(build_gn_path, js_file_path,
                                         file_name):
    '''
    Rename _unittest.js to _unittest.m.js.
    Update test URL in file_manager_jstest.cc.
    Update BUILD.gn rules.
    It is assumed that if we're converting [file]_unittest.js, [file].js has
    been converted already.

    Parameters:
        build_gn_path: Path of BUILD.gn file used by the file be converted.
        js_file_path: Path of the file to be converted
                        (ui/file_manager/.../..._unittest.m.js)
        file_name: Name of the file to be converted (..._unittest)
    '''
    print 'Updating BUILD.gn dependencies for ' + file_name

    main_build_path = 'ui/file_manager/BUILD.gn'
    file_manager_jstest_path = ('chrome/browser/chromeos/file_manager/'
                                'file_manager_jstest.cc')

    # Get file contents.
    build_file_lines = get_file_lines(build_gn_path)
    main_build_file_lines = get_file_lines(main_build_path)
    file_manager_jstest_lines = get_file_lines(file_manager_jstest_path)

    # Rename _unittest.js to _unittest.m.js.
    original_js_file_path = js_file_path.replace('.m.js', '.js')
    if not os.path.isfile(js_file_path):
        if not os.path.isfile(original_js_file_path):
            e = 'Could not find neither file {} or {}'.format(
                js_file_path, original_js_file_path)
            raise ValueError(e)
        os.rename(original_js_file_path, js_file_path)

    # Update chrome/browser/chromeos/file_manager/file_manager_jstest.cc
    # by updating RunTestURL for the unittest being converted.
    # [...]_unittest_gen.html -> [...]_unittest.m_gen.html.
    index = get_index_substr(file_manager_jstest_lines, file_name + '.m')
    if index < 0:
        index = get_index_substr(file_manager_jstest_lines, file_name)
        if index < 0:
            print 'file_manager_jstest.cc: {} not found'.format(file_name)
        file_manager_jstest_lines[index] = file_manager_jstest_lines[
            index].replace(file_name, file_name + '.m')

    # Add "js_test_gen_html_modules" target.
    build_rule = 'js_test_gen_html("js_test_gen_html_modules") {'
    if not build_rule in build_file_lines:
        if 'js_test_gen_html("js_test_gen_html") {' in build_file_lines:
            index = build_file_lines.index(
                'js_test_gen_html("js_test_gen_html") {') - 1
            new_lines = r'''
js_test_gen_html("js_test_gen_html_modules") {
  deps = [
  ]
  js_module = true

  closure_flags =
      strict_error_checking_closure_args + [
        "js_module_root=./gen/ui",
        "js_module_root=../../ui",
        "browser_resolver_prefix_replacements=\"chrome://test/=./\"",
      ]
}'''
            build_file_lines[index:index] = new_lines.split('\n')
        else:
            e = '"js_test_gen_html" build target not found on {}'.format(
                build_gn_path)
            raise ValueError(e)

    # Add ":js_test_gen_html_modules_type_check_auto", in closure_compile deps.
    index = get_index_substr(build_file_lines,
                             ':js_test_gen_html_modules_type_check_auto')
    if index < 0:
        index = get_index_substr(build_file_lines,
                                 ':js_test_gen_html_type_check_auto')
        if index < 0:
            e = '"js_test_gen_html_type_check_auto" dependency not found'
            raise ValueError(e)
        else:
            new_line = '    ":js_test_gen_html_modules_type_check_auto",'
            build_file_lines.insert(index, new_line)

    # In BUILD.gn, update js_unittest target to .m
    js_unittest = 'js_unittest("%s") {' % (file_name)
    js_unittest_m = 'js_unittest("%s.m") {' % (file_name)
    if js_unittest in build_file_lines:
        index = build_file_lines.index(js_unittest)
        build_file_lines[index] = js_unittest_m
        # Remove dependencies.
        while index < len(
                build_file_lines) and not 'deps =' in build_file_lines[index]:
            index += 1
        if ']' in build_file_lines[index]:
            build_file_lines[index] = '  deps = ['
            build_file_lines.insert(index, '  ]')
        else:
            index += 1
            while index < len(
                    build_file_lines) and not ']' in build_file_lines[index]:
                del build_file_lines[index]
    else:
        if not js_unittest_m in build_file_lines:
            print 'Unable to find "{}" target'.format(file_name)

    # Move unittest dependency from js_test_gen_html to
    # js_test_gen_html_modules.
    index = get_index_substr(build_file_lines, '":{}",'.format(file_name))
    if index >= 0:
        del build_file_lines[index]
    else:
        single_line_deps = '  deps = [ ":{}" ]'.format(file_name)
        if single_line_deps in build_file_lines:
            index = build_file_lines.index(single_line_deps)
            build_file_lines[index] = '  deps = []'

    add_dependency(build_file_lines,
                   'js_test_gen_html("js_test_gen_html_modules") {', 'deps',
                   '    ":{}.m",'.format(file_name))
    # Add dependency in ui/file_manager/BUILD.gn.
    dir_path = os.path.dirname(js_file_path)
    rel_dir_path = '/'.join(dir_path.split('/')[2:])
    dependency_line = '    "{}:js_test_gen_html_modules",'.format(rel_dir_path)
    if not dependency_line in main_build_file_lines:
        if not add_dependency(main_build_file_lines,
                              'group("unit_test_data") {', 'deps',
                              dependency_line):
            print 'unable to update ui/file_manager/BUILD.gn'

    # Save file contents.
    save_file_lines(build_gn_path, build_file_lines)
    save_file_lines(main_build_path, main_build_file_lines)
    save_file_lines(file_manager_jstest_path, file_manager_jstest_lines)


def parse_compiler_output(compiler_output, build_gn_path, file_name):
    '''Parse closure compile output and return list of token that are
    undefined/undeclared (no duplicate).'''
    print "Parsing closure compiler output"
    with open(compiler_output, 'r') as f:
        # Retrieve from the closure compile output the types that need to be
        # imported.
        variables_to_import = []
        for line in f:
            line = line.rstrip()
            # Select from the output the lines that contain 'ERROR - []' and
            # 'Unknown type '.
            if 'ERROR - [' in line:
                if not file_name in line:
                    if '/third_party/' in line:
                        add_hide_third_party(build_gn_path)
                    else:
                        print 'UNHANDLED ERROR (Other file): ' + line
                elif 'Unknown type ' in line:
                    unknown_type = line.split('Unknown type ')[1]
                    # Select the main type by removing what follows the dot:
                    # ThumbnailLoader.LoaderType -> ThumbnailLoader.
                    unknown_type = unknown_type.split('.')[0]
                    if not unknown_type in variables_to_import:
                        variables_to_import.append(unknown_type)
                elif 'variable ' in line and ' is undeclared' in line:
                    if ' cr ' in line:  # namespace rewrite.
                        add_namespace_rewrite(build_gn_path)
                        continue
                    unknown_type = line.split('variable ')[1]
                    unknown_type = unknown_type.split(' is undeclared')[0]
                    if not unknown_type in variables_to_import:
                        variables_to_import.append(unknown_type)
                else:
                    print 'UNHANDLED ERROR: ' + line
    return variables_to_import


def find_dependencies(dir_path, build_gn_path, js_file_path,
                      variables_to_import, is_unittest):
    '''Finds dependencies and updates BUILD.gn and JS file to import these
    dependencies.'''
    # Get BUILD.gn and JS file contents.
    build_file_lines = get_file_lines(build_gn_path)
    js_file_lines = get_file_lines(js_file_path)

    file_name = js_file_path.split('/').pop().replace('.js', '')

    # Define the first line of the rule in which the new dependency has
    # to be added.
    rule_first_line = 'js_unittest("%s") {' % (
        file_name) if is_unittest else 'js_library("%s.m") {' % (file_name)

    # Special case: classes that extend "cr.EventTarget".
    index = get_index_substr(js_file_lines, ' extends cr.EventTarget')
    if index >= 0:
        if is_unittest:
            js_file_lines[index] = js_file_lines[index].replace(
                ' extends cr.EventTarget', ' extends EventTarget')
        add_dependency(build_file_lines, rule_first_line, 'deps',
                       '    "//ui/webui/resources/js/cr:event_target.m",')
        add_import_line(js_file_lines, 'NativeEventTarget as EventTarget',
                        'chrome://resources/js/cr/event_target.m.js',
                        is_unittest)

    # General case.
    for variable in variables_to_import:
        # First, handle special cases.

        # "//chrome/test/data/webui:chai_assert".
        out, _ = subprocess.Popen([
            'egrep', '-R', 'export function {}\('.format(variable), '-l',
            './chrome/test/data/webui/chai_assert.js'
        ],
                                  stdout=subprocess.PIPE).communicate()
        if out.rstrip() != '':
            add_dependency(build_file_lines, rule_first_line, 'deps',
                           '    "//chrome/test/data/webui:chai_assert",')
            add_import_line(js_file_lines, variable,
                            'chrome://test/chai_assert.js', is_unittest)
            continue

        # "//ui/webui/resources/js".
        out, _ = subprocess.Popen([
            'egrep', '-R', '\/\* #export \*\/ \w+ {}\W'.format(variable), '-l',
            './ui/webui/resources/js'
        ],
                                  stdout=subprocess.PIPE).communicate()
        path = out.rstrip()
        print path
        if path != '':
            path = path.replace('./', '//').replace('.js', '.m')
            dependency = ':'.join(path.rsplit(
                '/', 1))  # //ui/webui/resources/js:assert.m.
            add_dependency(build_file_lines, rule_first_line, 'deps',
                           '    "{}",'.format(dependency))
            path = path.replace('//ui/webui/', 'chrome://').replace(
                '.m', '.m.js')  # chrome://resources/js/assert.m.js.
            add_import_line(js_file_lines, variable, path, is_unittest)
            continue

        # Look for exported variables.
        out, _ = subprocess.Popen([
            'egrep', '-R', '\/\* #export \*\/ \w+ {}\W'.format(variable), '-l',
            './ui/file_manager/'
        ],
                                  stdout=subprocess.PIPE).communicate()
        path = out.rstrip()
        if path == '':
            # Look for variables exported as wrapped objects
            out, _ = subprocess.Popen([
                'egrep', '-R',
                '\/\* #export \*\/ \{%s\}' % variable, '-l',
                './ui/file_manager/'
            ],
                                      stdout=subprocess.PIPE).communicate()
            path = out.rstrip()
            if path == '':
                continue
        if '\n' in path:
            print '{}: export found in more than 1 file'.format(variable)
            print path
            continue

        # Remove './' at the start.
        path = path[2:]
        relative_path = os.path.relpath(path, start=dir_path).replace(
            '.js', '.m.js')
        if not relative_path.startswith('../'):
            relative_path = './' + relative_path
        add_import_line(js_file_lines, variable, relative_path, is_unittest)
        add_dependency(
            build_file_lines, rule_first_line, 'deps',
            '    "{}",'.format(get_relative_dependency(path, dir_path)))

    # Save BUILD.gn and JS file contents.
    save_file_lines(build_gn_path, build_file_lines)
    save_file_lines(js_file_path, js_file_lines)


def add_js_file_exports(file_lines):
    for i, line in enumerate(file_lines):
        # Export class.
        if line.startswith('class '):
            # Extract class name.
            class_name = line.split(' ')[1]
            # Export class if this class is used or referred to in another js
            # file.
            out, _ = subprocess.Popen([
                'egrep', '-R', '\W{}\W'.format(class_name), '-l',
                './ui/file_manager/'
            ],
                                      stdout=subprocess.PIPE).communicate()
            path = out.rstrip()
            if not '\n' in path:
                print 'Not exporting ' + class_name + ': local class'
                continue
            file_lines[i] = '/* #export */ ' + line
        # Export variable in global namespace.
        elif line.startswith('const ') or line.startswith(
                'let ') or line.startswith('var '):
            # Extract variable name.
            variable_name = line.split(' ')[1]

            # Check if the variable is further defined/extended in the file. If
            # so, export it.
            export_variable = False
            for l in range(len(file_lines)):
                if file_lines[l].startswith(variable_name + '.'):
                    export_variable = True
                    break
            if not export_variable:
                continue

            # Check if export already added.
            if get_index_substr(file_lines, '/* #export */ {%s}' %
                                (variable_name)) >= 0:
                continue

            # Export variable in wrapping object at the end of the file.
            new_lines = '''
// eslint-disable-next-line semi,no-extra-semi
/* #export */ {%s};''' % (variable_name)
            file_lines.extend(new_lines.split('\n'))

            # Check if @suppress already added.
            if get_index_substr(file_lines, ' @suppress {uselessCode}') >= 0:
                continue
            index = get_end_of_file_overview(file_lines)
            if index < 0:
                index = get_end_of_copyright(file_lines)
                if index < 0:
                    index = 0
                index += 1
                new_lines = '''
/**
 * @fileoverview
 * @suppress {uselessCode} Temporary suppress %s.
 */''' % ('because of the line exporting')
                file_lines[index:index] = new_lines.split('\n')
            else:
                while ' */' not in file_lines[index]:
                    index += 1
                new_line = (' * @suppress {uselessCode} Temporary suppress '
                            'because of the line exporting.')
                file_lines.insert(index, new_line)
        # Export function.
        elif line.startswith('function '):
            # Extract function name.
            result = re.search('function (.*)\(', line)
            function_name = result.group(1).strip()

            # Check if the function is used within the current file.
            filtered_file_lines = filter(lambda x: x != line, file_lines)
            if get_index_substr(filtered_file_lines,
                                '{}('.format(function_name)) < 0:
                # The function has to be used outside the file, so has to be
                # exported.
                file_lines[i] = '/* #export */ ' + line


def convert_js_file(js_file_path):
    '''Add exports (/* #export */) and ignore 'use strict'.'''
    file_lines = get_file_lines(js_file_path)

    # Ignore 'use strict'.
    index = get_index_substr(file_lines, "'use strict'")
    if index >= 0:
        file_lines[index] = file_lines[index].replace(
            "'use strict'", "/* #ignore */ 'use strict'")

    # Add exports.
    add_js_file_exports(file_lines)

    # Save file contents.
    save_file_lines(js_file_path, file_lines)


def convert_unittest_file(js_file_path):
    '''Add exports and remove 'use strict'.'''
    file_lines = get_file_lines(js_file_path)

    # Remove 'use strict'.
    index = get_index_substr(file_lines, "'use strict'")
    if index >= 0:
        del file_lines[index]

    # Add exports.
    for i, line in enumerate(file_lines):
        if line.startswith('function setUp()') or line.startswith(
                'function test'):
            file_lines[i] = 'export ' + file_lines[i]

    # Save file contents.
    save_file_lines(js_file_path, file_lines)


def main():
    # Get command line arguments.
    js_file_path = sys.argv[1]
    action = sys.argv[2]
    if not js_file_path.endswith('.js'):
        e = "Argument 1 {} isn't a JS file (expected .js extension)".format(
            js_file_path)
        raise ValueError(e)
    dir_path = os.path.dirname(js_file_path)
    build_gn_path = dir_path + '/BUILD.gn'
    file_name = os.path.basename(js_file_path).replace('.js', '')
    is_unittest = file_name.endswith('_unittest')
    if is_unittest:
        js_file_path = js_file_path.replace('.js', '.m.js')

    if (action == 'generate'):
        if is_unittest:
            update_buid_gn_unittest_dependencies(build_gn_path, js_file_path,
                                                 file_name)
            convert_unittest_file(js_file_path)
        else:
            update_build_gn_dependencies(dir_path, file_name, build_gn_path)
            convert_js_file(js_file_path)
    elif (action == 'parse'):
        compiler_output = sys.argv[3]
        variables_to_import = parse_compiler_output(compiler_output,
                                                    build_gn_path, file_name)
        find_dependencies(dir_path, build_gn_path, js_file_path,
                          variables_to_import, is_unittest)


if __name__ == '__main__':
    print "-----modules.py-----"
    main()
    print "--------------------"
