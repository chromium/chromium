#!/bin/bash -e
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A shell script to run chrome for chromeos on linux desktop environment.

# Find a src directory of the repository.
function find_src_root {
  local real_dir=`dirname $(realpath $0)`
  local src_dir=`dirname $(dirname $real_dir)`
  # make sure that the src directory was correctly identified.
  local gclient_file="$src_dir/../.gclient"
  if [ ! -f $gclient_file ]; then
    echo "Unable to find a src directory."
    exit
  fi
  echo $src_dir
}

CHROME_SRC_ROOT=$(find_src_root)

USER_TMP_DIR=${HOME}/tmp
USER_DATA_DIR=${USER_TMP_DIR}/ash-chrome-user-data-dir

# You may set these env vars to match your working environment
ASH_CHROME_BUILD_DIR=${ASH_CHROME_BUILD_DIR:-${CHROME_SRC_ROOT}/out/Release}

# Display Configurations
declare -A DISPLAY_RES=(
[wxga]=1280x800
[fwxga]=1366x768
[hdp]=1600x900*1.25
[fhd]=1920x1080*1.25
[wuxga]=1920x1200*1.6
[qhd]=2560x1440*2
[qhdp]=3200x1800*2
[f4k]=3840x2160*2.6666666
[slate]=3000x2000*2.25225234
)

# Custom display configs is possible
#DISPLAY_CONFIG=1280x800
#DISPLAY_CONFIG=1366x768
#DISPLAY_CONFIG=1920x1080*1.25
#DISPLAY_CONFIG=2360x1700*2
#DISPLAY_CONFIG=3000x2000*2.25
#DISPLAY_CONFIG=3840*2160*2.66
# multi display example
#DISPLAY_CONFIG=1200x800,1200+0-1000x800

# Use WXGA as default panel.
DISPLAY_CONFIG=${DISPLAY_RES[wxga]}

FEATURES=

export XDG_RUNTIME_DIR=${USER_TMP_DIR}/xdg1

# Check if the directory contains chrome binary
function check_chrome_dir {
  local directory="$1"
  local flag="$2"
  if [ ! -d "$directory" ]; then
    exec cat << EOL
The directory '$directory' does not exit. Please set using --${flag}=<dir>.
EOL
    exit
  fi
  if [ ! -f "$directory/chrome" ]; then
    echo "The chrome binary '$directory/chrome' does not exist"
    exit
  fi
}

function ensure_user_dir {
  local dir=$1
  local name=$2
  if [ ! -d ${dir} ]; then
    echo "The user data directory '${dir} for ${name} does not exit".
    read -p "Do you want to create? (y/n) "
    [ "$REPLY" == "y" ] && mkdir -p ${dir}
    [ "$REPLY" == "y" ] || exit
  fi
}

# Build command arguments
function build_args {
  local guest_mode=$1
  local login_args
  if $guest_mode; then
    login_args="--login-profile=user --bwsi --incognito --login-user=\$guest"
  else
    login_args="--login-manager  --login-profile=user"
  fi

  ARGS="--user-data-dir=${USER_DATA_DIR} \
    --enable-wayland-server --ash-debug-shortcuts --overview-button-for-tests \
    --enable-ui-devtools --ash-dev-shortcuts \
    --ash-host-window-bounds=${DISPLAY_CONFIG} \
    --enable-features=${FEATURES} \
    ${login_args} \
    ${TOUCH_DEVICE_OPTION} \
    ${EXTRA_ARGS}"

  # To enable internal display.
  ARGS="${ARGS} --use-first-display-as-internal"
}

# Start ash chrome binary.
function start_ash_chrome {
  local guest_mode=$1
  build_args $guest_mode

  check_chrome_dir "$ASH_CHROME_BUILD_DIR" ash-chrome-build-dir
  ensure_user_dir ${USER_DATA_DIR} "ash-chrome"

  cat <<EOF
tip: Once you finished OOBE, you can login using any string (e.g. 'x').
EOF

  echo $ARGS

  exec ${ASH_CHROME_BUILD_DIR}/chrome $ARGS
}

# Start wayland client binary on ash-chrome
function start_wayland_client {
  exec $*
}

# Convert the list of panel names into host bounds list.
function build_display_config {
  panel_list=$*
  IFS=,
  local host_bounds_list=()
  for panel in ${panel_list}; do
    if [ -z ${DISPLAY_RES[${panel}]} ]; then
      return;
    fi
    host_bounds_list+=( ${DISPLAY_RES[${panel}]} )
  done
  IFS=_
  echo $(IFS=, ; echo "${host_bounds_list[*]}")
}

function help {
  exec cat <<EOF
`basename $0` <command> [options]
command
  ash-chrome (default)   start ash-chrome
  show-xinput-device-id  shows the device id that can be used to emulate touch.
  wayland-client         start wayland-client on ash-chrome. This command passes
                         all options to the client.
  help                   print this message.

[options]
  --ash-chrome-build-dir specifies the build directory for ash-chrome.
  --guest                start in guest mode.
  --panel=<list of type> specifies the panel type. Valid opptions are:
                         wxga(1280x800 default), fwxga(1355x768), hdp(1600,900),
                         fhd(1920x1080), wuxga(1920,1200), qhd(2560,1440),
                         qhdp(3200,1800), f4k(3840,2160)
                         multiple panels can be specifid, e.g. "wxga,hdp"
  --touch-device-id=<id> Specify the input device to emulate touch. Use id from
                         'show-xinput-device-id'.
  --user-data-dir        specifies the user data dir
  --wayland-debug        Enable WAYLAND_DEBUG=1
  --<chrome commandline flags>
                         Pass extra command line flags to ash-chrome.
                         The script will reject if the string does not exist in
                         chrome binary.
EOF
}

# Retrieve command.
if [ "${1}" == "wayland-client" ]; then
  shift
  start_wayland_client $*
elif [ ${#} -eq 0 -o "${1:0:2}" == "--" ]; then
  command=ash-chrome
else
  command=$1
  shift;
fi

SLEEP_IF_EXTRA_ARGS_NOT_MATCHED=false
GUEST_MODE=false

# Parse options.
while [ ${#} -ne 0 ]
do
  case ${1} in
    --ash-chrome-build-dir=*)
      ASH_CHROME_BUILD_DIR=${1:23}
      ;;
    --user-data-dir=*)
      USER_DATA_DIR=${1:16}
      ;;
    --wayland-debug)
      export WAYLAND_DEBUG=1
      ;;
    --touch-device-id=*)
      id=${1:18}
      TOUCH_DEVICE_OPTION="--touch-devices=${id} --force-show-cursor"
      ;;
    --guest)
      GUEST_MODE=true
      ;;
    --panel=*)
      panel=${1:8}
      DISPLAY_CONFIG=$(build_display_config ${panel})
      if [ -z $DISPLAY_CONFIG ]; then
        echo "Unknown display panel found in '$panel'"
        help
      fi
      ;;
    --*)
      if [ -f ${ASH_CHROME_BUILD_DIR}/chrome ]; then
        flag_name=${1:2}
        set +e
        result=$(strings ${ASH_CHROME_BUILD_DIR}/chrome | grep "$flag_name")
        set -e
        if [ -z "$result" ] ; then
          cat <<EOF
Warning: Can't find command line flag '${1}' in ash-chrome
EOF
          SLEEP_IF_EXTRA_ARGS_NOT_MATCHED=true
        fi
      fi
      EXTRA_ARGS="${EXTRA_ARGS} $1"
      ;;
    *) echo "Unknown option $1"; help ;;
  esac
  shift
done

if $SLEEP_IF_EXTRA_ARGS_NOT_MATCHED ; then
  echo
  sleep 2
fi

case $command in
  ash-chrome) start_ash_chrome $GUEST_MODE;;
  help) help ;;
  show-xinput-device-id) exec xinput -list ;;
  *) echo "Unknown command $command"; help ;;
esac
