#!/bin/bash

# Ensure that an argument is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <replacement_policy>"
  exit 1
fi

# Step 1: Read in the first argument and update the champsim_config.json file
replacement_policy="$1"
config_file="champsim_config.json"

# Update the "replacement" parameter in the "LLC" section of champsim_config.json
jq --arg replacement "$replacement_policy" '.LLC.replacement = $replacement' "$config_file" > tmp.$$.json && mv tmp.$$.json "$config_file"

# Step 2: Run config.sh with champsim_config.json
./config.sh "$config_file"

# Step 3: Run 'make' to build the project
make

#Create the output folder if it hasn't been created already
mkdir -p /home/student/ChampSim/output

# Step 4 and 5: Loop through each .xz file in ~/traces and run the simulation command
for trace_file in /home/student/traces/*.xz; do
  # Extract the trace name from the file path
  trace_name=$(basename "$trace_file" .xz)

  # Run the command and output to a log file with the replacement policy in the filename
  echo "Running ChampSim with trace:${trace_name}"
  bin/champsim --hide-heartbeat -w 10000000 -i 100000000 "$trace_file" >> "/home/student/ChampSim/output/${trace_name}.${replacement_policy}.log"
done
