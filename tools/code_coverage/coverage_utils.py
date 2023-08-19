# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The script intentionally does not have a shebang, as it is Py2/Py3 compatible.

import argparse
from collections import defaultdict
import functools
import json
import logging
import os
import re
import shutil
import subprocess
import sys

# Appends third_party/ so that coverage_utils can import jinja2 from
# third_party/.
sys.path.append(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir,
                 'third_party'))
import jinja2

# The default name of the html coverage report for a directory.
DIRECTORY_COVERAGE_HTML_REPORT_NAME = os.extsep.join(['report', 'html'])

# Name of the html index files for different views.
COMPONENT_VIEW_INDEX_FILE = os.extsep.join(['component_view_index', 'html'])
DIRECTORY_VIEW_INDEX_FILE = os.extsep.join(['directory_view_index', 'html'])
FILE_VIEW_INDEX_FILE = os.extsep.join(['file_view_index', 'html'])
INDEX_HTML_FILE = os.extsep.join(['index', 'html'])
REPORT_DIR = 'coverage'


class CoverageSummary(object):
  """Encapsulates coverage summary representation."""

  def __init__(self,
               regions_total=0,
               regions_covered=0,
               functions_total=0,
               functions_covered=0,
               lines_total=0,
               lines_covered=0):
    """Initializes CoverageSummary object."""
    self._summary = {
        'regions': {
            'total': regions_total,
            'covered': regions_covered
        },
        'functions': {
            'total': functions_total,
            'covered': functions_covered
        },
        'lines': {
            'total': lines_total,
            'covered': lines_covered
        }
    }

  def Get(self):
    """Returns summary as a dictionary."""
    return self._summary

  def AddSummary(self, other_summary):
    """Adds another summary to this one element-wise."""
    for feature in self._summary:
      self._summary[feature]['total'] += other_summary.Get()[feature]['total']
      self._summary[feature]['covered'] += other_summary.Get()[feature][
          'covered']


class CoverageReportHtmlGenerator(object):
  """Encapsulates coverage html report generation.

  The generated html has a table that contains links to other coverage reports.
  """

  def __init__(self, output_dir, output_path, table_entry_type):
    """Initializes _CoverageReportHtmlGenerator object.

    Args:
      output_dir: Path to the dir for writing coverage report to.
      output_path: Path to the html report that will be generated.
      table_entry_type: Type of the table entries to be displayed in the table
                        header. For example: 'Path', 'Component'.
    """
    css_file_name = os.extsep.join(['style', 'css'])
    css_absolute_path = os.path.join(output_dir, css_file_name)
    assert os.path.exists(css_absolute_path), (
        'css file doesn\'t exit. Please make sure "llvm-cov show -format=html" '
        'is called first, and the css file is generated at: "%s".' %
        css_absolute_path)

    self._css_absolute_path = css_absolute_path
    self._output_dir = output_dir
    self._output_path = output_path
    self._table_entry_type = table_entry_type

    self._table_entries = []
    self._total_entry = {}

    source_dir = os.path.dirname(os.path.realpath(__file__))
    template_dir = os.path.join(source_dir, 'html_templates')

    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_dir), trim_blocks=True)
    self._header_template = jinja_env.get_template('header.html')
    self._table_template = jinja_env.get_template('table.html')
    self._footer_template = jinja_env.get_template('footer.html')

    self._style_overrides = open(
        os.path.join(source_dir, 'static', 'css', 'style.css')).read()

  def AddLinkToAnotherReport(self, html_report_path, name, summary):
    """Adds a link to another html report in this report.

    The link to be added is assumed to be an entry in this directory.
    """
    # Use relative paths instead of absolute paths to make the generated reports
    # portable.
    html_report_relative_path = GetRelativePathToDirectoryOfFile(
        html_report_path, self._output_path)

    table_entry = self._CreateTableEntryFromCoverageSummary(
        summary, html_report_relative_path, name,
        os.path.basename(html_report_path) ==
        DIRECTORY_COVERAGE_HTML_REPORT_NAME)
    self._table_entries.append(table_entry)

  def CreateTotalsEntry(self, summary):
    """Creates an entry corresponds to the 'Totals' row in the html report."""
    self._total_entry = self._CreateTableEntryFromCoverageSummary(summary)

  def _CreateTableEntryFromCoverageSummary(self,
                                           summary,
                                           href=None,
                                           name=None,
                                           is_dir=None):
    """Creates an entry to display in the html report."""
    assert (href is None and name is None and is_dir is None) or (
        href is not None and name is not None and is_dir is not None), (
            'The only scenario when href or name or is_dir can be None is when '
            'creating an entry for the Totals row, and in that case, all three '
            'attributes must be None.')

    entry = {}
    if href is not None:
      entry['href'] = href
    if name is not None:
      entry['name'] = name
    if is_dir is not None:
      entry['is_dir'] = is_dir

    summary_dict = summary.Get()
    for feature in summary_dict:
      if summary_dict[feature]['total'] == 0:
        percentage = 0.0
      else:
        percentage = float(summary_dict[feature]
                           ['covered']) / summary_dict[feature]['total'] * 100

      color_class = self._GetColorClass(percentage)
      entry[feature] = {
          'total': summary_dict[feature]['total'],
          'covered': summary_dict[feature]['covered'],
          'percentage': '{:6.2f}'.format(percentage),
          'color_class': color_class
      }

    return entry

  def _GetColorClass(self, percentage):
    """Returns the css color class based on coverage percentage."""
    if percentage >= 0 and percentage < 80:
      return 'red'
    if percentage >= 80 and percentage < 100:
      return 'yellow'
    if percentage == 100:
      return 'green'

    assert False, 'Invalid coverage percentage: "%d".' % percentage

  def WriteHtmlCoverageReport(self, no_component_view, no_file_view):
    """Writes html coverage report.

    In the report, sub-directories are displayed before files and within each
    category, entries are sorted alphabetically.
    """

    def EntryCmp(left, right):
      """Compare function for table entries."""
      if left['is_dir'] != right['is_dir']:
        return -1 if left['is_dir'] == True else 1

      return -1 if left['name'] < right['name'] else 1

    self._table_entries = sorted(
        self._table_entries, key=functools.cmp_to_key(EntryCmp))

    css_path = os.path.join(self._output_dir, os.extsep.join(['style', 'css']))

    directory_view_path = GetDirectoryViewPath(self._output_dir)
    directory_view_href = GetRelativePathToDirectoryOfFile(
        directory_view_path, self._output_path)

    component_view_href = None
    if not no_component_view:
      component_view_path = GetComponentViewPath(self._output_dir)
      component_view_href = GetRelativePathToDirectoryOfFile(
          component_view_path, self._output_path)

    # File view is optional in the report.
    file_view_href = None
    if not no_file_view:
      file_view_path = GetFileViewPath(self._output_dir)
      file_view_href = GetRelativePathToDirectoryOfFile(file_view_path,
                                                        self._output_path)

    html_header = self._header_template.render(
        css_path=GetRelativePathToDirectoryOfFile(css_path, self._output_path),
        directory_view_href=directory_view_href,
        component_view_href=component_view_href,
        file_view_href=file_view_href,
        style_overrides=self._style_overrides)

    html_table = self._table_template.render(
        entries=self._table_entries,
        total_entry=self._total_entry,
        table_entry_type=self._table_entry_type)
    html_footer = self._footer_template.render()

    if not os.path.exists(os.path.dirname(self._output_path)):
      os.makedirs(os.path.dirname(self._output_path))
    with open(self._output_path, 'w') as html_file:
      html_file.write(html_header + html_table + html_footer)


class CoverageReportPostProcessor(object):
  """Post processing of code coverage reports produced by llvm-cov."""

  def __init__(self,
               output_dir,
               src_root_dir,
               summary_data,
               no_component_view,
               no_file_view,
               component_mappings={},
               path_equivalence=None):
    """Initializes CoverageReportPostProcessor object."""
    # Caller provided parameters.
    self.output_dir = output_dir
    self.src_root_dir = os.path.normpath(GetFullPath(src_root_dir))
    if not self.src_root_dir.endswith(os.sep):
      self.src_root_dir += os.sep
    self.summary_data = json.loads(summary_data)
    assert len(self.summary_data['data']) == 1
    self.no_component_view = no_component_view
    self.no_file_view = no_file_view

    # Mapping from components to directories
    self.component_to_directories = None
    if component_mappings:
      self._ExtractComponentToDirectoriesMapping(component_mappings)

    # The root directory that contains all generated coverage html reports.
    self.report_root_dir = GetCoverageReportRootDirPath(self.output_dir)

    # The root directory that all coverage html files generated by llvm-cov.
    self.html_file_root_dir = GetHtmlFileRootDirPath(self.output_dir)

    # Path to the HTML file for the component view.
    self.component_view_path = GetComponentViewPath(self.output_dir)

    # Path to the HTML file for the directory view.
    self.directory_view_path = GetDirectoryViewPath(self.output_dir)

    # Path to the HTML file for the file view.
    self.file_view_path = GetFileViewPath(self.output_dir)

    # Path to the main HTML index file.
    self.html_index_path = GetHtmlIndexPath(self.output_dir)

    self.path_map = None
    if path_equivalence:

      def _PreparePath(path):
        path = os.path.normpath(path)
        if not path.endswith(os.sep):
          # A normalized path does not end with '/', unless it is a root dir.
          path += os.sep
        return path

      self.path_map = [_PreparePath(p) for p in path_equivalence.split(',')]
      assert len(self.path_map) == 2, 'Path equivalence argument is incorrect.'

  def _ExtractComponentToDirectoriesMapping(self, component_mappings):
    """Initializes a mapping from components to directories."""
    directory_to_component = component_mappings['dir-to-component']

    self.component_to_directories = defaultdict(list)
    for directory in sorted(directory_to_component):
      component = directory_to_component[directory]

      # Check if we already added the parent directory of this directory. If
      # yes,skip this sub-directory to avoid double-counting.
      found_parent_directory = False
      for component_directory in self.component_to_directories[component]:
        if directory.startswith(component_directory + '/'):
          found_parent_directory = True
          break

      if not found_parent_directory:
        self.component_to_directories[component].append(directory)

  def _MapToLocal(self, path):
    """Maps a path from the coverage data to a local path."""
    if not self.path_map:
      return path
    return path.replace(self.path_map[0], self.path_map[1], 1)

  def CalculatePerDirectoryCoverageSummary(self, per_file_coverage_summary):
    """Calculates per directory coverage summary."""
    logging.debug('Calculating per-directory coverage summary.')
    per_directory_coverage_summary = defaultdict(lambda: CoverageSummary())

    for file_path in per_file_coverage_summary:
      summary = per_file_coverage_summary[file_path]
      parent_dir = os.path.dirname(file_path)

      while True:
        per_directory_coverage_summary[parent_dir].AddSummary(summary)

        if os.path.normpath(parent_dir) == os.path.normpath(self.src_root_dir):
          break
        parent_dir = os.path.dirname(parent_dir)

    logging.debug('Finished calculating per-directory coverage summary.')
    return per_directory_coverage_summary

  def CalculatePerComponentCoverageSummary(self,
                                           per_directory_coverage_summary):
    """Calculates per component coverage summary."""
    logging.debug('Calculating per-component coverage summary.')
    per_component_coverage_summary = defaultdict(lambda: CoverageSummary())

    for component in self.component_to_directories:
      for directory in self.component_to_directories[component]:
        absolute_directory_path = GetFullPath(directory)
        if absolute_directory_path in per_directory_coverage_summary:
          per_component_coverage_summary[component].AddSummary(
              per_directory_coverage_summary[absolute_directory_path])

    logging.debug('Finished calculating per-component coverage summary.')
    return per_component_coverage_summary

  def GeneratePerComponentCoverageInHtml(self, per_component_coverage_summary,
                                         per_directory_coverage_summary):
    """Generates per-component coverage reports in html."""
    logging.debug('Writing per-component coverage html reports.')
    for component in per_component_coverage_summary:
      self.GenerateCoverageInHtmlForComponent(component,
                                              per_component_coverage_summary,
                                              per_directory_coverage_summary)
    logging.debug('Finished writing per-component coverage html reports.')

  def GenerateComponentViewHtmlIndexFile(self, per_component_coverage_summary):
    """Generates the html index file for component view."""
    component_view_index_file_path = self.component_view_path
    logging.debug('Generating component view html index file as: "%s".',
                  component_view_index_file_path)
    html_generator = CoverageReportHtmlGenerator(
        self.output_dir, component_view_index_file_path, 'Component')
    for component in per_component_coverage_summary:
      html_generator.AddLinkToAnotherReport(
          self.GetCoverageHtmlReportPathForComponent(component), component,
          per_component_coverage_summary[component])

    # Do not create a totals row for the component view as the value is
    # incorrect due to failure to account for UNKNOWN component and some paths
    # belonging to multiple components.
    html_generator.WriteHtmlCoverageReport(self.no_component_view,
                                           self.no_file_view)
    logging.debug('Finished generating component view html index file.')

  def GenerateCoverageInHtmlForComponent(self, component_name,
                                         per_component_coverage_summary,
                                         per_directory_coverage_summary):
    """Generates coverage html report for a component."""
    component_html_report_path = self.GetCoverageHtmlReportPathForComponent(
        component_name)
    component_html_report_dir = os.path.dirname(component_html_report_path)
    if not os.path.exists(component_html_report_dir):
      os.makedirs(component_html_report_dir)

    html_generator = CoverageReportHtmlGenerator(
        self.output_dir, component_html_report_path, 'Path')

    for dir_path in self.component_to_directories[component_name]:
      dir_absolute_path = GetFullPath(dir_path)
      if dir_absolute_path not in per_directory_coverage_summary:
        # Any directory without an exercised file shouldn't be included into
        # the report.
        continue

      html_generator.AddLinkToAnotherReport(
          self.GetCoverageHtmlReportPathForDirectory(dir_path),
          os.path.relpath(dir_path, self.src_root_dir),
          per_directory_coverage_summary[dir_absolute_path])

    html_generator.CreateTotalsEntry(
        per_component_coverage_summary[component_name])
    html_generator.WriteHtmlCoverageReport(self.no_component_view,
                                           self.no_file_view)

  def GetCoverageHtmlReportPathForComponent(self, component_name):
    """Given a component, returns the corresponding html report path."""
    component_file_name = component_name.lower().replace('>', '-')
    html_report_name = os.extsep.join([component_file_name, 'html'])
    return os.path.join(self.report_root_dir, 'components', html_report_name)

  def GetCoverageHtmlReportPathForDirectory(self, dir_path):
    """Given a directory path, returns the corresponding html report path."""
    assert os.path.isdir(
        self._MapToLocal(dir_path)), '"%s" is not a directory.' % dir_path
    html_report_path = os.path.join(
        GetFullPath(dir_path), DIRECTORY_COVERAGE_HTML_REPORT_NAME)

    return self.CombineAbsolutePaths(self.report_root_dir, html_report_path)

  def GetCoverageHtmlReportPathForFile(self, file_path):
    """Given a file path, returns the corresponding html report path."""
    assert os.path.isfile(
        self._MapToLocal(file_path)), '"%s" is not a file.' % file_path
    html_report_path = os.extsep.join([GetFullPath(file_path), 'html'])

    return self.CombineAbsolutePaths(self.html_file_root_dir, html_report_path)

  def CombineAbsolutePaths(self, path1, path2):
    if GetHostPlatform() == 'win':
      # Absolute paths in Windows may start with a drive letter and colon.
      # Remove them from the second path before appending to the first.
      _, path2 = os.path.splitdrive(path2)

    # '+' is used instead of os.path.join because both of them are absolute
    # paths and os.path.join ignores the first path.
    return path1 + path2

  def GenerateFileViewHtmlIndexFile(self, per_file_coverage_summary,
                                    file_view_index_file_path):
    """Generates html index file for file view."""
    logging.debug('Generating file view html index file as: "%s".',
                  file_view_index_file_path)
    html_generator = CoverageReportHtmlGenerator(
        self.output_dir, file_view_index_file_path, 'Path')
    totals_coverage_summary = CoverageSummary()

    for file_path in per_file_coverage_summary:
      if not os.path.isfile(self._MapToLocal(file_path)):
        logging.warning('%s is not a file.', file_path)
        continue
      totals_coverage_summary.AddSummary(per_file_coverage_summary[file_path])
      html_generator.AddLinkToAnotherReport(
          self.GetCoverageHtmlReportPathForFile(file_path),
          os.path.relpath(file_path, self.src_root_dir),
          per_file_coverage_summary[file_path])

    html_generator.CreateTotalsEntry(totals_coverage_summary)
    html_generator.WriteHtmlCoverageReport(self.no_component_view,
                                           self.no_file_view)
    logging.debug('Finished generating file view html index file.')

  def GeneratePerFileCoverageSummary(self):
    """Generate per file coverage summary using coverage data in JSON format."""
    files_coverage_data = self.summary_data['data'][0]['files']

    per_file_coverage_summary = {}
    for file_coverage_data in files_coverage_data:
      file_path = os.path.normpath(file_coverage_data['filename'])
      assert file_path.startswith(self.src_root_dir), (
          'File path "%s" in coverage summary is outside source checkout.' %
          file_path)

      summary = file_coverage_data['summary']
      if summary['lines']['count'] == 0:
        continue

      per_file_coverage_summary[file_path] = CoverageSummary(
          regions_total=summary['regions']['count'],
          regions_covered=summary['regions']['covered'],
          functions_total=summary['functions']['count'],
          functions_covered=summary['functions']['covered'],
          lines_total=summary['lines']['count'],
          lines_covered=summary['lines']['covered'])

    logging.debug('Finished generating per-file code coverage summary.')
    return per_file_coverage_summary

  def GeneratePerDirectoryCoverageInHtml(self, per_directory_coverage_summary,
                                         per_file_coverage_summary):
    """Generates per directory coverage breakdown in html."""
    logging.debug('Writing per-directory coverage html reports.')
    for dir_path in per_directory_coverage_summary:
      self.GenerateCoverageInHtmlForDirectory(
          dir_path, per_directory_coverage_summary, per_file_coverage_summary)

    logging.debug('Finished writing per-directory coverage html reports.')

  def GenerateCoverageInHtmlForDirectory(self, dir_path,
                                         per_directory_coverage_summary,
                                         per_file_coverage_summary):
    """Generates coverage html report for a single directory."""
    html_generator = CoverageReportHtmlGenerator(
        self.output_dir, self.GetCoverageHtmlReportPathForDirectory(dir_path),
        'Path')

    for entry_name in os.listdir(self._MapToLocal(dir_path)):
      entry_path = os.path.normpath(os.path.join(dir_path, entry_name))

      if entry_path in per_file_coverage_summary:
        entry_html_report_path = self.GetCoverageHtmlReportPathForFile(
            entry_path)
        entry_coverage_summary = per_file_coverage_summary[entry_path]
      elif entry_path in per_directory_coverage_summary:
        entry_html_report_path = self.GetCoverageHtmlReportPathForDirectory(
            entry_path)
        entry_coverage_summary = per_directory_coverage_summary[entry_path]
      else:
        # Any file without executable lines shouldn't be included into the
        # report. For example, OWNER and README.md files.
        continue

      html_generator.AddLinkToAnotherReport(entry_html_report_path,
                                            os.path.basename(entry_path),
                                            entry_coverage_summary)

    html_generator.CreateTotalsEntry(per_directory_coverage_summary[dir_path])
    html_generator.WriteHtmlCoverageReport(self.no_component_view,
                                           self.no_file_view)

  def GenerateDirectoryViewHtmlIndexFile(self):
    """Generates the html index file for directory view.

    Note that the index file is already generated under src_root_dir, so this
    file simply redirects to it, and the reason of this extra layer is for
    structural consistency with other views.
    """
    directory_view_index_file_path = self.directory_view_path
    logging.debug('Generating directory view html index file as: "%s".',
                  directory_view_index_file_path)
    src_root_html_report_path = self.GetCoverageHtmlReportPathForDirectory(
        self.src_root_dir)
    WriteRedirectHtmlFile(directory_view_index_file_path,
                          src_root_html_report_path)
    logging.debug('Finished generating directory view html index file.')

  def OverwriteHtmlReportsIndexFile(self):
    """Overwrites the root index file to redirect to the default view."""
    html_index_file_path = self.html_index_path
    directory_view_index_file_path = self.directory_view_path
    WriteRedirectHtmlFile(html_index_file_path, directory_view_index_file_path)

  def CleanUpOutputDir(self):
    """Perform a cleanup of the output dir."""
    # Remove the default index.html file produced by llvm-cov.
    index_path = os.path.join(self.output_dir, INDEX_HTML_FILE)
    if os.path.exists(index_path):
      os.remove(index_path)

  def PrepareHtmlReport(self):
    per_file_coverage_summary = self.GeneratePerFileCoverageSummary()

    if not self.no_file_view:
      self.GenerateFileViewHtmlIndexFile(per_file_coverage_summary,
                                         self.file_view_path)

    per_directory_coverage_summary = self.CalculatePerDirectoryCoverageSummary(
        per_file_coverage_summary)

    self.GeneratePerDirectoryCoverageInHtml(per_directory_coverage_summary,
                                            per_file_coverage_summary)

    self.GenerateDirectoryViewHtmlIndexFile()

    if not self.no_component_view:
      per_component_coverage_summary = (
          self.CalculatePerComponentCoverageSummary(
              per_directory_coverage_summary))
      self.GeneratePerComponentCoverageInHtml(per_component_coverage_summary,
                                              per_directory_coverage_summary)
      self.GenerateComponentViewHtmlIndexFile(per_component_coverage_summary)

    # The default index file is generated only for the list of source files,
    # needs to overwrite it to display per directory coverage view by default.
    self.OverwriteHtmlReportsIndexFile()
    self.CleanUpOutputDir()

    html_index_file_path = 'file://' + GetFullPath(self.html_index_path)
    logging.info('Index file for html report is generated as: "%s".',
                 html_index_file_path)


def ConfigureLogging(verbose=False, log_file=None):
  """Configures logging settings for later use."""
  log_level = logging.DEBUG if verbose else logging.INFO
  log_format = '[%(asctime)s %(levelname)s] %(message)s'
  logging.basicConfig(filename=log_file, level=log_level, format=log_format)


def GetComponentViewPath(output_dir):
  """Path to the HTML file for the component view."""
  return os.path.join(
      GetCoverageReportRootDirPath(output_dir), COMPONENT_VIEW_INDEX_FILE)


def GetCoverageReportRootDirPath(output_dir):
  """The root directory that contains all generated coverage html reports."""
  return os.path.join(output_dir, GetHostPlatform())


def GetHtmlFileRootDirPath(output_dir):
  """The directory that contains llvm-cov generated coverage html reports. """
  return os.path.join(output_dir, REPORT_DIR)


def GetDirectoryViewPath(output_dir):
  """Path to the HTML file for the directory view."""
  return os.path.join(
      GetCoverageReportRootDirPath(output_dir), DIRECTORY_VIEW_INDEX_FILE)


def GetFileViewPath(output_dir):
  """Path to the HTML file for the file view."""
  return os.path.join(
      GetCoverageReportRootDirPath(output_dir), FILE_VIEW_INDEX_FILE)


def GetHtmlIndexPath(output_dir):
  """Path to the main HTML index file."""
  return os.path.join(GetCoverageReportRootDirPath(output_dir), INDEX_HTML_FILE)


def GetFullPath(path):
  """Return full absolute path."""
  return os.path.abspath(os.path.expandvars(os.path.expanduser(path)))


def GetHostPlatform():
  """Returns the host platform.

  This is separate from the target platform/os that coverage is running for.
  """
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    return 'win'
  if sys.platform.startswith('linux'):
    return 'linux'
  else:
    assert sys.platform == 'darwin'
    return 'mac'


def GetRelativePathToDirectoryOfFile(target_path, base_path):
  """Returns a target path relative to the directory of base_path.

  This method requires base_path to be a file, otherwise, one should call
  os.path.relpath directly.
  """
  assert os.path.dirname(base_path) != base_path, (
      'Base path: "%s" is a directory, please call os.path.relpath directly.' %
      base_path)
  base_dir = os.path.dirname(base_path)
  return os.path.relpath(target_path, base_dir)


def GetSharedLibraries(binary_paths, build_dir, otool_path):
  """Returns list of shared libraries used by specified binaries."""
  logging.info('Finding shared libraries for targets (if any).')
  shared_libraries = []
  cmd = []
  shared_library_re = None

  if sys.platform.startswith('linux'):
    cmd.extend(['ldd'])
    shared_library_re = re.compile(r'.*\.so[.0-9]*\s=>\s(.*' + build_dir +
                                   r'.*\.so[.0-9]*)\s.*')
  elif sys.platform.startswith('darwin'):
    otool = otool_path if otool_path else 'otool'
    cmd.extend([otool, '-L'])
    shared_library_re = re.compile(r'\s+(@rpath/.*\.dylib)\s.*')
  else:
    assert False, 'Cannot detect shared libraries used by the given targets.'

  assert shared_library_re is not None

  cmd.extend(binary_paths)
  output = subprocess.check_output(cmd).decode('utf-8', 'ignore')

  for line in output.splitlines():
    m = shared_library_re.match(line)
    if not m:
      continue

    shared_library_path = m.group(1)
    if sys.platform.startswith('darwin'):
      # otool outputs "@rpath" macro instead of the dirname of the given binary.
      shared_library_path = shared_library_path.replace('@rpath', build_dir)

    if shared_library_path in shared_libraries:
      continue

    assert os.path.exists(shared_library_path), ('Shared library "%s" used by '
                                                 'the given target(s) does not '
                                                 'exist.' % shared_library_path)
    with open(shared_library_path, 'rb') as f:
      data = f.read()

    # Do not add non-instrumented libraries. Otherwise, llvm-cov errors outs.
    if b'__llvm_cov' in data:
      shared_libraries.append(shared_library_path)

  logging.debug('Found shared libraries (%d): %s.', len(shared_libraries),
                shared_libraries)
  logging.info('Finished finding shared libraries for targets.')
  return shared_libraries


def WriteRedirectHtmlFile(from_html_path, to_html_path):
  """Writes a html file that redirects to another html file."""
  to_html_relative_path = GetRelativePathToDirectoryOfFile(
      to_html_path, from_html_path)
  content = ("""
    <!DOCTYPE html>
    <html>
      <head>
        <!-- HTML meta refresh URL redirection -->
        <meta http-equiv="refresh" content="0; url=%s">
      </head>
    </html>""" % to_html_relative_path)
  with open(from_html_path, 'w') as f:
    f.write(content)


def _CmdSharedLibraries(args):
  """Handles 'shared_libs' command."""
  if not args.object:
    logging.error('No binaries are specified.')
    return 1

  library_paths = GetSharedLibraries(args.object, args.build_dir, None)
  if not library_paths:
    return 0

  # Print output in the format that can be passed to llvm-cov tool.
  output = ' '.join(
      '-object=%s' % os.path.normpath(path) for path in library_paths)
  print(output)
  return 0


def _CmdPostProcess(args):
  """Handles 'post_process' command."""
  with open(args.summary_file) as f:
    summary_data = f.read()

  processor = CoverageReportPostProcessor(
      args.output_dir,
      args.src_root_dir,
      summary_data,
      no_component_view=True,
      no_file_view=False,
      path_equivalence=args.path_equivalence)
  processor.PrepareHtmlReport()


def Main():
  parser = argparse.ArgumentParser(
      'coverage_utils', description='Code coverage utils.')
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Prints additional debug output.')

  subparsers = parser.add_subparsers(dest='command')

  shared_libs_parser = subparsers.add_parser(
      'shared_libs', help='Detect shared libraries.')
  shared_libs_parser.add_argument(
      '-build-dir', help='Path to the build dir.', required=True)
  shared_libs_parser.add_argument(
      '-object',
      action='append',
      help='Path to the binary using shared libs.',
      required=True)

  post_processing_parser = subparsers.add_parser(
      'post_process', help='Post process a report.')
  post_processing_parser.add_argument(
      '-output-dir', help='Path to the report dir.', required=True)
  post_processing_parser.add_argument(
      '-src-root-dir', help='Path to the src root dir.', required=True)
  post_processing_parser.add_argument(
      '-summary-file', help='Path to the summary file.', required=True)
  post_processing_parser.add_argument(
      '-path-equivalence',
      help='Map the paths in the coverage data to local '
      'source files path (=<from>,<to>)')

  args = parser.parse_args()
  ConfigureLogging(args.verbose)

  if args.command == 'shared_libs':
    return _CmdSharedLibraries(args)
  elif args.command == 'post_process':
    return _CmdPostProcess(args)
  else:
    parser.print_help(sys.stderr)


if __name__ == '__main__':
  sys.exit(Main())
