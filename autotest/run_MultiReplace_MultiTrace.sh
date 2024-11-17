#!/bin/bash

# Ensure that an argument is provided
if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <warmup_instructions> <simulation_instructions> <trace_folder_filepath>"
  exit 1
fi

# Check if jq is installed
if ! command -v jq &> /dev/null; then
    echo "jq is not installed. Installing jq..."
    sudo apt-get update
    sudo apt-get install -y jq
else
    echo "jq is already installed."
fi

# Setup variables for the script
warmup_instructions="$1"
simulation_instructions="$2"
trace_folder_filepath="$3"

test_dir=$(pwd)
champsim_dir=$(dirname $(pwd))
config_file="$champsim_dir/champsim_config.json"

echo "AutoTest directory: $test_dir"
echo "Champsim directory: $champsim_dir"
echo "Config file: $config_file"


# Loop through all folders in the repalcement folder in ChampSim
for replacement_folder in $champsim_dir/replacement/*/; do
    # Update the "replacement" parameter in the "LLC" section of champsim_config.json
    replacement_policy=$(basename "$replacement_folder")
    jq --arg replacement "$replacement_policy" '.LLC.replacement = $replacement' "$config_file" > tmp.$$.json && mv tmp.$$.json "$config_file"
    echo "Running tests with replacement policy:${replacement_policy}"

    ./run_SingleReplace_MultiTrace.sh "$replacement_policy" "$warmup_instructions" "$simulation_instructions" "$trace_folder_filepath"
done