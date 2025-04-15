#!/bin/bash
# test_redirection.sh
echo "=== Redirection Test ==="
# Redirect the list of files from test_env into a file.
ls test_env > test_env/out.txt
echo "Contents of test_env/out.txt after output redirection:"
cat test_env/out.txt

# Create a sample file if it does not exist.
echo "Sample text for input redirection." > test_env/sample.txt
echo "Displaying contents using input redirection:"
cat < test_env/sample.txt
