/*
 * Assignment 2 (CS 464/564) - Remote Shell (rsshell)
 * 
 * Base implementation from Assignment 1 by Dr. Stefan D. Bruda
 * Extended with TCP/IP networking for Assignment 2
 * 
 * This solution builds upon the shell framework and utilities provided
 * by Dr. Stefan D. Bruda, incorporating additional features for:
 * - Remote command execution via TCP/IP
 * - Asynchronous command handling with fork() and child processes
 * - Persistent connection mode (keepalive)
 * - Protocol-agnostic response handling
 */

#include <stdio.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "tokenize.h"
#include "tcp-utils.h"  // for `readline' and TCP functions

/*
 * Global configuration variables.
 */
const char* path[] = {"/bin","/usr/bin",0}; // path, null terminated (static)
const char* prompt = "sshell> ";            // prompt
const char* config = "shconfig";            // configuration file

/*
 * Global state for remote connection management.
 */
int remote_socket = -1;  // -1 means no connection, otherwise socket descriptor
int keepalive_mode = 0;  // 0 = close after each command, 1 = keep connection open
char remote_host[256];   // remote host name
char remote_port[10];    // remote port number
int bg_remote_active = 0; // flag for active background remote commands

/*
 * Forward declarations for remote communication functions.
 */
int read_remote_response_sync(int sd);
int read_remote_response_async(int sd);

/*
 * Typical reaper.
 */
void zombie_reaper (int signal) {
    int ignore;
    while (waitpid(-1, &ignore, WNOHANG) >= 0)
        /* NOP */;
}

/*
 * Dummy reaper, set as signal handler in those cases when we want
 * really to wait for children.  Used by run_it().
 *
 * Note: we do need a function (that does nothing) for all of this, we
 * cannot just use SIG_IGN since this is not guaranteed to work
 * according to the POSIX standard on the matter.
 */
void block_zombie_reaper (int signal) {
    /* NOP */
}

/*
 * Close the remote connection if open and mark socket as -1.
 */
void close_remote_connection() {
    if (remote_socket >= 0) {
        close(remote_socket);
        remote_socket = -1;
#ifdef DEBUG
        fprintf(stderr, "%s: remote connection closed\n", __FILE__);
#endif
    }
}

/*
 * Establish a connection to the remote server.
 * Returns the socket descriptor on success, -1 on failure.
 */
int open_remote_connection() {
    if (remote_socket >= 0) {
        return remote_socket;  // already connected
    }
    
    int sd = connectbyport(remote_host, remote_port);
    if (sd < 0) {
        fprintf(stderr, "Cannot connect to %s:%s (error code: %d)\n", 
                remote_host, remote_port, sd);
        return -1;
    }
    
    remote_socket = sd;
#ifdef DEBUG
    fprintf(stderr, "%s: connected to remote server at %s:%s (fd=%d)\n", 
            __FILE__, remote_host, remote_port, sd);
#endif
    return sd;
}

/*
 * Send a line to the remote server and receive response.
 * Returns 1 on success, 0 on failure, -1 on connection closed by server.
 */
int send_remote_command(const char* command, int async) {
    int sd = open_remote_connection();
    if (sd < 0) {
        return 0;
    }
    
    // Send command to server
    char* to_send = new char[strlen(command) + 2];
    sprintf(to_send, "%s\n", command);
    
    if (write(sd, to_send, strlen(to_send)) < 0) {
        perror("write to remote server");
        delete [] to_send;
        close_remote_connection();
        return 0;
    }
    
#ifdef DEBUG
    fprintf(stderr, "%s: sent to remote: %s", __FILE__, to_send);
#endif
    
    delete [] to_send;
    
    // Receive response
    if (async) {
        // Background mode: print response as it arrives
        return read_remote_response_async(sd);
    } else {
        // Foreground mode: read entire response before returning
        return read_remote_response_sync(sd);
    }
}

/*
 * Read response from remote server in synchronous mode.
 * Waits for entire response before returning.
 */
int read_remote_response_sync(int sd) {
    const size_t bufsize = 1024;
    char* buf = new char[bufsize];
    int total_read = 0;
    
    while (1) {
        int n = recv_nonblock(sd, buf, bufsize - 1, 5000);  // 5 second timeout
        
        if (n == recv_nodata) {
            // No more data available
            break;
        } else if (n < 0) {
            // Error
            perror("recv from remote server");
            delete [] buf;
            close_remote_connection();
            return -1;
        } else if (n == 0) {
            // Connection closed by server
            delete [] buf;
            close_remote_connection();
            return -1;  // Signal connection was closed
        }
        
        buf[n] = '\0';
        printf("%s", buf);
        total_read += n;
    }
    
    printf("\n");  // Add newline after response
    delete [] buf;
    
    // Close connection if not in keepalive mode
    if (!keepalive_mode) {
        close_remote_connection();
    }
    
    return 1;
}

/*
 * Read response from remote server in asynchronous mode.
 * Prints response as it arrives and returns immediately.
 */
int read_remote_response_async(int sd) {
    // Fork a child process to handle the response
    int bgp = fork();
    
    if (bgp == 0) {
        // Child process: read and print response
        const size_t bufsize = 1024;
        char* buf = new char[bufsize];
        
        while (1) {
            int n = recv_nonblock(sd, buf, bufsize - 1, 5000);
            
            if (n == recv_nodata) {
                // Timeout, continue trying
                continue;
            } else if (n < 0) {
                // Error
                perror("recv from remote server");
                break;
            } else if (n == 0) {
                // Connection closed by server
                break;
            }
            
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }
        
        printf("\n[Background command completed]\n");
        delete [] buf;
        
        // Child closes its copy of the socket
        close(sd);
        exit(0);
    } else {
        // Parent returns immediately
        return 1;
    }
}



/*
 * run_it(c, a, e, p) executes the command c with arguments a and
 * within environment p just like execve.  In addition, run_it also
 * awaits for the completion of c and returns a status integer (which
 * has the same structure as the integer returned by waitpid). If c
 * cannot be executed, then run_it attempts to prefix c successively
 * with the values in p (the path, null terminated) and execute the
 * result.  The function stops at the first match, and so it executes
 * at most one external command.
 */
int run_it (const char* command, char* const argv[], char* const envp[], const char** path) {

    // we really want to wait for children so we inhibit the normal
    // handling of SIGCHLD
    signal(SIGCHLD, block_zombie_reaper);

    int childp = fork();
    int status = 0;

    if (childp == 0) { // child does execve
#ifdef DEBUG
        fprintf(stderr, "%s: attempting to run (%s)", __FILE__, command);
        for (int i = 1; argv[i] != 0; i++)
            fprintf(stderr, " (%s)", argv[i]);
        fprintf(stderr, "\n");
#endif
        execve(command, argv, envp);     // attempt to execute with no path prefix...
        for (size_t i = 0; path[i] != 0; i++) { // ... then try the path prefixes
            char* cp = new char [strlen(path[i]) + strlen(command) + 2];
            sprintf(cp, "%s/%s", path[i], command);
#ifdef DEBUG
            fprintf(stderr, "%s: attempting to run (%s)", __FILE__, cp);
            for (int i = 1; argv[i] != 0; i++)
                fprintf(stderr, " (%s)", argv[i]);
            fprintf(stderr, "\n");
#endif
            execve(cp, argv, envp);
            delete [] cp;
        }

        // If we get here then all execve calls failed and errno is set
        char* message = new char [strlen(command)+10];
        sprintf(message, "exec %s", command);
        perror(message);
        delete [] message;
        exit(errno);   // crucial to exit so that the function does not
                       // return twice!
    }

    else { // parent just waits for child completion
        waitpid(childp, &status, 0);
        // we restore the signal handler now that our baby answered
        signal(SIGCHLD, zombie_reaper);
        return status;
    }
}

/*
 * Implements the internal command `more'.  In addition to the file
 * whose content is to be displayed, the function also receives the
 * terminal dimensions.
 */
void do_more(const char* filename, const size_t hsize, const size_t vsize) {
    const size_t maxline = hsize + 256;
    char* line = new char [maxline + 1];
    line[maxline] = '\0';

    // Print some header (useful when we more more than one file)
    printf("--- more: %s ---\n", filename);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        sprintf(line, "more: %s", filename);
        perror(line);
        delete [] line;
        return;
    }

    // main loop
    while(1) {
        for (size_t i = 0; i < vsize; i++) {
            int n = readline(fd, line, maxline);
            if (n < 0) {
                if (n != recv_nodata) {  // error condition
                    sprintf(line, "more: %s", filename);
                    perror(line);
                }
                // End of file
                close(fd);
                delete [] line;
                return;
            }
            line[hsize] = '\0';  // trim longer lines
            printf("%s\n", line);
        }
        // next page...
        printf(":");
        fflush(stdout);
        fgets(line, 10, stdin);
        if (line[0] != '\n') {
            close(fd);
            delete [] line;
            return;
        }
    }
    delete [] line;
}

int main (int argc, char** argv, char** envp) {
    size_t hsize = 0, vsize = 0;  // terminal dimensions, read from
                                  // the config file
    char command[129];   // buffer for commands
    command[128] = '\0';
    char* com_tok[129];  // buffer for the tokenized commands
    size_t num_tok;      // number of tokens

    printf("Remote shell v2.0.\n");

    // Initialize remote host and port to defaults
    strcpy(remote_host, "localhost");
    strcpy(remote_port, "8001");

    // Config:
    int confd = open(config, O_RDONLY);
    if (confd < 0) {
        perror("config");
        fprintf(stderr, "config: cannot open the configuration file.\n");
        fprintf(stderr, "config: will now attempt to create one.\n");
        confd = open(config, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
        if (confd < 0) {
            // cannot open the file, giving up.
            perror(config);
            fprintf(stderr, "config: giving up...\n");
            return 1;
        }
        close(confd);
        // re-open the file read-only
        confd = open(config, O_RDONLY);
        if (confd < 0) {
            // cannot open the file, we'll probably never reach this
            // one but who knows...
            perror(config);
            fprintf(stderr, "config: giving up...\n");
            return 1;
        }
    }

    // read configuration (terminal size and remote host/port)
    while (1) {
        int n = readline(confd, command, 128);
        if (n == recv_nodata)
            break;
        if (n < 0) {
            sprintf(command, "config: %s", config);
            perror(command);
            break;
        }
        
        // Handle empty lines
        if (strlen(command) == 0)
            continue;
            
        num_tok = str_tokenize(command, com_tok, strlen(command));
        if (num_tok == 0)
            continue;
        
        if (strcmp(com_tok[0], "VSIZE") == 0 && num_tok > 1 && atol(com_tok[1]) > 0)
            vsize = atol(com_tok[1]);
        else if (strcmp(com_tok[0], "HSIZE") == 0 && num_tok > 1 && atol(com_tok[1]) > 0)
            hsize = atol(com_tok[1]);
        else if (strcmp(com_tok[0], "RHOST") == 0 && num_tok > 1)
            strcpy(remote_host, com_tok[1]);
        else if (strcmp(com_tok[0], "RPORT") == 0 && num_tok > 1)
            strcpy(remote_port, com_tok[1]);
        // lines that do not make sense are hereby ignored
    }
    close(confd);

    if (hsize <= 0) {
        hsize = 75;
        confd = open(config, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
        write(confd, "HSIZE 75\n", strlen( "HSIZE 75\n"));
        close(confd);
        fprintf(stderr, "%s: cannot obtain a valid horizontal terminal size, will use the default\n", 
                config);
    }
    if (vsize <= 0) {
        vsize = 40;
        confd = open(config, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
        write(confd, "VSIZE 40\n", strlen( "VSIZE 40\n"));
        close(confd);
        fprintf(stderr, "%s: cannot obtain a valid vertical terminal size, will use the default\n",
                config);
    }

    printf("Terminal set to %ux%u.\n", (unsigned int)hsize, (unsigned int)vsize);
    printf("Remote server: %s:%s\n", remote_host, remote_port);

    // install the typical signal handler for zombie cleanup
    signal(SIGCHLD, zombie_reaper);

    // Command loop:
    while(1) {

        // Read input:
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(command, 128, stdin) == 0) {
            // EOF, will quit
            printf("\nBye\n");
            close_remote_connection();
            return 0;
        }
        // fgets includes the newline in the buffer, get rid of it
        if(strlen(command) > 0 && command[strlen(command) - 1] == '\n')
            command[strlen(command) - 1] = '\0';

        // Tokenize input:
        char** real_com = com_tok;
        num_tok = str_tokenize(command, real_com, strlen(command));
        real_com[num_tok] = 0;

        if (strlen(real_com[0]) == 0)  // no command
            continue;

        // Check for local command (prefixed with !)
        if (strcmp(real_com[0], "!") == 0) {
            // Local command
            if (num_tok < 2) {
                printf("! expects a command\n");
                continue;
            }
            
            real_com = com_tok + 1;  // skip the '!'
            
            // Process local commands
            if (strcmp(real_com[0], "exit") == 0) {
                printf("Bye\n");
                close_remote_connection();
                return 0;
            }
            else if (strcmp(real_com[0], "keepalive") == 0) {
                if (!keepalive_mode) {
                    keepalive_mode = 1;
                    printf("Keepalive mode enabled. Use '! close' to close the connection.\n");
                    // Try to open connection immediately
                    if (open_remote_connection() < 0) {
                        printf("Warning: Could not establish initial connection to remote server.\n");
                    }
                } else {
                    printf("Already in keepalive mode.\n");
                }
            }
            else if (strcmp(real_com[0], "close") == 0) {
                if (remote_socket >= 0) {
                    close_remote_connection();
                    printf("Connection closed. Keepalive mode disabled.\n");
                    keepalive_mode = 0;
                } else {
                    printf("No connection to close.\n");
                }
            }
            else if (strcmp(real_com[0], "more") == 0) {
                if (real_com[1] == 0)
                    printf("more: too few arguments\n");
                for (size_t i = 1; real_com[i] != 0; i++) 
                    do_more(real_com[i], hsize, vsize);
            }
            else {
                // External local command
                int bg = 0;
                char** exec_com = real_com;
                
                // Check for background flag
                if (strcmp(real_com[0], "&") == 0) {
                    bg = 1;
                    exec_com = real_com + 1;
                }
                
                if (bg) {
                    int bgp = fork();
                    if (bgp == 0) {
                        int r = run_it(exec_com[0], exec_com, envp, path);
                        printf("& %s done (%d)\n", exec_com[0], WEXITSTATUS(r));
                        if (r != 0) {
                            printf("& %s completed with a non-null exit code\n", exec_com[0]);
                        }
                        return 0;
                    }
                    else
                        continue;
                } else {
                    int r = run_it(exec_com[0], exec_com, envp, path);
                    if (r != 0) {
                        printf("%s completed with a non-null exit code (%d)\n", exec_com[0], WEXITSTATUS(r));
                    }
                }
            }
        }
        else {
            // Remote command
            int bg = 0;
            char** remote_com = real_com;
            
            // Check for background flag
            if (strcmp(real_com[0], "&") == 0) {
                bg = 1;
                remote_com = real_com + 1;
                if (num_tok < 2) {
                    printf("& expects a command\n");
                    continue;
                }
            }
            
            // Build the command string to send to server
            char remote_cmd[256] = "";
            for (size_t i = 0; remote_com[i] != 0; i++) {
                if (i > 0)
                    strcat(remote_cmd, " ");
                strcat(remote_cmd, remote_com[i]);
            }
            
#ifdef DEBUG
            fprintf(stderr, "%s: remote %s command: %s\n", 
                    __FILE__, bg ? "background" : "foreground", remote_cmd);
#endif
            
            if (!send_remote_command(remote_cmd, bg)) {
                printf("Failed to send remote command\n");
            }
        }
    }
}
