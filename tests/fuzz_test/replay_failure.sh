#!/bin/bash

# RPC++ Fuzz Test Replay Helper Script
# Usage: ./replay_failure.sh [failure_file.json] [output_directory]

REPLAY_DIR="tests/fuzz_test/replays"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <failure_scenario_file> [output_directory]"
    echo ""
    echo "Available failure scenarios in $REPLAY_DIR:"
    find "$REPLAY_DIR" -name "FAILURE_*.json" -exec basename {} \; 2>/dev/null | sort || echo "  No failure scenarios found yet"
    echo ""
    echo "Examples:"
    echo "  $0 FAILURE_1_1234567890.json"
    echo "  $0 FAILURE_1_1234567890.json /tmp/my_scenarios"
    echo "  $0 /tmp/my_scenarios/FAILURE_1_1234567890.json"
    exit 1
fi

FAILURE_FILE="$1"

# If second argument provided, use it as the search directory
if [ $# -ge 2 ]; then
    REPLAY_DIR="$2"
fi

# Check if file exists
if [ ! -f "$FAILURE_FILE" ]; then
    # Try to find it in the specified directory
    if [ -f "$REPLAY_DIR/$FAILURE_FILE" ]; then
        FAILURE_FILE="$REPLAY_DIR/$FAILURE_FILE"
    else
        echo "Error: File $FAILURE_FILE not found"
        echo "Searched in: $REPLAY_DIR"
        exit 1
    fi
fi

echo "🔄 Building fuzz_test_main..."
cmake --build build --target fuzz_test_main

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "🚀 Replaying failure scenario: $FAILURE_FILE"
echo "📝 TIP: Run in debugger with: gdb --args ./build/output/debug/fuzz_test_main replay $FAILURE_FILE"
echo ""

# Run the replay
./build/output/debug/fuzz_test_main replay "$FAILURE_FILE"