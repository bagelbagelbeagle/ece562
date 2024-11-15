#!/bin/bash

# Ensure that an argument is provided
if [ "$#" -lt 4 ]; then
  echo "Usage: $0 <replacement_policy> <warmup_instructions> <simulation_instructions> <trace_folder_filepath>"
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

# Step 1: Read in the first argument and update the champsim_config.json file
replacement_policy="$1"
warmup_instructions="$2"
simulation_instructions="$3"
trace_folder_filepath="$4"

config_file="champsim_config.json"
champsim_dir=$(pwd)
echo "Champsim directory: $champsim_dir"

# Update the "replacement" parameter in the "LLC" section of champsim_config.json
jq --arg replacement "$replacement_policy" '.LLC.replacement = $replacement' "$config_file" > tmp.$$.json && mv tmp.$$.json "$config_file"

# Step 2: Run config.sh with champsim_config.json
./config.sh "$config_file"

# Step 3: Run 'make' to build the project
make

#Create the output folder if it hasn't been created already
mkdir -p "$champsim_dir/output"

# Step 4 and 5: Loop through each .xz file in ~/traces and run the simulation command
for trace_file in $trace_folder_filepath/*.xz; do
  # Extract the trace name from the file path
  trace_name=$(basename "$trace_file" .xz)

  # Run the command and output to a log file with the replacement policy in the filename
  echo "Running ChampSim with trace:${trace_name}"
  bin/champsim --hide-heartbeat -w "$warmup_instructions" -i "$simulation_instructions" "$trace_file" >> "$champsim_dir/output/${trace_name}.${replacement_policy}.log"
done
