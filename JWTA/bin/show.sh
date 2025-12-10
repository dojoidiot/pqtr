#!/bin/bash
# Show user info by email

if [ -z "$1" ]; then
    echo "Usage: show.sh <email>"
    exit 1
fi

if [ -z "$JWT" ]; then
    echo "Error: JWT env var required"
    echo "  export JWT=eyJ..."
    exit 1
fi

BASE_URL="${JWTA_URL:-https://jwta.pqtr.ai}"
EMAIL=$1

curl -s -X POST "$BASE_URL/jrpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"method\":\"find\",\"params\":{\"jwt\":\"$JWT\",\"email\":\"$EMAIL\"},\"id\":1}"
echo ""
