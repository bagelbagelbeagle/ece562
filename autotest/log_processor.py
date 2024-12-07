import os
import re
import csv

# Folder containing the log files
log_folder = "C:\\Users\\Charles\\Desktop\\Data Processing\\ship_mod 4-20241205201926"  # Change this to your folder path
output_csv = "summary.csv"

# CSV column headers
headers = [
    "trace_name", "replacement_policy", "hits", 
    "misses", "total_accesses", "empty_column", 
    "instructions", "cycles"
]

# Function to extract data from a single log file
def extract_log_data(file_path):
    with open(file_path, "r") as file:
        content = file.read()
    
    # Extract trace_name and replacement_policy from the filename
    filename = os.path.basename(file_path)
    trace_name = filename.split(".champsimtrace")[0]
    replacement_policy = filename.split(".champsimtrace.")[-1].replace(".log", "")
    
    # Extract hits, misses, and total_accesses
    match = re.search(r"LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)", content)
    total_accesses, hits, misses = match.groups() if match else ("", "", "")
    
    # Extract instructions and cycles
    match = re.search(r"CPU 0 cumulative IPC: .* instructions: (\d+) cycles: (\d+)", content)
    instructions, cycles = match.groups() if match else ("", "")
    
    return [
        trace_name, replacement_policy, hits, 
        misses, total_accesses, "", 
        instructions, cycles
    ]

# Read all log files and extract data
data = []
for file_name in os.listdir(log_folder):
    if file_name.endswith(".log"):
        file_path = os.path.join(log_folder, file_name)
        log_data = extract_log_data(file_path)
        data.append(log_data)

# Write data to CSV
with open(output_csv, "w", newline="") as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(headers)
    writer.writerows(data)

print(f"Summary CSV generated: {output_csv}")
