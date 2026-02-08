# rsshell - Remote Shell TCP/IP Client

## Overview

**rsshell** is a TCP/IP-based remote shell client that extends a Unix shell with network capabilities. It enables users to execute commands on remote servers and receive responses through network sockets, supporting both synchronous and asynchronous command execution.

This project implements **Assignment 2** from the CS 464/564 course at Bishop's University, building upon the shell framework provided in Assignment 1.

## Features

### Core Functionality
- **Local Command Execution**: Commands prefixed with `!` execute locally
- **Remote Command Execution**: Commands without prefix are sent to configured remote server
- **Asynchronous Execution**: Commands prefixed with `&` return immediately without waiting for response
- **Persistent Connections**: `! keepalive` mode maintains TCP connection across multiple commands
- **Protocol Support**: Handles both Unix (`\n`) and Windows (`\r\n`) line terminators

### Technical Highlights
- Non-blocking socket I/O with configurable timeouts
- Automatic connection state management
- Server-initiated closure detection
- Child process forking for background command execution
- Configuration file support (RHOST, RPORT, VSIZE, HSIZE parameters)

## Building

### Requirements
- Unix/Linux environment (Linux, macOS, or WSL on Windows)
- g++ compiler
- GNU make
- POSIX APIs (fork, exec, signal handling, sockets)

### Build Instructions

```bash
cd sshell
make clean && make
```

This produces the `rsshell` executable.

## Usage

### Launch the Shell
```bash
./rsshell
```

### Local Commands (prefixed with '!')
```
! <command>          Execute local command
! exit               Terminate shell
! more <file>        Display file with paging
! keepalive          Enable persistent remote connection
! close              Close persistent remote connection
! & <command>        Execute local command in background
```

### Remote Commands
```
<command>            Send command to remote server (foreground - wait for response)
& <command>          Send command to remote server (background - return immediately)
```

### Configuration
Edit `shconfig` file:
```
VSIZE 30             Terminal height in lines
HSIZE 80             Terminal width in characters
RHOST localhost      Remote server hostname/IP
RPORT 8001           Remote server port number
```

## Implementation Details

### Architecture

**Socket Management**
- TCP/IP connections using AF_INET sockets
- Non-blocking I/O with configurable timeouts via `recv_nonblock()`
- Persistent connection mode (keepalive) for reduced network overhead
- Automatic connection cleanup and state tracking

**Asynchronous Execution**
- Commands with `&` prefix use `fork()` to create child processes
- Parent shell returns prompt immediately
- Child processes independently read responses from server
- Multiple children may execute concurrently

**Protocol Handling**
- Protocol-agnostic response reading supporting both `\r\n` and `\n` terminators
- Proper detection of server-initiated connection closure
- Timeout-based reading prevents hanging on unresponsive servers

### Key Functions

- `open_remote_connection()` - Establish TCP connection to remote server
- `send_remote_command()` - Transmit command to remote server
- `read_remote_response_sync()` - Read response in foreground (blocking)
- `read_remote_response_async()` - Read response in background (non-blocking with fork)
- `close_remote_connection()` - Terminate TCP connection

## Asynchronous Command Behavior

### Why Responses Are Interleaved

When multiple asynchronous commands are issued in rapid succession:

1. **Process Creation**: Each `& command` forks a new child process
2. **Concurrent Execution**: Child processes run time-multiplexed by kernel scheduler
3. **Non-Blocking I/O**: Each child calls `recv_nonblock()` to read independently
4. **Shared Buffer**: Children read from same socket buffer in unpredictable order
5. **Kernel Scheduling**: Scheduler determines which child's read executes when

This results in responses appearing interleaved or out-of-order, which is the expected and correct behavior for unsynchronized concurrent processes.

### Example

```bash
! keepalive
& This is command 1
& This is command 2  
& This is command 3
```

Possible output (due to kernel scheduling):
```
This is command
This is command
1
2
This is command 3
```

All variants are valid because there is no synchronization between child processes.

## Testing

The project has been verified through:

1. **Build Verification**: Compiles without errors or warnings
2. **Functional Testing**: All 10 assignment requirements tested
3. **Network Testing**: SMTP server at linux.ubishops.ca:25
   - Foreground commands: Successfully transmitted and received
   - Keepalive mode: Persistent connection established and maintained
   - Async commands: Background execution with proper interleaving
4. **Code Quality**: No memory leaks, proper signal handling, error checking

## Assignment 2 Requirements Verification

✓ 1. Local commands with '!' prefix
✓ 2. Remote commands routed to RHOST:RPORT
✓ 3. Response display and handling
✓ 4. Asynchronous commands with '&' prefix
✓ 5. Line terminator support (\r\n and \n)
✓ 6. Keepalive command for persistent connections
✓ 7. Close command for connection termination
✓ 8. Server-initiated connection closure detection
✓ 9. rsshell executable target in Makefile
✓ 10. Comprehensive testing and documentation

## Files

- `sshell.cc` - Main remote shell implementation (565 lines)
- `tcp-utils.h/cc` - TCP socket utilities (Dr. Stefan D. Bruda)
- `tokenize.h/cc` - String parsing utilities (Dr. Stefan D. Bruda)
- `Makefile` - Build configuration
- `shconfig` - Runtime configuration file
- `testmore` - Sample file for testing 'more' command
- `README` - Comprehensive documentation

## Attribution

This implementation extends the Assignment 1 Unix shell framework provided by **Dr. Stefan D. Bruda** at Bishop's University.

**Base Code (Assignment 1) by Dr. Bruda:**
- tcp-utils.h/cc - TCP socket utilities
- tokenize.h/cc - String parsing utilities
- Base shell structure and signal handling framework
- Configuration file parsing system

**Assignment 2 Extensions:**
- TCP/IP remote command routing
- Persistent connection (keepalive) mode
- Asynchronous response handling via fork()
- Non-blocking socket I/O
- Protocol-agnostic response processing
- Complete documentation and testing

## Known Limitations

- Long lines in files displayed by 'more' may truncate
- Commands longer than 128 characters are truncated
- File paging requires blank line + enter (not just enter)
- Maximum hostname length constrained by buffer size

These limitations are inherited from the Assignment 1 framework and do not affect Assignment 2 functionality.

## Author

**Matheus Dornelles Barbosa Maia**
- Email: mbarbosa25@ubishops.ca
- Student Number: 2362581
- Bishop's University, CS 464/564

## License

This project is submitted as coursework for Bishop's University.

---

**Last Updated:** February 7, 2026
