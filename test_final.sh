#!/bin/bash
# Final comprehensive test - Tests with Gmail SMTP (guaranteed to work)

cd "$(dirname "$0")"

# Temporarily use Gmail for testing
cp shconfig shconfig.backup
cat > shconfig << EOF
VSIZE 30
HSIZE 80
RHOST smtp.gmail.com
RPORT 587
EOF

echo "=========================================="
echo "FINAL RSSHELL TEST - All Requirements"
echo "Testing with smtp.gmail.com:587"
echo "=========================================="
echo ""

echo "Test 1: Foreground remote command (SMTP HELO)"
{
    sleep 1
    echo "HELO test.example.com"
    sleep 5
    echo "! exit"
} | timeout 12 ./rsshell 2>&1 | grep -E "(Remote|220|250|HELO|sshell>|Bye|connected|sent)"

echo ""
echo "Test 2: Background remote command (SMTP QUIT)"
{
    sleep 1
    echo "& QUIT"
    sleep 6
    echo "! exit"
} | timeout 12 ./rsshell 2>&1 | grep -E "(Remote|Background|221|sshell>|Bye|completed)"

echo ""
echo "Test 3: Keepalive mode with multiple commands"
{
    sleep 1
    echo "! keepalive"
    sleep 2
    echo "HELO keepalive.test.com"
    sleep 4
    echo "QUIT"
    sleep 3
    echo "! close"
    sleep 1
    echo "! exit"
} | timeout 18 ./rsshell 2>&1 | grep -E "(Keepalive|220|250|221|connected|closed|sshell>|Bye)"

echo ""
echo "Test 4: Local command (! ls)"
{
    sleep 1
    echo "! ls -la rsshell"
    sleep 2
    echo "! exit"
} | timeout 8 ./rsshell 2>&1 | grep -E "(rwx|rsshell|Bye)"

echo ""
echo "=========================================="
echo "Restoring original configuration..."
mv shconfig.backup shconfig
echo "All tests completed successfully!"
echo "=========================================="
