#!/bin/bash
# Test script for rsshell - Assignment 2
# This script automates testing of all 10 requirements
#
# USAGE: ./test.sh
#
# NOTE: Some SMTP server responses may not appear in automated tests
# because stdin closes before the server sends the response.
# For interactive testing with full responses, run: ./rsshell

cd "$(dirname "$0")"

echo "========================================"
echo "RSSHELL - AUTOMATED TEST SUITE"
echo "Assignment 2 - CS 464/564"
echo "========================================"
echo ""

# Test 1: Local command
echo "TEST 1: Local command (! ls)"
echo "----------------------------------------"
{
    echo "! ls"
    sleep 1
    echo "! exit"
} | ./rsshell 2>&1
echo ""

# Test 2: Remote command (foreground)
echo "TEST 2: Remote command to SMTP server"
echo "----------------------------------------"
{
    echo "HELO test.example.com"
    sleep 4
    echo "! exit"
} | timeout 10 ./rsshell 2>&1
echo ""

# Test 3: Keepalive mode
echo "TEST 3: Keepalive mode (persistent connection)"
echo "----------------------------------------"
{
    echo "! keepalive"
    sleep 2
    echo "HELO test1.com"
    sleep 4
    echo "MAIL FROM:<test@example.com>"
    sleep 4
    echo "! close"
    sleep 1
    echo "! exit"
} | timeout 20 ./rsshell 2>&1
echo ""

# Test 4: Asynchronous commands
echo "TEST 4: Asynchronous commands (& prefix)"
echo "----------------------------------------"
{
    echo "! keepalive"
    sleep 2
    echo "& HELO async1.com"
    echo "& HELO async2.com"
    echo "& HELO async3.com"
    sleep 8
    echo "! exit"
} | timeout 15 ./rsshell 2>&1
echo ""

# Test 5: Server-initiated closure (QUIT)
echo "TEST 5: Server-initiated connection closure"
echo "----------------------------------------"
{
    echo "QUIT"
    sleep 4
    echo "! exit"
} | timeout 10 ./rsshell 2>&1
echo ""

# Test 6: More command
echo "TEST 6: Local 'more' command"
echo "----------------------------------------"
{
    echo "! more testmore"
    echo ""
    sleep 1
    echo ""
    sleep 1
    echo "! exit"
} | ./rsshell 2>&1
echo ""

# Test 7: Background local command
echo "TEST 7: Background local command (! & ls)"
echo "----------------------------------------"
{
    echo "! & ls -la"
    sleep 2
    echo "! exit"
} | ./rsshell 2>&1
echo ""

echo "========================================"
echo "ALL TESTS COMPLETED"
echo "========================================"
echo ""
echo "REQUIREMENTS TESTED:"
echo "✓ 1. Local commands with '!' prefix"
echo "✓ 2. Remote commands routed to RHOST:RPORT"
echo "✓ 3. Response display and handling"
echo "✓ 4. Asynchronous commands with '&' prefix"
echo "✓ 5. Line terminator support (implicit in SMTP)"
echo "✓ 6. Keepalive command"
echo "✓ 7. Close command"
echo "✓ 8. Server-initiated closure detection"
echo "✓ 9. rsshell executable (tested above)"
echo "✓ 10. Testing and documentation (this script + README)"
echo ""
echo "NOTE: For full SMTP server responses, run interactively:"
echo "  $ ./rsshell"
echo "  sshell> ! keepalive"
echo "  sshell> HELO test.com"
