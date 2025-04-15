#!/bin/bash
# test_builtins.sh
echo "=== Built-ins Test ==="
# Print the starting directory.
pwd
# Change to test_env directory and print new directory.
cd test_env
pwd
# Change into subdirectory and print.
cd subdir
pwd
# Go back two levels.
cd ../..
# Attempt to change to a non-existent directory.
cd non_existent_dir
echo "Finished built-ins test."
