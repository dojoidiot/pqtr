#!/bin/bash
# JWTA API test script

BASE_URL="${JWTA_URL:-https://jwta.pqtr.ai}"
ADMIN_EMAIL="${JWTA_ADMIN:-pqtr@pqtr.ai}"
TEST_EMAIL="${JWTA_TEST:-p@horyzon.co}"

echo "=== JWTA API Tests ==="
echo "Base URL: $BASE_URL"
echo ""

jrpc() {
    local method=$1
    local params=$2
    echo ">>> $method"
    curl -s -X POST "$BASE_URL/jrpc" \
        -H "Content-Type: application/json" \
        -d "{\"jsonrpc\":\"2.0\",\"method\":\"$method\",\"params\":$params,\"id\":1}" | tee /tmp/jwta_response.json
    echo -e "\n"
}

echo "=== Public Methods ==="

# Register
jrpc "register" "{\"email\":\"$TEST_EMAIL\"}"

# Login (for existing user)
jrpc "login" "{\"email\":\"$TEST_EMAIL\"}"

echo "=== Verify (requires OTP from email) ==="
echo "# jrpc verify '{\"email\":\"$TEST_EMAIL\",\"otp\":\"123456\"}'"
echo ""

echo "=== Admin Methods (requires JWT) ==="
echo "Set JWT env var after verify:"
echo "  export JWT=eyJ..."
echo ""

if [ -n "$JWT" ]; then
    # Find user
    jrpc "find" "{\"jwt\":\"$JWT\",\"email\":\"$TEST_EMAIL\"}"

    # Info (stats)
    jrpc "info" "{\"jwt\":\"$JWT\"}"

    # Extract user_id from find response for other tests
    USER_ID=$(cat /tmp/jwta_response.json 2>/dev/null | grep -o '"user_id":"[^"]*"' | head -1 | cut -d'"' -f4)

    if [ -n "$USER_ID" ]; then
        echo "Found user_id: $USER_ID"
        echo ""

        # Give role
        jrpc "give" "{\"jwt\":\"$JWT\",\"user_id\":\"$USER_ID\",\"role\":\"PLAY\"}"

        # Find again to verify role
        jrpc "find" "{\"jwt\":\"$JWT\",\"email\":\"$TEST_EMAIL\"}"

        # Take role
        jrpc "take" "{\"jwt\":\"$JWT\",\"user_id\":\"$USER_ID\"}"

        # Lock user
        jrpc "lock" "{\"jwt\":\"$JWT\",\"user_id\":\"$USER_ID\"}"

        # Free user
        jrpc "free" "{\"jwt\":\"$JWT\",\"user_id\":\"$USER_ID\"}"

        # Drop user (uncomment to test - deletes user!)
        # jrpc "drop" "{\"jwt\":\"$JWT\",\"user_id\":\"$USER_ID\"}"
    fi
else
    echo "# find"
    echo "jrpc find '{\"jwt\":\"\$JWT\",\"email\":\"$TEST_EMAIL\"}'"
    echo ""
    echo "# info"
    echo "jrpc info '{\"jwt\":\"\$JWT\"}'"
    echo ""
    echo "# give"
    echo "jrpc give '{\"jwt\":\"\$JWT\",\"user_id\":\"UUID\",\"role\":\"PLAY\"}'"
    echo ""
    echo "# take"
    echo "jrpc take '{\"jwt\":\"\$JWT\",\"user_id\":\"UUID\"}'"
    echo ""
    echo "# lock"
    echo "jrpc lock '{\"jwt\":\"\$JWT\",\"user_id\":\"UUID\"}'"
    echo ""
    echo "# free"
    echo "jrpc free '{\"jwt\":\"\$JWT\",\"user_id\":\"UUID\"}'"
    echo ""
    echo "# drop"
    echo "jrpc drop '{\"jwt\":\"\$JWT\",\"user_id\":\"UUID\"}'"
fi

echo "=== Refresh (requires refresh_token from verify) ==="
echo "# jrpc refresh '{\"refresh_token\":\"...\"}'"
