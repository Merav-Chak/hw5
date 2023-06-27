#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h> // for open file

// Based on the script we saw in rec. 10

int main(int argc, char *argv[])
{
    int fd, notwritten, totalsent, nsent;
    int  sockfd     = -1;
    size_t message_size;
    ssize_t  bytes_read;
    char buffer[1000000]; // Allocations of up to 1MB are OK
    off_t file_s;
    uint32_t file_size, N, C;
    struct sockaddr_in serv_addr; // where we Want to get to

    // Check if the number of arguments is 4
    if (argc != 4)
    {
        fprintf(stderr, "number of arguments incorrect, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    // Open the file, only read mode
    fd = open(argv[3], O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "couldn't open the file, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    // Create TCP connection
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "couldn't create socket, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) != 1)
    {
        fprintf(stderr, "IP address is not valid, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    // connect socket to the target address
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Connect Failed, ERROR: %s\n", strerror(errno));
        exit(1);
    }
    // Check the size of the file
    file_s = lseek(fd, 0, SEEK_END);
    if (file_s == -1)
    {
        fprintf(stderr, "Failed to seek to the end of the file, ERROR: %s\n", strerror(errno));
        close(fd);
        exit(1);
    }
    file_size = (uint32_t)file_s; // Convert file_s to uint32_t
    // Return to the beginning of the file
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        fprintf(stderr, "Failed to seek to the beginning of the file, ERROR: %s\n", strerror(errno));
        close(fd);
        exit(1);
    }
    N = htonl(file_size); // The htonl() function converts the unsigned integer hostlong from host byte order to network byte order.

    // Sending N to server
    // notwritten = how much we have left to write
    // totalsent  = how much we've written so far
    // nsent = how much we've written in last write() call
    totalsent = 0;
    notwritten = 4; // The size of the file 32-bit (4 bytes)
    while (notwritten > 0)
    {
        nsent = write(sockfd, (char *) &N + totalsent, notwritten);
        if (nsent < 0)
        {
            close(sockfd);
            fprintf(stderr, "Writing data to server failed, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        totalsent  += nsent;
        notwritten -= nsent;
    }

    // Sent N bytes which contains the file content
    totalsent = 0;
    notwritten = file_s; // file_s is the file size as int type
    while (notwritten > 0) // We haven't sent all the content
    {
        // Read content from the file
        if (sizeof(buffer) < notwritten) // we want to read the exact number of bits
            message_size = sizeof(buffer);
        else message_size = (size_t)notwritten; // Should be size_t format for using read system call (https://man7.org/linux/man-pages/man2/read.2.html)
        bytes_read = read(fd, buffer, message_size);
        if (bytes_read == -1)
        {
            fprintf(stderr, "Couldn't read data from file, ERROR: %s\n", strerror(errno));
            close(sockfd);
            exit(1);
        }
        nsent = write(sockfd, buffer, bytes_read);
        if (nsent < 0)
        {
            close(sockfd);
            fprintf(stderr, "Couldn't send file content to the server, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        totalsent  += nsent;
        notwritten -= nsent;
        if (lseek(fd, totalsent, SEEK_SET) == -1)
        {
            fprintf(stderr, "Failed to seek to specific place of the file, ERROR: %s\n", strerror(errno));
            close(sockfd);
            close(fd);
            exit(1);
        }
    }
    close(fd);

    // Receive C (number of printable characters), the value is a 32-bit unsigned integer
    totalsent = 0;
    notwritten = 4; // The size is 32-bit (4 bytes)
    while (notwritten > 0)
    {
        bytes_read = read(sockfd, (char *) &C + totalsent, notwritten);
        if (bytes_read < 0)
        {
            close(sockfd);
            fprintf(stderr, "Couldn't receive C from the server, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        totalsent  += nsent;
        notwritten -= nsent;
    }
    close(sockfd);
    C = ntohl(C);
    printf("# of printable characters: %u\n", C);
    exit(0);
}
