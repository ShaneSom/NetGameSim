#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

bash "$SCRIPT_DIR/run_10_node.sh"
echo ""
bash "$SCRIPT_DIR/run_60_node.sh"

echo ""
echo "All experiments completed."