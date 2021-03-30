# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Model of a structured metrics description xml file.

This marshals an XML string into a Model, and validates that the XML is
semantically correct. The model can also be used to create a canonically
formatted version XML.
"""

import xml.etree.ElementTree as ET
import textwrap as tw
import model_util as util


def wrap(text, indent):
  wrapper = tw.TextWrapper(width=80,
                           initial_indent=indent,
                           subsequent_indent=indent)
  return wrapper.fill(tw.dedent(text))


# TODO(crbug.com/1148168): This can be removed and replaced with textwrap.indent
# once this is run under python3.
def indent(text, prefix):
  return '\n'.join(prefix + line if line else '' for line in text.split('\n'))


class Model:
  """Represents all projects in the structured.xml file.

  A Model is initialized with an XML string representing the top-level of
  the structured.xml file. This file is built from three building blocks:
  metrics, events, and projects. These have the following attributes.

    METRIC
    - summary
    - data type

    EVENT
    - summary
    - one or more metrics

    PROJECT
    - summary
    - id specifier
    - one or more owners
    - one or more events

  The following is an example input XML.

    <structured-metrics>
    <project name="MyProject">
      <owner>owner@chromium.org</owner>
      <id>none</id>
      <summary> My project. </summary>

      <event name="MyEvent">
        <summary> My event. </summary>
        <metric name="MyMetric" type="int">
          <summary> My metric. </summary>
        </metric>
      </event>
    </project>
    </structured-metrics>

  Calling str(model) will return a canonically formatted XML string.
  """

  OWNER_REGEX = r'^.+@(chromium\.org|google\.com)$'
  NAME_REGEX = r'^[A-Za-z0-9_.]+$'
  TYPE_REGEX = r'^(hmac-string|int)$'
  ID_REGEX = r'^(none|per-project|uma)$'

  def __init__(self, xml_string):
    elem = ET.fromstring(xml_string)
    util.check_attributes(elem, set())
    util.check_children(elem, {'project'})
    util.check_child_names_unique(elem, 'project')

    projects = util.get_compound_children(elem, 'project')
    self.projects = [Project(p) for p in projects]

  def __repr__(self):
    projects = '\n\n'.join(str(p) for p in self.projects)

    result = tw.dedent("""\
               <structured-metrics>

               {projects}

               </structured-metrics>""")
    return result.format(projects=projects)


class Project:
  """Represents a single structured metrics project.

  A Project is initialized with an XML node representing one project, eg:

    <project name="MyProject">
      <owner>owner@chromium.org</owner>
      <id>none</id>
      <summary> My project. </summary>

      <event name="MyEvent">
        <summary> My event. </summary>
        <metric name="MyMetric" type="int">
          <summary> My metric. </summary>
        </metric>
      </event>
    </project>

  Calling str(project) will return a canonically formatted XML string.
  """

  def __init__(self, elem):
    util.check_attributes(elem, {'name'})
    util.check_children(elem, {'id', 'summary', 'owner', 'event'})
    util.check_child_names_unique(elem, 'event')

    self.name = util.get_attr(elem, 'name', Model.NAME_REGEX)
    self.id = util.get_text_child(elem, 'id', Model.ID_REGEX)
    self.summary = util.get_text_child(elem, 'summary')
    self.owners = util.get_text_children(elem, 'owner', Model.OWNER_REGEX)

    self.events = [Event(e) for e in util.get_compound_children(elem, 'event')]

  def __repr__(self):
    events = '\n\n'.join(str(e) for e in self.events)
    events = indent(events, '  ')
    summary = wrap(self.summary, indent='    ')
    owners = '\n'.join('  <owner>{}</owner>'.format(o) for o in self.owners)

    result = tw.dedent("""\
               <project name="{name}">
               {owners}
                 <id>{id}</id>
                 <summary>
               {summary}
                 </summary>

               {events}
               </project>""")
    return result.format(name=self.name,
                         owners=owners,
                         id=self.id,
                         summary=summary,
                         events=events)


class Event:
  """Represents a single structured metrics event.

  An Event is initialized with an XML node representing one event, eg:

    <event name="MyEvent">
      <summary> My event. </summary>
      <metric name="MyMetric" type="int">
        <summary> My metric. </summary>
      </metric>
    </event>

  Calling str(event) will return a canonically formatted XML string.
  """

  def __init__(self, elem):
    util.check_attributes(elem, {'name'})
    util.check_children(elem, {'summary', 'metric'})
    util.check_child_names_unique(elem, 'metric')

    self.name = util.get_attr(elem, 'name', Model.NAME_REGEX)
    self.summary = util.get_text_child(elem, 'summary')
    self.metrics = [
        Metric(m) for m in util.get_compound_children(elem, 'metric')
    ]

  def __repr__(self):
    metrics = '\n'.join(str(m) for m in self.metrics)
    metrics = indent(metrics, '  ')
    summary = wrap(self.summary, indent='    ')
    result = tw.dedent("""\
               <event name="{name}">
                 <summary>
               {summary}
                 </summary>
               {metrics}
               </event>""")
    return result.format(name=self.name, summary=summary, metrics=metrics)


class Metric:
  """Represents a single metric.

  A Metric is initialized with an XML node representing one metric, eg:

    <metric name="MyMetric" type="int">
      <summary> My metric. </summary>
    </metric>

  Calling str(metric) will return a canonically formatted XML string.
  """

  def __init__(self, elem):
    util.check_attributes(elem, {'name', 'type'})
    util.check_children(elem, {'summary'})

    self.name = util.get_attr(elem, 'name', Model.NAME_REGEX)
    self.type = util.get_attr(elem, 'type', Model.TYPE_REGEX)
    self.summary = util.get_text_child(elem, 'summary')

  def __repr__(self):
    summary = wrap(self.summary, indent='    ')
    result = tw.dedent("""\
               <metric name="{name}" type="{type}">
                 <summary>
               {summary}
                 </summary>
               </metric>""")
    return result.format(name=self.name, type=self.type, summary=summary)
