#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import io
import json
import pathlib
import re
import shutil
import sys
import urllib.request

_DEPENDENCY_DIVIDER = "-------------------- DEPENDENCY DIVIDER --------------------"
_HEADER_PATTERN = re.compile(r"^\w+:.*$")


class NoticeParsingException(Exception):
    pass


class ThirdPartyNoticeParser:
    def __init__(
        self,
        third_party_notices_path: pathlib.Path,
        readme_path: pathlib.Path,
        license_dir: pathlib.Path,
        template_path: pathlib.Path | None = None,
    ):
        self._third_party_notices_path = third_party_notices_path
        self._readme_path = readme_path
        self._license_dir = license_dir
        self._template_path = template_path
        self._seen_names = set()
        self._headers = {}
        self._readme_file: io.TextIOWrapper | None = None
        self._license_begin = 0
        self._license_end = 0

    def parse_and_append_notices(self):
        # If template is provided, initialize readme with template contents
        if self._template_path is not None and self._template_path.exists():
            shutil.copyfile(self._template_path, self._readme_path)

        # Remove the license files for each dependency
        if self._license_dir.exists():
            for license_path in self._license_dir.glob("LICENSE.*"):
                license_path.unlink()
        else:
            self._license_dir.mkdir(parents=True, exist_ok=True)

        with self._third_party_notices_path.open("r", encoding="utf-8") as notices_file:
            lines = notices_file.readlines()
        if len(lines) == 0:
            return

        lines = [line.rstrip() for line in lines]

        state = self._before_headers_state
        with self._readme_path.open("a", encoding="utf-8") as readme_file:
            self._readme_file = readme_file
            line_index = 0
            # Some value not less than the total state count
            state_count = 10
            # A guard intended to detect an infinite loop
            loop_variant = len(lines) * state_count
            while line_index < len(lines):
                if loop_variant <= 0:
                    raise NoticeParsingException(
                        "Internal error: no progress during the iteration"
                    )
                state, line_index = state(lines, line_index)
                loop_variant -= 1

            if state == self._read_license_state:
                state = self._print_license_state
                state(lines, line_index)

    def _before_headers_state(self, lines, line_index):
        line = lines[line_index]
        if line == "":
            return self._before_headers_state, line_index + 1

        self._headers = {}
        return self._read_headers_state, line_index

    def _read_headers_state(self, lines, line_index):
        line = lines[line_index]
        if line == "":
            self._license_begin = line_index + 1
            self._license_end = line_index + 1
            return self._read_license_state, line_index + 1

        if not _HEADER_PATTERN.match(line):
            raise NoticeParsingException(
                f'Expected a header field, got "{line.strip()}"'
            )
        header_key = line.split(":")[0]
        if header_key in self._headers:
            raise NoticeParsingException(f'Duplicated header: "{line.strip()}"')
        self._headers[header_key] = line_index
        return self._read_headers_state, line_index + 1

    def _read_license_state(self, lines, line_index):
        if lines[line_index] == _DEPENDENCY_DIVIDER:
            self._license_end = line_index
            return self._print_license_state, line_index + 1
        self._license_end = line_index + 1
        return self._read_license_state, line_index + 1

    def _print_license_state(self, lines, line_index):
        expected_headers = ["Name", "URL", "Version", "License"]
        for header in expected_headers:
            if header not in self._headers:
                raise NoticeParsingException(f'Expected header is missing: "{header}"')

        original_name_value = self._get_header_value("Name", lines)
        original_version_value = self._get_header_value("Version", lines)

        # Skip root package itself if present in notices
        if original_name_value == "chromium-bidi":
            return self._before_headers_state, line_index

        print("\n" + _DEPENDENCY_DIVIDER + "\n", file=self._readme_file)
        for header in expected_headers:
            print(lines[self._headers[header]], file=self._readme_file)
        print(
            "Revision: "
            + self._get_revision(original_name_value, original_version_value),
            file=self._readme_file,
        )
        print("Update Mechanism: Manual", file=self._readme_file)
        print("Security Critical: no", file=self._readme_file)
        print("Shipped: yes", file=self._readme_file)

        path_name_value = re.sub(r"[^\w_]", "_", original_name_value)
        name_value = path_name_value
        index = 0
        while name_value in self._seen_names:
            index += 1
            name_value = f"{path_name_value}_{index}"
        self._seen_names.add(name_value)
        license_path = self._license_dir.joinpath(f"LICENSE.{name_value}")
        relative_license_path = license_path.relative_to(self._readme_path.parent)
        print(f"License File: {relative_license_path}", file=self._readme_file)

        while (
            self._license_begin < self._license_end and lines[self._license_begin] == ""
        ):
            self._license_begin += 1
        while (
            self._license_begin < self._license_end
            and lines[self._license_end - 1] == ""
        ):
            self._license_end -= 1
        if self._license_begin == self._license_end:
            raise NoticeParsingException(
                f'License text is missing for dependency: "{original_name_value}"'
            )

        with license_path.open("w", encoding="utf-8") as license_file:
            for k in range(self._license_begin, self._license_end):
                print(lines[k], file=license_file)

        return self._before_headers_state, line_index

    def _get_header_value(self, name, lines):
        header_line = lines[self._headers[name]]
        return header_line[len(name + ":") :].strip()

    @staticmethod
    def _get_revision(name, version):
        """Get the revision of the package from the npm registry. Required, as the
        specific dependency revisions are required for the build. As long as the
        information about the specific revision is not available on the local
        npm package, this function fetches the revision from the npm registry.
        Falls back to `N/A` if the revision cannot be fetched for any reason.
        """
        try:
            registry_url = f"https://registry.npmjs.org/{name}/{version}"
            req = urllib.request.Request(
                registry_url, headers={"User-Agent": "Mozilla/5.0"}
            )
            with urllib.request.urlopen(req, timeout=10) as registry_response:
                registry_data = json.load(registry_response)
                return registry_data.get("gitHead") or "N/A"
        except Exception:
            return "N/A"


def find_default_notices_file(bidi_root: pathlib.Path) -> pathlib.Path | None:
    candidate_paths = [
        # Chromium build output in chromium repo
        bidi_root.parent.parent
        / "out"
        / "Default"
        / "gen"
        / "third_party"
        / "chromium-bidi"
        / "src"
        / "mapper_bundle_THIRD_PARTY_NOTICES",
        # Standalone build output inside chromium-bidi
        bidi_root
        / "out"
        / "Default"
        / "gen"
        / "src"
        / "mapper_bundle_THIRD_PARTY_NOTICES",
        # Fallback names
        bidi_root.parent.parent
        / "out"
        / "Default"
        / "gen"
        / "third_party"
        / "chromium-bidi"
        / "src"
        / "THIRD_PARTY_NOTICES",
        bidi_root / "out" / "Default" / "gen" / "src" / "THIRD_PARTY_NOTICES",
    ]
    for path in candidate_paths:
        if path.exists():
            return path
    return None


def main():
    script_dir = pathlib.Path(__file__).resolve().parent
    bidi_root = script_dir.parent

    default_template = bidi_root / "README.chromium.in"
    default_readme = bidi_root / "README.chromium"
    default_license_dir = bidi_root / "licenses"
    default_notices = find_default_notices_file(bidi_root)

    parser = argparse.ArgumentParser(
        description="Generate README.chromium and license files from build third-party notices"
    )
    parser.add_argument(
        "--third-party-notices",
        type=pathlib.Path,
        default=default_notices,
        help="Path to third party notices file (e.g. mapper_bundle_THIRD_PARTY_NOTICES)",
    )
    parser.add_argument(
        "--template",
        type=pathlib.Path,
        default=default_template,
        help="Path to README.chromium.in template",
    )
    parser.add_argument(
        "--readme",
        type=pathlib.Path,
        default=default_readme,
        help="Path to output README.chromium",
    )
    parser.add_argument(
        "--license-dir",
        type=pathlib.Path,
        default=default_license_dir,
        help="Directory for third party license files",
    )
    options = parser.parse_args()

    if options.third_party_notices is None or not options.third_party_notices.exists():
        parser.error(
            f"Third party notices file not found: {options.third_party_notices}.\n"
            "Please ensure chromium-bidi has been built (e.g. `npm run build` or `autoninja`)."
        )

    if options.template is not None and not options.template.exists():
        parser.error(f"Template file not found: {options.template}")

    if not options.license_dir.exists():
        try:
            options.license_dir.mkdir(parents=True, exist_ok=True)
        except Exception as ex:
            parser.error(f"Unable to create directory: {options.license_dir}: {ex}")

    try:
        notice_parser = ThirdPartyNoticeParser(
            options.third_party_notices,
            readme_path=options.readme,
            license_dir=options.license_dir,
            template_path=options.template,
        )
        notice_parser.parse_and_append_notices()
    except Exception as ex:
        sys.stderr.write(f"Failed to append notices: {ex}\n")
        return 1

    print(f"Successfully updated {options.readme} and {options.license_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
