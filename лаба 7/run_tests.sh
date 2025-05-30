#!/bin/bash

# Define the output file for the results
OUTPUT_FILE="matrix_mult_results.txt"

# Write the CSV header to the output file
echo "N,Threads,Time_ms" > "$OUTPUT_FILE"

# Define matrix sizes (N) to test. Adjust these values as needed.
# N should be at least 1 and at most MAX_N (2500 in your C code).
matrix_sizes=(2 4 6 8 12 16 20 24)

# Define thread counts to test. Adjust these values as needed.
# Threads should be at least 1 and at most 128 (as per your C code's check).
thread_counts=(1 3 4 7 15 32 64 128)

echo "Starting data collection..."

# Loop through each matrix size
for N_val in "${matrix_sizes[@]}"; do
    # Loop through each thread count
    for THREADS_val in "${thread_counts[@]}"; do
        echo "  Running N=${N_val}, THREADS=${THREADS_val}..."

        # Execute the C program and pipe its output to awk.
        # awk extracts the time and uses the N_val and THREADS_val from the bash loop.
        # The output is appended to the results file.
        ./laba7full "$N_val" "$THREADS_val" | \
        awk -v N="$N_val" -v T="$THREADS_val" '/Matrix size:/ { print N "," T "," $(NF-1) }' >> "$OUTPUT_FILE"
    done
done

echo "Data collection complete. Results saved to '$OUTPUT_FILE'"
echo "You can now run the Python plotting script."

