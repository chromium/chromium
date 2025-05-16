#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pprint
import requests
import sys
import json

API_URL = 'https://decisiongraph-pa.googleapis.com/v1/rundecisiongraph'
DECISION_GRAPH_NAME = 'smart_test_selection_graph_chrome'
STAGE_ID = 'test_selection_for_%d_%d_%d'
STAGE_NAME = 'smart_test_selection_stage'
PROJECT = 'chromium/src'
BRANCH = 'main'
# Only the first word of gerrit host, i.e., %s-review.googlesource.com
HOSTNAME = 'chromium'
LOCATION_ENUM = 1
STAGE_SERVICE_GSLB = 'blade:test-relevance-stage-service-prod-luci'
# TODO(crbug.com/405145095): Change this to a lower value after decisiongraph
# has moved to spanner queues.
MAX_DURATION_SECONDS = 900
TIMEOUT_SECONDS = MAX_DURATION_SECONDS + 60
MAX_ATTEMPTS = 3
BLOCKING_ENUM = 2

BATCH_SIZE = 5


def fetch_api_data(url, json_payload=None):
  """
    Sends an HTTP POST request to the url and returns the JSON response.

    Args:
      url: The url to send request to.
      json_payload: The payload to send with request.

    Returns:
      The JSON response as a dictionary, or None if the request fails.
    """
  try:
    response = requests.post(url, json=json_payload, timeout=TIMEOUT_SECONDS)
    print(response.text)
    print(response.status_code)
    print(response.json)
    response.raise_for_status(
    )  # Raise an HTTPError for bad responses (4xx and 5xx).
    return response.json()
  except requests.exceptions.RequestException as e:
    print(f"An error occurred: {e}")
    return None


def load_config_from_json(file_path):
  """
    Reads the configuration parameters from a JSON file.

    Args:
      file_path: Path to the JSON file.
    Returns:
      A dictionary containing the configuration parameters.
    """
  config_data = {}
  try:
    with open(file_path, "r", encoding="utf-8") as f:
      config_data = json.load(f)
  except FileNotFoundError:
    print(f"Error: Configuration file not found at {file_path}")
    sys.exit(1)
  except json.JSONDecodeError as e:
    print(f"Error: Could not decode JSON from {file_path}. Details: {e}")
    sys.exit(1)
  except Exception as e:
    print(
        f"Unexpected error occurred while reading the configuration file: {e}")
    sys.exit(1)

  # Validate required arguments needed to make API call.
  required_args = ["build_id", "change", "patchset", "builder", "api_key"]
  missing_args = [arg for arg in required_args if arg not in config_data]

  if missing_args:
    print(f"Error: Missing required arguments in JSON config file.")
    for arg in missing_args:
      print(f"  - {arg}")
    sys.exit(1)

  # Type checking/conversion for API call parameters.
  try:
    config_data["change"] = int(config_data["change"])
    config_data["patchset"] = int(config_data["patchset"])
    if not isinstance(config_data["build_id"], str):
      print("Error: 'build_id' must be a string in the JSON configuration.")
      sys.exit(1)
    if not isinstance(config_data["builder"], str):
      print("Error: 'builder' must be a string in the JSON configuration.")
      sys.exit(1)
    if not isinstance(config_data["api_key"], str):
      print("Error: 'api_key_file' must be a string in the JSON configuration.")
      sys.exit(1)

  except ValueError:
    print(f"Error: Invalid data type for 'change' or 'patchset' in JSON. " +
          "Expected integers.")
    sys.exit(1)
  except KeyError as e:
    # This should be caught by missing_args check, but it's included
    # as a safeguard.
    print(f"Error: A required key {e} is missing during type validation.")
    sys.exit(1)

  return config_data


def main():
  parser = argparse.ArgumentParser(
      description=
      "Fetch data from an API using parameters from a JSON config file.")
  parser.add_argument("--test-targets",
                      required=True,
                      nargs='+',
                      type=str,
                      help="Name of the test targets e.g., browser_tests.")
  parser.add_argument(
      "--sts-config-file",
      required=True,
      type=str,
      help="Path to the JSON file containing config for smart test selection.")
  args = parser.parse_args()

  config = load_config_from_json(args.sts_config_file)

  build_id = config["build_id"]
  change = config["change"]
  patchset = config["patchset"]
  builder = config["builder"]
  api_key = config["api_key"]

  # Find the corresponding "main" CQ builder name.
  canonical_builder = builder.removesuffix("-test-selection")

  test_target_batches = [
      args.test_targets[i:i + BATCH_SIZE]
      for i in range(0, len(args.test_targets), BATCH_SIZE)
  ]

  return_status = 0
  for batch_idx, test_target_batch in enumerate(test_target_batches):
    checks = []
    print("batch num = %d" % batch_idx)
    for test_target in test_target_batch:
      check = {
          'identifier': {
              'luci_test': {
                  'project': PROJECT,
                  'branch': BRANCH,
                  'builder': canonical_builder,
                  'test_suite': test_target,
              }
          },
          'run': {
              'luci_test': {
                  'build_id': build_id
              }
          },
      }
      checks.append(check)

    payload = {
        'graph': {
            'name':
            DECISION_GRAPH_NAME,
            'stages': [{
                'stage': {
                    'id': STAGE_ID % (change, patchset, batch_idx),
                    'name': STAGE_NAME,
                },
                'execution_options': {
                    'location': LOCATION_ENUM,
                    'address': STAGE_SERVICE_GSLB,
                    'prepare': True,
                    'max_duration': {
                        'seconds': MAX_DURATION_SECONDS
                    },
                    'max_attempts': MAX_ATTEMPTS,
                    'blocking': BLOCKING_ENUM,
                },
            }],
        },
        'input': [{
            'stage': {
                'id': STAGE_ID % (change, patchset, batch_idx),
                'name': STAGE_NAME,
            },
            'input': [{
                'checks': checks,
            }],
            'changes': {
                'changes': [{
                    'hostname': HOSTNAME,
                    'change_number': change,
                    'patchset': patchset,
                }],
            },
        }]
    }
    print("payload = ")
    pprint.pprint(payload)

    request_with_key = '%s?key=%s' % (API_URL, api_key)
    response_data = fetch_api_data(url=request_with_key, json_payload=payload)

    if response_data:
      print("API Response:")
      print(response_data)
    else:
      print("Failed to fetch data from the API.")
      return_status = 1

  sys.exit(return_status)


if __name__ == '__main__':
  sys.exit(main())
