#!/bin/bash

# Ensure that an argument is provided
if [ "$#" -ne 4 ]; then
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

# Setup variables for the script
replacement_policy="$1"
warmup_instructions="$2"
simulation_instructions="$3"
trace_folder_filepath="$4"


test_dir=$(pwd)
champsim_dir=$(dirname $test_dir)
config_file="$champsim_dir/champsim_config.json"

echo "AutoTest directory: $test_dir"
echo "Champsim directory: $champsim_dir"
echo "Config file: $config_file"

# Update the "replacement" parameter in the "LLC" section of champsim_config.json
jq --arg replacement "$replacement_policy" '.LLC.replacement = $replacement' "$config_file" > tmp.$$.json && mv tmp.$$.json "$config_file"


#Create the output folder if it hasn't been created already
logs_dir="${test_dir}/logs/${replacement_policy}-"$(date +"%Y%m%d%H%M%S")""
mkdir -p "$logs_dir"
echo "Log file: ${logs_dir}"

# Rebuild ChampSim
cd "$champsim_dir"
./config.sh "$config_file"
make
cd "$test_dir"


# Loop through each .xz file in the trace folder
for trace_file in $trace_folder_filepath/*.xz; do
    # Extract the trace name from the file path
    trace_name=$(basename "$trace_file" .xz)

    # Run the command and output to a log file with the replacement policy in the filename
    echo "Running ChampSim with trace:${trace_name}"
    "${champsim_dir}/bin/champsim" --hide-heartbeat -w "$warmup_instructions" -i "$simulation_instructions" "$trace_file" >> "$logs_dir/${trace_name}.${replacement_policy}.log"
done