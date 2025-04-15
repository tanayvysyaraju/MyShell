#!/bin/bash
# test_wildcards.sh
echo "=== Wildcard Expansion Test ==="
# Test wildcard expansion in the test_env directory.
ls test_env/*.txt

# Test wildcard expansion in a subdirectory.
ls test_env/subdir/*.txt
