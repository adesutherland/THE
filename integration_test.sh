#!/bin/bash

# Setup paths dynamically so it can be run via CTest or directly
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
THE_BIN="$DIR/cmake-build-debug/release/the"

# We must run from the source root so THE finds the profile and file
cd "$DIR"

# Clean up old log
rm -f parser.log editor.log editor_stderr.log

# Run THE in batch mode to trigger the SDSLH handshake
$THE_BIN -b -p test_profile.the test.toy 2> editor_stderr.log

# Wait a moment to ensure parser flushed logs
sleep 1

# Check for successful handshake
if grep -q "base_load_initial_content" parser.log; then
    echo "Integration Test Passed: Handshake and initial load successful."
    exit 0
else
    echo "Integration Test Failed: Handshake not found in parser log."
    cat parser.log || echo "No parser.log found"
    exit 1
fi
