#!/bin/bash

(
    retrospective-crate-version-tagging detect \
        --crate-name typed-builder \
        --changelog-path CHANGELOG.md \
        --tag-prefix v \
) | retrospective-crate-version-tagging create-releases
