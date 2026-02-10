#!/bin/bash
# Manual SMTP test - gives time for server responses

cd "$(dirname "$0")"

echo "=== Testing SMTP Connection to linux.ubishops.ca:25 ==="
echo ""

# Test with keepalive and multiple commands
{
    sleep 1
    echo "! keepalive"
    sleep 2
    echo "HELO test.example.com"
    sleep 3
    echo "MAIL FROM:<test@example.com>"
    sleep 3
    echo "RCPT TO:<mbarbosa25@ubishops.ca>"
    sleep 3
    echo "QUIT"
    sleep 3
    echo "! exit"
} | ./rsshell

echo ""
echo "=== Test Complete ==="
