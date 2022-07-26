#!/bin/bash

filter_protocol_commands() {
 jq '{"domains": [{domains} | .domains[] | {domain,commands}]}' "$1" > "$2"
}

filter_protocol_commands node_modules/devtools-protocol/json/browser_protocol{,_commands_only}.json
filter_protocol_commands node_modules/devtools-protocol/json/js_protocol{,_commands_only}.json
