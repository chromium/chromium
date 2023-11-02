#!/bin/bash
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
shopt -s nullglob
shopt -s extglob

function kill_process() {
    name=$1
    pid=$($adb shell "pidof $name" || true)
    if [ ! -z "$pid" ]; then
        echo "Killing $name..."
        $adb shell "kill $pid"
    else
        echo "$name is not running."
    fi
}

function get_data_file() {
    trace_file=$1
    extension=$2
    echo "${trace_file%.*}.$extension"
}

function get_logcat_file() {
    trace_file=$1
    get_data_file "$trace_file" "logcat"
}

function get_meminfo_file() {
    trace_file=$1
    tag=$2
    get_data_file "$trace_file" "$tag.meminfo"
}

function analyze_trace_file() {
    trace_file=$1
    trace_events_to_print=$2
    logcat_file="$(get_logcat_file "$trace_file")"
    echo "Processes started / died:"
    grep -E "Start proc|died|MY-DBG" "$logcat_file"

    extractor_arguments="$trace_file"
    if [ ! -z "$trace_events_to_print" ]; then
        extractor_arguments="$extractor_arguments --print-events=$trace_events_to_print"
    fi
    "$this_path/systrace-extract-startup.py" $extractor_arguments
}

function analyze_meminfo_file() {
    meminfo_file=$1
    if [ -f "$meminfo_file" ]; then
        echo "$meminfo_file"
        grep -A 5 "Total RAM" "$meminfo_file"
    fi
}

function capture_analyze_meminfo_file() {
    meminfo_file=$1
    $adb shell "dumpsys meminfo" > "$meminfo_file"
    analyze_meminfo_file "$meminfo_file"
}

function echo_separator() {
    s="----------"
    echo
    echo "$s$s$s$s$s$s$s$s" # 8*10
}

this_path="$(dirname "$0")"

adb="$ADB_PATH"

if [ -z "$adb" ]; then
    adb="$(which adb)"
fi
if [ -z "$adb" ]; then
    echo "Where's adb? Can't find it."
    exit 1
fi

export ADB_PATH="$adb"

output_tag=
browser=chrome
trace_time=10
cold=false
url=
atrace_buffer_size=
atrace_categories=
killg=false
repeat=
analyze=false
meminfo=false
checktabs=false
taburl=
trace_events_to_print=
webapk_package=

for i in "$@"; do
    case $i in
        --help)
        echo "$(basename "$0") output-tag [options]"
        exit 0
        ;;
        --browser=*)
        browser="${i#*=}"
        ;;
        --url=*)
        url="${i#*=}"
        ;;
        --warm)
        cold=false
        ;;
        --cold)
        cold=true
        ;;
        --atrace=*)
        atrace_categories="${i#*=}"
        ;;
        --atrace-buffer-size*)
        atrace_buffer_size="${i#*=}"
        ;;
        --killg)
        killg=true
        ;;
        --trace-time=*)
        trace_time="${i#*=}"
        ;;
        --repeat=*)
        repeat="${i#*=}"
        ;;
        --analyze)
        analyze=true
        ;;
        --meminfo)
        meminfo=true
        ;;
        --checktabs)
        checktabs=true
        ;;
        --taburl=*)
        taburl="${i#*=}"
        ;;
        --print-trace-events=*)
        trace_events_to_print="${i#*=}"
        ;;
        --webapk=*)
        webapk_package="${i#*=}"
        ;;
        --extra_chrome_categories=*)
        extra_chrome_categories="${i#*=}"
        ;;
        --*)
        echo "Unknown option or missing option argument: $i"
        exit 1
        ;;
        *)
        if [ -z "$output_tag" ]; then
            output_tag="$i"
        else
            echo "Unknown option: $i"
            exit 1
        fi
        ;;
    esac
    if [ -z "$output_tag" ]; then
        echo "First argument must be the output tag."
        exit 1
    fi
    shift
done

if [ -d "$output_tag" ]; then
    output_tag="${output_tag%%+(/)}/trace"
fi
specified_output_tag="$output_tag"

if [ $analyze = true ]; then
    for file in $output_tag*.html; do
        echo_separator
        echo "$file"

        analyze_meminfo_file "$(get_meminfo_file "$output_file" "before")"
        analyze_meminfo_file "$(get_meminfo_file "$output_file" "after")"

        echo

        analyze_trace_file "$file" "$print_trace_events"
    done

    exit 0
fi

browser_package=
case $browser in
    chrome)
    browser_package="com.google.android.apps.chrome"
    ;;
    canary)
    browser_package="com.chrome.canary"
    browser="chrome_canary"
    ;;
    dev)
    browser_package="com.chrome.dev"
    ;;
    beta)
    browser_package="com.chrome.beta"
    ;;
    stable)
    browser_package="com.android.chrome"
    ;;
    chromium)
    browser_package="org.chromium.chrome"
    ;;
    *)
    echo "Unknown browser $browser"
    exit 1
esac

profile_options=

profile_options="$profile_options --browser=$browser"
if [ ! "$browser" = "chrome" ]; then
    output_tag="$output_tag-$browser"
fi

profile_options="$profile_options --time=$trace_time"

if [ ! -z "$atrace_categories" ]; then
    profile_options="$profile_options --atrace-categories=$atrace_categories"
    output_tag="$output_tag-${atrace_categories//,/_}"
fi

if [ ! -z "$atrace_buffer_size" ]; then
    profile_options="$profile_options --atrace-buffer-size=$atrace_buffer_size"
    output_tag="$output_tag-${atrace_buffer_size}"
fi

if [ $cold = true ]; then
    profile_options="$profile_options --cold"
    output_tag="$output_tag-cold"
fi

profile_options="$profile_options --url=$url"
if [ ! -z "$url" ]; then
    output_tag="$output_tag-url"
fi

if [ ! -z "$webapk_package" ]; then
    output_tag="$output_tag-webapk"
    profile_options="$profile_options --webapk-package=$webapk_package"
fi

if [ $killg = true ]; then
    output_tag="$output_tag-killg"
fi

# Must be last for ease of globbing.
output_tag="$output_tag-${trace_time}s"

if [ $checktabs = true ] || [ $killg = true ] || [ ! -z "$taburl" ]; then
    $adb root > /dev/null
fi

# Make sure Chrome can write the trace file.
# These commands may fail with a security exception on some Android devices that
# don't allow changing permissions. Chrome likely already has these permissions
# so the script should still work.
$adb shell "pm grant $browser_package \
                android.permission.READ_EXTERNAL_STORAGE" || true
$adb shell "pm grant $browser_package \
                android.permission.WRITE_EXTERNAL_STORAGE" || true

if [ ! -z "$taburl" ]; then
    echo "Opening $taburl in a single tab..."
    $adb shell "am force-stop $browser_package"
    $adb shell "rm -f /data/data/$browser_package/app_tabs/0/tab*"
    $adb shell "am start -a android.intent.action.VIEW \
                    -n $browser_package/org.chromium.chrome.browser.ChromeTabbedActivity \
                    -d $taburl"
    sleep 5
    $adb shell "am start -a android.intent.action.MAIN -c android.intent.category.HOME"
    sleep 1
    $adb shell "am force-stop $browser_package"
    echo
fi

repeat_count=1
if [ ! -z "$repeat" ]; then
    repeat_count="$repeat"
fi

if [ ! -z "$extra_chrome_categories" ]; then
    chrome_categories="_DEFAULT_CHROME_CATEGORIES,${extra_chrome_categories}"
    profile_options="$profile_options --chrome_categories=${chrome_categories}"
fi

first_iteration=true
for iteration in $(seq "$repeat_count"); do

    if [ $first_iteration = true ]; then
        first_iteration=false
    else
        echo_separator
        sleep 2
    fi

    if [ $killg = true ]; then
        echo "Preemptively killing g* processes..."
        $adb logcat -c
        kill_process "com.google.process.gapps"
        kill_process "com.google.android.gms"
        sleep 1
        $adb logcat -d | grep -E "Start proc|died" || true
    fi

    echo

    if [ -z "$repeat" ]; then
        output_file="$output_tag.html"
        if [ -f "$output_file" ]; then
            for i in {2..1000}; do
                output_file="$output_tag~$i.html"
                if [ ! -f "$output_file" ]; then
                    break;
                fi
            done
        fi
        if [ -f "$output_file" ]; then
            echo "Failed to find unoccupied output file. Last was: $output_file"
            exit 1
        fi
    else
        output_file="$output_tag~${repeat_count}_$iteration.html"
        if [ -f "$output_file" ]; then
            echo "Output file already exists: $output_file"
            echo "Please add something unique to the tag ($specified_output_tag)."
            exit 1
        fi
    fi
    echo "Output file: $output_file"

    logcat_file="$(get_logcat_file "$output_file")"
    echo "Logcat output file: $logcat_file"

    if [ $meminfo = true ]; then
        echo
        capture_analyze_meminfo_file "$(get_meminfo_file "$output_file" "before")"
    fi

    echo

    echo "Profiling with options: $profile_options"
    ./build/android/adb_profile_chrome_startup \
        $profile_options \
        "--output=$output_file"

    if [ $meminfo = true ]; then
        echo
        capture_analyze_meminfo_file "$(get_meminfo_file "$output_file" "after")"
    fi

    $adb shell "am force-stop $browser_package"
    $adb shell "killall logcat" > /dev/null 2>&1 || true

    $adb logcat -d > "${logcat_file}"

    rm -f chrome-profile-results-*
    $adb shell "rm -f /sdcard/Download/chrome-profile-results-*"

    echo

    analyze_trace_file "$output_file" "$trace_events_to_print"

    if [ $checktabs = true ]; then
        # Yields empty string when value is -1
        active_tab_id="$( \
            $adb shell "cat /data/data/$browser_package/shared_prefs/${browser_package}_preferences.xml" \
            | sed -n 's/^.*ACTIVE_TAB_ID" value="\([0-9]\{1,3\}\).*$/\1/p')"
        # Yields empty string when there is no 'tab' file
        tab_id="$( \
            $adb shell ls /data/data/$browser_package/app_tabs/0 \
            | sed -n 's/tab\([0-9]\{1,3\}\)\s*/\1/p')"
        if [ "$active_tab_id" != "$tab_id" ]; then
            echo "Tab IDs don't match (active_tab_id=$active_tab_id, tab_id=$tab_id), last result is not reliable."
            exit 1
        fi
    fi
done
