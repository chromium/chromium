# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Model of a structured metrics description xml file.

This marshals an XML string into a Model, and validates that the XML is
semantically correct. The model can also be used to create a canonically
formatted version XML.
"""

import textwrap as tw
import xml.etree.ElementTree as ET
import re

import sync.model_util as util


# Default key rotation period if not explicitly specified in the XML.
DEFAULT_KEY_ROTATION_PERIOD = 90

# Default scope if not explicitly specified in the XML.
DEFAULT_PROJECT_SCOPE = "device"


def wrap(text: str, indent: str) -> str:
  wrapper = tw.TextWrapper(width=80,
                           initial_indent=indent,
                           subsequent_indent=indent)
  return wrapper.fill(tw.dedent(text))


class Model:
  """Represents all projects in the structured.xml file.

    A Model is initialized with an XML string representing the top-level of
    the structured.xml file. This file is built from three building blocks:
    projects, events, and metrics. These have the following attributes.

      PROJECT
      - summary
      - id specifier
      - (optional) one or more targets. If undefined, defaults to 'chromium'
      - one or more owners
      - one or more events

      EVENT
      - summary
      - one or more metrics

      METRIC
      - summary
      - data type

    The following is an example input XML.

      <structured-metrics>
      <project name="MyProject" targets="chromium">
        <owner>owner@chromium.org</owner>
        <id>none</id>
        <scope>profile</scope>
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

  OWNER_REGEX = r"^.+@(chromium\.org|google\.com)$"
  NAME_REGEX = r"^[A-Za-z0-9_.]+$"
  VARIANT_NAME_REGEX = r"^[A-Z0-9_.]+$"
  TYPE_REGEX = r"^(hmac-string|raw-string|int|double|int-array)$"
  ID_REGEX = r"^(none|per-project|uma)$"
  SCOPE_REGEX = r"^(profile|device)$"
  KEY_REGEX = r"^[0-9]+$"
  MAX_REGEX = r"^[0-9]+$"
  TARGET_REGEX = r"^(chromium|webui)$"

  def __init__(self, xml_string: str, platform: str):
    elem = ET.fromstring(xml_string)
    util.check_attributes(elem, set())
    util.check_children(elem, {"project"})
    util.check_child_names_unique(elem, "project")
    projects = util.get_compound_children(elem, "project")
    self.projects = [Project(p, platform) for p in projects]

  def __repr__(self):
    projects = "\n\n".join(str(p) for p in self.projects)

    return f"""\
<structured-metrics>

{projects}

</structured-metrics>"""


def merge_models(primary: Model, other: Model) -> Model:
  """Merges two models into one."""
  primary.projects += [
      p for p in other.projects if not re.match("Test", p.name)
  ]
  return primary


class Project:
  """Represents a single structured metrics project.

    A Project is initialized with an XML node representing one project, eg:

      <project name="MyProject" cros_events="true" targets="webui,chromium">
        <owner>owner@chromium.org</owner>
        <id>none</id>
        <scope>project</scope>
        <key-rotation>60</key-rotation>
        <summary> My project. </summary>

        <enum name="Enum1">
          <variant value="1">VARIANT1</variant>
          <variant value="2">VARIANT2</variant>
          <variant value="5">VARIANT3</variant>
        </enum>

        <event name="MyEvent">
          <summary> My event. </summary>
          <metric name="MyMetric" type="int">
            <summary> My metric. </summary>
          </metric>
        </event>
      </project>

    Calling str(project) will return a canonically formatted XML string.
    """

  def __init__(self, elem: ET.Element, platform: str):
    util.check_attributes(elem, {"name"}, {"cros_events", "targets"})
    util.check_children(elem, {"id", "summary", "owner", "event"}, {"enum"})
    util.check_child_names_unique(elem, "event")

    self.name = util.get_attr(elem, "name", Model.NAME_REGEX)
    self.id = util.get_text_child(elem, "id", Model.ID_REGEX)
    self.summary = util.get_text_child(elem, "summary")
    self.owners = util.get_text_children(elem, "owner", Model.OWNER_REGEX)
    self.platform = platform

    self.key_rotation_period = DEFAULT_KEY_ROTATION_PERIOD
    self.scope = DEFAULT_PROJECT_SCOPE
    self.is_event_sequence_project = util.get_boolean_attr(elem, "cros_events")

    # Check if key-rotation is specified. If so, then change the
    # key_rotation_period.
    if elem.find("key-rotation") is not None:
      self.key_rotation_period = util.get_text_child(elem, "key-rotation",
                                                     Model.KEY_REGEX)

    # enums need to be populated first because they are used for validation
    util.check_child_names_unique(elem, "enum")
    self.enums = [
        Enum(e, self) for e in util.get_compound_children(elem, "enum", True)
    ]

    if "targets" in elem.attrib:
      self.targets = set(
          util.get_optional_attr_list(elem, "targets", Model.TARGET_REGEX))
    else:
      self.targets = set()

    # Check if scope is specified. If so, then change the scope.
    if elem.find("scope") is not None:
      self.scope = util.get_text_child(elem, "scope", Model.SCOPE_REGEX)

    self.events = [
        Event(e, self) for e in util.get_compound_children(elem, "event")
    ]

  def has_enum(self, enum_name: str) -> bool:
    enum_names = [e.name for e in self.enums]
    return enum_name in enum_names

  def __repr__(self):
    events = "\n\n".join(str(e) for e in self.events)
    events = tw.indent(events, "  ")
    summary = wrap(self.summary, indent="    ")
    owners = "\n".join(f"  <owner>{o}</owner>" for o in self.owners)
    if self.is_event_sequence_project:
      cros_events_attr = ' cros_events="true"'
    else:
      cros_events_attr = ""
    if self.targets:
      targets = ' targets="' + ",".join(self.targets) + '"'
    else:
      targets = ""

    enums = "\n\n".join(str(v) for v in self.enums)
    enums = tw.indent(enums, "  ")

    return f"""\
<project name="{self.name}"{cros_events_attr}{targets}>
{owners}
  <id>{self.id}</id>
  <scope>{self.scope}</scope>
  <key-rotation>{self.key_rotation_period}</key-rotation>
  <summary>
{summary}
  </summary>
{enums}
{events}
</project>"""


class Enum:
  """Represents an enum value for a project.

    An Enum is initialized with an XML node representing one enum, eg:

    <enum name="EnumName">
      <variant value="1">Name1</variant>
      <variant value="2">Name2</variant>
      <variant value="5">Name3</variant>
    </enum>

    Calling str(enum) will return a canonically formatted XML string.
    """

  def __init__(self, elem: ET.Element, project: Project):
    self.project = project
    util.check_attributes(elem, {"name"})

    util.check_children(elem, {"variant"})

    self.name = util.get_attr(elem, "name", Model.NAME_REGEX)
    self.variants = [
        Variant(e, self)
        for e in util.get_compound_children(elem, "variant", allow_text=True)
    ]
    variant_names = [v.name for v in self.variants]
    util.check_names_unique(elem, variant_names, "variant")

  def __repr__(self):
    variants = '\n'.join(str(v) for v in self.variants)
    variants = tw.indent(variants, "  ")
    return f"""\
<enum name="{self.name}">
{variants}
</enum>"""


class Variant:
  """Represents an element of an Enum.

    <variant value="1">Name1</variant>

    Calling str(variant) will return a canonically formatted XML string.
    """

  def __init__(self, elem: ET.Element, enum: Enum):
    util.check_attributes(elem, {"value"})
    self.name = util.get_text(elem, Model.VARIANT_NAME_REGEX)
    self.value = util.get_attr(elem, "value")
    self.enum = enum

  def __repr__(self):
    return f'<variant value="{self.value}">{self.name}</variant>'


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

  def __init__(self, elem: ET.Element, project: Project):
    util.check_attributes(elem, {"name"}, {"force_record"})

    if project.is_event_sequence_project:
      expected_children = {"summary"}
    else:
      expected_children = {"summary", "metric"}

    util.check_children(elem, expected_children)

    util.check_child_names_unique(elem, "metric")

    self.name = util.get_attr(elem, "name", Model.NAME_REGEX)
    self.force_record = util.get_boolean_attr(elem, "force_record")
    self.summary = util.get_text_child(elem, "summary")
    self.metrics = [
        Metric(m, project) for m in util.get_compound_children(
            elem, "metric", project.is_event_sequence_project)
    ]

  def __repr__(self):
    metrics = "\n".join(str(m) for m in self.metrics)
    metrics = tw.indent(metrics, "  ")
    summary = wrap(self.summary, indent="    ")
    if self.force_record:
      force_record = ' force_record="true"'
    else:
      force_record = ""

    return f"""\
<event name="{self.name}"{force_record}>
  <summary>
{summary}
  </summary>
{metrics}
</event>"""


class Metric:
  """Represents a single metric.

    A Metric is initialized with an XML node representing one metric, eg:

      <metric name="MyMetric" type="int">
        <summary> My metric. </summary>
      </metric>

    Calling str(metric) will return a canonically formatted XML string.
    """

  def __init__(self, elem: ET.Element, project: Project):
    util.check_attributes(elem, {"name", "type"}, {"max"})
    util.check_children(elem, {"summary"})

    self.name = util.get_attr(elem, "name", Model.NAME_REGEX)

    self.type = util.get_attr(elem, "type")
    # If the type isn't an enum then check it it must be a builtin type.
    if project.has_enum(self.type):
      self.is_enum = True
    else:
      self.is_enum = False
      self.type = util.get_attr(elem, "type", Model.TYPE_REGEX)

    self.summary = util.get_text_child(elem, "summary")

    if self.type == "int-array":
      self.max_size = int(util.get_attr(elem, "max", Model.MAX_REGEX))

    if self.type == "raw-string" and (project.id != "none" and
                                      not project.is_event_sequence_project):
      util.error(
          elem,
          "raw-string metrics must be in a project with id type "
          f"'none' or sequenced project, but {project.name} has "
          f"id type '{project.id}'",
      )

  def is_array(self) -> bool:
    return "array" in self.type

  def __repr__(self):
    summary = wrap(self.summary, indent="    ")
    return f"""\
<metric name="{self.name}" type="{self.type}">
  <summary>
{summary}
  </summary>
</metric>"""
