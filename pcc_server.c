#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

// Based on the script we saw in rec. 10
int connfd = -1;
int sigint_flag;
size_t message_size;
uint32_t pcc_total[127] = {0}; // The structure that counts how many times each printable character was observed in all clients, cells 0-31 would be 0 (I will ignore them)

// Special handling for SIGINT signal
void handle_sigint(int sig)
{
    int i;
    if (connfd < 0) // not processing a client - should print the statistic
    {
        for (i = 32; i < 127; i++) {
            printf("char '%c' : %u times\n", i, pcc_total[i]);
        }
        exit(0);
    } else sigint_flag = 1;  // caught in server's while loop after client processing finished
}

int main(int argc, char *argv[]) {
    int notwritten, totalsent, nsent, listenfd, bytes_read, i;
    uint32_t N, C = 0;
    char buffer[1000000]; // Allocations of up to 1MB are OK
    uint32_t local_pcc_total[95] = {0};
    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    const int enable = 1;

    // Check if the number of arguments is 4
    if (argc != 2) {
        fprintf(stderr, "number of arguments incorrect, ERROR: %s\n", strerror(errno));
        exit(1);
    }

    // SIGINT handler, the struct is similar to the one I implement in hw2 (myShell)
    // Used https://man7.org/linux/man-pages/man2/sigaction.2.html and http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
    struct sigaction sa;
    sa.sa_handler = &handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0) == -1) {
        fprintf(stderr, "Couldn't resister signal handle. Error: %s\n", strerror(errno));
        exit(1);
    }

    // Create TCP connection
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) // Case of error: https://man7.org/linux/man-pages/man2/listen.2.html
    {
        fprintf(stderr, "couldn't create socker, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    // Used https://man7.org/linux/man-pages/man7/socket.7.html and example https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt failed, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr *) &serv_addr, addrsize) != 0) {
        fprintf(stderr, "Bind Failed, ERROR: %s\n", strerror(errno));
        exit(1);
    }

    if (listen(listenfd, 10) != 0) {
        fprintf(stderr, "Listen Failed, ERROR: %s\n", strerror(errno));
        exit(1);
    }

    while (1) {
        // Check if we had SIGINT signal - in this case we should print the number of time that printable char was observed
        if (sigint_flag) {
            for (i = 32; i < 127; i++) {
                printf("char '%c' : %u times\n", i, pcc_total[i]);
            }
            break;
        }
        // Initialize the local array to zero
        for (i = 0; i < 127; i++) {
            local_pcc_total[i] = 0;
        }

        // Accept a connection.
        // I use NULL in 2nd and 3rd argument because I don't want to print the client socket details
        connfd = accept(listenfd, NULL, NULL);

        if (connfd < 0) {
            printf("\n Error : Accept Failed. %s \n", strerror(errno));
            return 1;
        }

        // Receive N from Client, the value is a 32-bit unsigned integer
        totalsent = 0;
        notwritten = 4; // The size is 32-bit (4 bytes)
        while (notwritten > 0) {
            bytes_read = read(connfd, (char *) &N + totalsent, notwritten);
            if (bytes_read <= 0) {
                if (bytes_read == 0 || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
                    fprintf(stderr, "Couldn't receive N from the client, ERROR: %s\n", strerror(errno));
                    close(listenfd);
                    connfd = -1;
                    break;
                } else {
                    fprintf(stderr, "Couldn't receive N from the client, ERROR: %s\n", strerror(errno));
                    close(listenfd);
                    exit(1);
                }
            }
            totalsent += bytes_read;
            notwritten -= bytes_read;
        }
        if (connfd == -1) continue; // For accepting new client connection
        N = ntohl(N);
        // Receive N bytes that are file content, make and print the statistic
        totalsent = 0;
        notwritten = N; // The size is 32-bit (4 bytes)
        while (notwritten > 0) {
            if (sizeof(buffer) < notwritten) // we want to read the exact number of bits
                message_size = sizeof(buffer);
            else message_size = (size_t) notwritten; // Should be size_t format for using read system call (https://man7.org/linux/man-pages/man2/read.2.html)
            bytes_read = read(connfd, (char *) &buffer, message_size);
            if (bytes_read <= 0) {
                if (bytes_read == 0 || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
                    fprintf(stderr, "Couldn't receive N from the client, ERROR: %s\n", strerror(errno));
                    close(connfd);
                    connfd = -1;
                    break;
                } else {
                    fprintf(stderr, "Couldn't receive N from the client, ERROR: %s\n", strerror(errno));
                    close(listenfd);
                    exit(1);
                }
            }
            // Update the statistic from this run in the local_pcc_total
            for (i = 0; i < bytes_read; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) // printable char.
                {
                    local_pcc_total[(int) (buffer[i])]++; // The cast is for having the value as integer and minus 32 is for the array cells (I have specific order)
                    C++; // We read printable char.
                }
            }
            totalsent += bytes_read;
            notwritten -= bytes_read;
        }
        if (connfd == -1) continue; // For accepting new client connection
        C = htonl(C); // Change the format, we want to send C

        // Send C - printable char.
        totalsent = 0;
        notwritten = 4; // The size is 32-bit (4 bytes)
        while (notwritten > 0) // We haven't sent all the content
        {
            // Read content from the file
            bytes_written = write(connfd, (char *) &C + totalsent, notwritten);
            if (bytes_written <= 0) {
                if (bytes_written == 0 || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
                    fprintf(stderr, "Couldn't send C to the client, ERROR: %s\n", strerror(errno));
                    close(connfd);
                    connfd = -1;
                    break;
                } else {
                    fprintf(stderr, "Couldn't send C to the client, ERROR: %s\n", strerror(errno));
                    close(listenfd);
                    exit(1);
                }
            }
            totalsent += bytes_written;
            notwritten -= bytes_written;
        }
        if (connfd == -1) continue; // For accepting new client connection

        // Finished reading and send necessary data, this is time to update the main structure
        for (i = 32; i < 127; i++) {
            pcc_total[i] += local_pcc_total[i];
        }
        close(connfd);
        connfd = -1;
        C = 0;
        N = 0;
        for (i = 32; i < 127; i++) {
            printf("char '%c' : %u times\n", i , pcc_total[i]);
        }
    }
}
