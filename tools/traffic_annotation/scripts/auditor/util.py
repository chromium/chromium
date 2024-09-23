# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import platform
import re
import sys

from functools import reduce
from itertools import chain
from google.protobuf import text_format
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message
from pathlib import Path
from typing import NewType, Any, Optional, List, Iterable

UniqueId = NewType("UniqueId", str)
HashCode = NewType("HashCode", int)

# Configure logging with timestamp, log level, filename, and line number.
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s:%(levelname)s:%(filename)s(%(lineno)d)] %(message)s")
logger = logging.getLogger(__name__)


def import_compiled_proto(build_path) -> Any:
  """Global import from function. |self.build_path| is needed to perform
  this import, hence why it's not a top-level import.

  The compiled proto is located ${build_path}/pyproto/ and generated as a part
  of compiling Chrome."""
  # Use the build path to import the compiled traffic annotation proto.
  proto_path = build_path / "pyproto" / "chrome" / "browser" / "privacy"
  sys.path.insert(0, str(proto_path))

  try:
    global traffic_annotation_pb2
    global traffic_annotation
    import traffic_annotation_pb2
    # Used for accessing enum constants.
    from traffic_annotation_pb2 import NetworkTrafficAnnotation as \
      traffic_annotation
    return traffic_annotation_pb2
  except ImportError as e:
    logger.critical(
      "Failed to import the compiled traffic annotation proto. Make sure "
      "you're on Linux or Windows and Chrome is built in '{}' before "
      "running this script.".format(build_path))
    raise


def get_current_platform(build_path: Optional[Path] = None) -> str:
  """Return the target platform of |build_path| based on heuristics."""
  # Use host platform as the source of truth (in most cases).
  current_platform: str = platform.system().lower()

  if current_platform == "linux" and build_path is not None:
    # It could be an Android build directory, being compiled from a Linux host.
    # Look for a target_os="android" line in args.gn.
    try:
      gn_args = (build_path / "args.gn").read_text(encoding="utf-8")
      pattern = re.compile(r"^\s*target_os\s*=\s*\"(android|chromeos)\"\s*$",
                           re.MULTILINE)
      match = pattern.search(gn_args)
      if match:
        current_platform = match.group(1)

    except (ValueError, OSError) as e:
      logger.info(e)
      # Maybe the file's absent, or it can't be decoded as UTF-8, or something.
      # It's probably not Android/ChromeOS in that case.
      pass

  return current_platform


def twos_complement_8bit(b: int) -> int:
  """Interprets b like a signed 8-bit integer, possibly changing its sign.

  For instance, twos_complement_8bit(204) returns -52."""
  if b >= 256:
    raise ValueError("b must fit inside 8 bits")
  if b & (1 << 7):
    # Negative number, calculate its value using two's-complement.
    return b - (1 << 8)
  else:
    # Positive number, do not touch.
    return b


def iterative_hash(s: str) -> HashCode:
  """Compute the has code of the given string as in:
  net/traffic_annotation/network_traffic_annotation.h

  Args:
    s: str
      The seed, e.g. unique id of traffic annotation.
  Returns: int
    A hash code.
  """
  return HashCode(
      reduce(lambda acc, b: (acc * 31 + twos_complement_8bit(b)) % 138003713,
             s.encode("utf-8"), 0))


def compute_hash_value(text: str) -> HashCode:
  """Same as iterative_hash, but returns -1 for empty strings."""
  return iterative_hash(text) if text else HashCode(-1)


def merge_string_field(src: Message, dst: Message, field: str):
  """Merges the content of one string field into an annotation."""
  if getattr(src, field):
    if getattr(dst, field):
      setattr(dst, field, "{}\n{}".format(getattr(src, field),
                                          getattr(dst, field)))
    else:
      setattr(dst, field, getattr(src, field))


def fill_proto_with_bogus(unique_id: str, proto: Message,
                          field_numbers: List[int]):
  """Fill proto with bogus values for the fields identified by field_numbers.
  Uses reflection to fill the proto with the right types."""
  descriptor = proto.DESCRIPTOR
  for field_number in field_numbers:
    field_number = abs(field_number)

    if field_number not in descriptor.fields_by_number:
      raise ValueError("{} is not a valid {} field".format(
          field_number, descriptor.name))

    field = descriptor.fields_by_number[field_number]
    repeated = field.label == FieldDescriptor.LABEL_REPEATED

    if field.type == FieldDescriptor.TYPE_STRING and not repeated:
      setattr(proto, field.name, "[Archived]")
    elif field.type == FieldDescriptor.TYPE_ENUM and not repeated:
      # Assume the 2nd value in the enum is reasonable, since the 1st is
      # UNSPECIFIED.
      setattr(proto, field.name, field.enum_type.values[1].number)
    elif field.type == FieldDescriptor.TYPE_MESSAGE and repeated:
      getattr(proto, field.name).add()
    elif field.type == FieldDescriptor.TYPE_MESSAGE:
      # Non-repeated message, nothing to do.
      pass
    else:
      raise NotImplementedError(
          "Unimplemented proto field {} of type {} ({}) in {}".format(
              field.name, field.type,
              "repeated" if repeated else "non-repeated", unique_id))


def extract_annotation_id(line: str) -> Optional[UniqueId]:
  """Returns the annotation id given an '<item id=...' line"""
  m = re.search('id="([^"]+)"', line)
  return UniqueId(m.group(1)) if m else None


def escape_for_tsv(text: str) -> str:
  """Changes double-quotes to single-quotes, and adds double-quotes around the
  text if it has newlines/tabs."""
  text.replace("\"", "'")
  if "\n" in text or "\t" in text:
    return "\"{}\"".format(text)
  return text


def policy_to_text(chrome_policy: Iterable[Message]) -> str:
  """Unnests the policy name/values from chrome_policy, producing a
  human-readable string.

  For example, this:
    chrome_policy {
      SyncDisabled {
        policy_options {
          mode: MANDATORY
        }
        SyncDisabled: true
      }
    }

  becomes this:
    SyncDisabled: true"""
  items = []
  # Use the protobuf serializer library to print the fields, 2 levels deep.
  for policy in chrome_policy:
    for field, value in policy.ListFields():
      for subfield, subvalue in value.ListFields():
        if subfield.name == "policy_options":
          # Skip the policy_options field.
          continue
        writer = text_format.TextWriter(as_utf8=True)
        if subfield.label == FieldDescriptor.LABEL_REPEATED:
          # text_format.PrintField needs repeated fields passed in
          # one-at-a-time.
          for repeated_value in subvalue:
            text_format.PrintField(subfield,
                                   repeated_value,
                                   writer,
                                   as_one_line=True,
                                   use_short_repeated_primitives=True)
        else:
          text_format.PrintField(subfield,
                                 subvalue,
                                 writer,
                                 as_one_line=True,
                                 use_short_repeated_primitives=True)
        items.append(writer.getvalue().strip())
  # We wrote an extra comma at the end, remove it before returning.
  return ", ".join(items)
  return re.sub(r", $", "", writer.getvalue()).strip()


def write_annotations_tsv_file(file_path: Path, annotations: List["Annotation"],
                               missing_ids: List[UniqueId]):
  """Writes a TSV file of all annotations and their contents in file_path."""
  logger.info("Saving annotations to TSV file: {}.".format(file_path))
  Destination = traffic_annotation.TrafficSemantics.Destination
  CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed

  lines = []
  title = "Unique ID\tLast Update\tSender\tDescription\tTrigger\tData\t" + \
  "Destination\tCookies Allowed\tCookies Store\tSetting\tChrome Policy\t" + \
  "Comments\tSource File"

  column_count = title.count("\t")
  for missing_id in missing_ids:
    lines.append(missing_id + "\t" * column_count)

  for annotation in annotations:
    if annotation.type.value != "definition":
      continue

    # TODO(nicolaso): Use StringIO for faster concatenation.

    line = annotation.proto.unique_id
    # Placeholder for Last Update Date, will be updated in the scripts.
    line += "\t"

    # Semantics.
    semantics = annotation.proto.semantics
    semantics_list = [
        semantics.sender,
        escape_for_tsv(semantics.description),
        escape_for_tsv(semantics.trigger),
        escape_for_tsv(semantics.data),
    ]

    for semantic_info in semantics_list:
      line += "\t{}".format(semantic_info)

    destination_names = {
        Destination.WEBSITE: "Website",
        Destination.GOOGLE_OWNED_SERVICE: "Google",
        Destination.LOCAL: "Local",
        Destination.PROXIED_GOOGLE_OWNED_SERVICE: "Proxied to Google",
        Destination.OTHER: "Other",
    }
    if (semantics.destination == Destination.OTHER
        and semantics.destination_other):
      line += "\tOther: {}".format(semantics.destination_other)
    elif semantics.destination in destination_names:
      line += "\t{}".format(destination_names[semantics.destination])
    else:
      raise ValueError(
          "Invalid value for the semantics.destination field: {}".format(
              semantics.destination))

    # Policy.
    policy = annotation.proto.policy
    if annotation.proto.policy.cookies_allowed == CookiesAllowed.YES:
      line += "\tYes"
    else:
      line += "\tNo"

    line += "\t{}".format(escape_for_tsv(policy.cookies_store))
    line += "\t{}".format(escape_for_tsv(policy.setting))

    # Chrome policies.
    if annotation.has_policy():
      policies_text = policy_to_text(
          chain(policy.chrome_policy, policy.chrome_device_policy))
    else:
      policies_text = policy.policy_exception_justification
    line += "\t{}".format(escape_for_tsv(policies_text))

    # Comments.
    line += "\t{}".format(escape_for_tsv(annotation.proto.comments))
    # Source.
    source = annotation.proto.source
    code_search_link = "https://cs.chromium.org/chromium/src/"
    line += "\t{}{}?l={}".format(code_search_link, source.file, source.line)
    lines.append(line)

  lines.sort()
  lines.insert(0, title)
  report = "\n".join(lines) + "\n"

  file_path.write_text(report, encoding="utf-8")
