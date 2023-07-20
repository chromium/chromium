#!/bin/bash -e
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A shell script to run chrome for chromeos on linux desktop, or lacros on
# that (chromeos for chrome on linux) environment.

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
LACROS_BUILD_DIR=${LASCROS_BUILD_DIR:-${CHROME_SRC_ROOT}/out/lacros}

# For information only
LACROS_LOG_FILE=${USER_DATA_DIR}/lacros/lacros.log

# Display Configurations
declare -A DISPLAY_RES=(
[wxga]=1280x800
[fwxga]=1366x768
[hdp]=1600x900*1.25
[fhd]=1920x1080*1.25
[wuxga]=1920x1200*1.6
[qhd]=2560x1440*2
[qhdp]=3200x1800*2.25
[f4k]=3840x2160*2.66
[slate]=3000x2000*2.25
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

# Use FHD as default panel.
DISPLAY_CONFIG=${DISPLAY_RES[fhd]}

LACROS_FEATURES=LacrosOnly
FEATURES=OverviewButton

LACROS_ENABLED=false

export XDG_RUNTIME_DIR=${USER_TMP_DIR}/xdg1
LACROS_SOCK_FILE=${USER_TMP_DIR}/lacros.sock

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
  ARGS="--user-data-dir=${USER_DATA_DIR} \
    --enable-wayland-server --ash-debug-shortcuts \
    --enable-ui-devtools --ash-dev-shortcuts --login-manager \
    --lacros-chrome-additional-args=--gpu-sandbox-start-early \
    --login-profile=user --lacros-mojo-socket-for-testing=$LACROS_SOCK_FILE \
    --ash-host-window-bounds=${DISPLAY_CONFIG} \
    --enable-features=${FEATURES} \
    ${TOUCH_DEVICE_OPTION} \
    --enable-ash-debug-browser \
    --lacros-chrome-path=${LACROS_BUILD_DIR}" \

  # To enable internal display.
  ARGS="${ARGS} --use-first-display-as-internal"
}

# Start ash chrome binary.
function start_ash_chrome {
  if $LACROS_ENABLED ; then
    FEATURES="$FEATURES,$LACROS_FEATURES"
  fi
  build_args

  check_chrome_dir "$ASH_CHROME_BUILD_DIR" ash-chrome-build-dir
  if $LACROS_ENABLED ; then
    check_chrome_dir "$LACROS_BUILD_DIR" lacros-build-dir
  fi
  ensure_user_dir ${USER_DATA_DIR} "ash-chrome"

  cat <<EOF
tip: Once you finished OOBE, you can login using any string (e.g. 'x').
EOF
  if $LACROS_ENABLED ; then
    cat <<EOF

tip: Lacros log file ${LACROS_LOG_FILE}
  or run

 $ `basename $0` lacros-log

tip: To start lacros from command line (not from shelf icon), run

 $ `basename $0` lacros

Starting ash-chrome ...
=======================================

EOF
  fi

  echo $ARGS

  exec ${ASH_CHROME_BUILD_DIR}/chrome $ARGS
}

# Start lacros chrome binary.
function start_lacros_chrome {
  local lacros_user_data_dir=${USER_TMP_DIR}/lacros-user-data-dir

  check_chrome_dir "$LACROS_BUILD_DIR" lacros-build-dir
  ensure_user_dir ${lacros_user_data_dir} "lacros"
  export EGL_PLATFORM=surfaceless
  exec ${CHROME_SRC_ROOT}/build/lacros/mojo_connection_lacros_launcher.py \
    -s ${LACROS_SOCK_FILE} \
    ${LACROS_BUILD_DIR}/chrome \
    --user-data-dir=${lacros_user_data_dir} \
    --enable-ui-devtools --gpu-sandbox-start-early
}

function lacros_log {
  exec tail -f ${LACROS_LOG_FILE}
}

function help {
  exec cat <<EOF
`basename $0` <command> [options]
command
  ash-chrome (default)   start ash-chrome
  lacros                 start lacros chrome. You must have ash-chrome running.
  lacros-log             tail lacros chrome log.
  show-xinput-device-id  shows the device id that can be used to emulate touch.
  help                   print this message.

[options]
  --enable-lacros        enables lacros.
  --ash-chrome-build-dir specifies the build directory for ash-chrome.
  --lacros-build-dir     specifies the build directory for lacros.
  --touch-device-id=<id> [ash-chrome only] Specify the input device to emulate
                         touch. Use id from 'show-xinput-device-id'.
  --wayland-debug        [ash-chrome,lacros] Enable WAYLAND_DEBUG=1
  --panel=<type>         specifies the panel type. Valid opptions are:
                         wxga(1280x800), fwxga(1355x768), hdp(1600,900),
                         fhd(1920x1080), wuxga(1920,1200), qhd(2560,1440),
                         qhdp(3200,1800), f4k(3840,2160)
EOF
}

# Retrieve command.
if [ ${#} -eq 0 -o "${1:0:2}" == "--" ]; then
  command=ash-chrome
else
  command=$1
  shift;
fi

# Parse options.
while [ ${#} -ne 0 ]
do
  case ${1} in
    --enable-lacros)
      LACROS_ENABLED=true
      ;;
    --ash-chrome-build-dir=*)
      ASH_CHROME_BUILD_DIR=${1:23}
      ;;
    --lacros-build-dir=*)
      LACROS_ENABLED=true
      LACROS_BUILD_DIR=${1:19}
      ;;
    --wayland-debug)
      export WAYLAND_DEBUG=1
      ;;
    --touch-device-id=*)
      id=${1:18}
      TOUCH_DEVICE_OPTION="--touch-devices=${id} --force-show-cursor"
      ;;
    --panel=*)
      panel=${1:8}
      DISPLAY_CONFIG=${DISPLAY_RES[${panel}]}
      if [ -z $DISPLAY_CONFIG ]; then
        echo "Unknown display panel: $panel"
        help
      fi
      ;;
    *) echo "Unknown option $1"; help ;;
  esac
  shift
done

case $command in
  lacros) start_lacros_chrome;;
  ash-chrome) start_ash_chrome ;;
  lacros-log) lacros_log ;;
  help) help ;;
  show-xinput-device-id) exec xinput -list ;;
  *) echo "Unknown command $command"; help ;;
esac
