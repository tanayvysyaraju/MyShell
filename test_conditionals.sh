#!/bin/bash
# test_conditionals.sh
echo "=== Conditional Execution Test ==="
# First command fails (boguscmd), so the or-condition should execute.
boguscmd
or echo "Fallback: boguscmd failed as expected"

# Another failing command, then an and-condition triggering die.
boguscmd2
and die "Forced failure after boguscmd2"

# This line should not be executed once 'die' terminates the shell.
echo "This command should not run."
