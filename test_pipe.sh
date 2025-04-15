#!/bin/bash
# test_pipe.sh
echo "=== Pipe Test ==="
# Pipe the output of ls into grep to list only .txt files.
ls test_env | grep ".txt"
