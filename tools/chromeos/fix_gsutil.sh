#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Fixes gsutil authentication issues for ChromeOS tools.
# See b/467778582#comment36 for details.

set -u

readonly LDAP="${USER:-$(id -un)}"
readonly GCLOUD_LEGACY_DIR="${HOME}/.config/gcloud/legacy_credentials"
readonly BOTO_SRC="${GCLOUD_LEGACY_DIR}/${LDAP}@google.com/.boto"
readonly BOTO_DST="${HOME}/.boto"

REMOVE_BOTO_APPROVED=false

function die {
  echo "Error: $*" >&2
  exit 1
}

function confirm {
  local reply
  read -r -p "$1 (y/N): " reply && [[ "${reply}" =~ ^[yY]$ ]]
}

function has_gui_display {
  if [ -n "${WAYLAND_DISPLAY:-}" ]; then
    local wl_sock="${WAYLAND_DISPLAY}"
    if [[ "${wl_sock}" != /* ]]; then
      wl_sock="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/${wl_sock}"
    fi
    [ -S "${wl_sock}" ] && return 0
  fi

  if [ -n "${DISPLAY:-}" ]; then
    if command -v xdpyinfo >/dev/null 2>&1; then
      local dpy_info
      if dpy_info="$(xdpyinfo -ext XInputExtension 2>/dev/null)"; then
        if [[ "${dpy_info}" != *'"Xvfb '* ]] || \
           [ "${CHROME_REMOTE_DESKTOP_SESSION:-}" = "1" ]; then
          return 0
        fi
      fi
    elif command -v xset >/dev/null 2>&1; then
      xset q >/dev/null 2>&1 && return 0
    else
      return 0
    fi
  fi

  return 1
}

function check_boto_target {
  if [ -e "${BOTO_DST}" ] && [ ! -L "${BOTO_DST}" ] && \
     [ ! -f "${BOTO_DST}" ]; then
    die "${BOTO_DST} cannot be replaced (not a regular file or symlink)."
  fi
  if [ ! -w "${HOME}" ]; then
    die "${HOME} is not writable; cannot update ${BOTO_DST}."
  fi
  if [ -f "${BOTO_DST}" ] && [ ! -L "${BOTO_DST}" ] && \
     [ "${REMOVE_BOTO_APPROVED}" = false ]; then
    echo "${BOTO_DST} already exists and is not a symlink."
    if ! confirm "Is it OK to remove ${BOTO_DST}?"; then
      die "Aborted: ${BOTO_DST} was not removed."
    fi
    REMOVE_BOTO_APPROVED=true
  fi
}

if ! command -v cipd >/dev/null 2>&1; then
  die "'cipd' not found in PATH (ensure depot_tools is in PATH)."
fi

cat <<EOF
File an exemption at http://go/gcloud-cba-exemption.
Click:
  "gclient sync and cros flash fail with gsutil auth errors (b/467778582)"
and submit. Wait until the page shows (${LDAP} is currently a member).

EOF

if ! confirm "Proceed?"; then
  echo "Aborted."
  exit 1
fi

check_boto_target

if ! command -v gcloud >/dev/null 2>&1; then
  echo "Download Google Cloud SDK: sudo apt install -y google-cloud-cli"
  if ! sudo apt install -y google-cloud-cli; then
    die "Failed to install google-cloud-cli."
  fi
fi

if ! has_gui_display; then
  die "A real GUI session (e.g. Chrome Remote Desktop, not remote SSH or" \
      "Xvfb) is required because 'cipd auth-login' opens a browser."
fi

echo "Running: cipd auth-login"
if ! cipd auth-login; then
  die "'cipd auth-login' failed. Skipping ${BOTO_DST} update."
fi

echo "Running: gcloud auth login ${LDAP}@google.com --force"
if ! gcloud auth login "${LDAP}@google.com" --force; then
  die "'gcloud auth login' failed. Skipping ${BOTO_DST} update."
fi

if [ ! -f "${BOTO_SRC}" ]; then
  die "Credentials file ${BOTO_SRC} not found after login" \
      "(ensure you logged in as ${LDAP}@google.com)."
fi

check_boto_target

echo "Linking ${BOTO_SRC} -> ${BOTO_DST}"
if ! ln -sfT "${BOTO_SRC}" "${BOTO_DST}"; then
  die "Failed to link ${BOTO_SRC} to ${BOTO_DST}."
fi

echo "Done."
