#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define N_BACKLOG 10

// async safe variable for signals
volatile sig_atomic_t grace_exit = 0;

// signal handler
void handle_termination(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        grace_exit = 1;
    }
}

int main()
{
    // initiate logging
    openlog(NULL, 0, LOG_USER);

    // setup signal handler
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handle_termination;
    act.sa_flags = SA_NODEFER;

    //  register signals
    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        syslog(LOG_ERR, "Failed to register SIGINT for handler with error: %s", strerror(errno));
        closelog();
        return -1;
    }
    if (sigaction(SIGTERM, &act, NULL) < 0)
    {
        syslog(LOG_ERR, "Failed to register SIGTERM for handler with error: %s", strerror(errno));
        closelog();
        return -1;
    }

    // define socket hints for localhost port 9000
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;


    int ret = getaddrinfo("localhost", "9000", &hints, &result);
    if (ret != 0)
    {
        if (grace_exit)
        {
            closelog();
            return 0;
        } else {
            syslog(LOG_ERR, "getaddrinfo failed with error: %s", gai_strerror(ret));
            closelog();
            return -1;
        }
    }

    int fd = -1; 
    int sfd = -1;
    struct addrinfo* itr;
    for (itr = result; itr != NULL; itr = itr->ai_next)
    {
        sfd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);
        if (sfd < 0) continue;

        // successful socket initiation, bind
        int status = bind(sfd, itr->ai_addr, itr->ai_addrlen);
        if (status == 0) break;

        // bind failed, close socket
        close(sfd);
        sfd = -1;
    }

    // free allocated addrinfo result
    freeaddrinfo(result);

    // no address found
    if (itr == NULL)
    {
        if (!grace_exit) syslog(LOG_ERR, "No address found to bind socker to. Last error: %s", strerror(errno));

        goto cleanup;
    }

    // on successful bind, start listening
    ret = listen(sfd, N_BACKLOG);
    if (ret < 0)
    {
        if (!grace_exit) syslog(LOG_ERR, "Failed to listen on the socket with error: %s", strerror(errno));

        goto cleanup;
    }

    // initiate data file
    fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        if (!grace_exit) syslog(LOG_ERR, "Failed to create/open /var/tmp/aesdsocketdata with error: %s", strerror(errno));

        goto cleanup;
    }

    while (grace_exit == 0)
    {
        // accept connection
        struct sockaddr_in6 caddr;
        unsigned int caddr_len = sizeof(caddr);
        int cfd = accept(sfd, (struct sockaddr *) &caddr, (socklen_t *) &caddr_len);
        if (cfd < 0)
        {
            if (!grace_exit) syslog(LOG_ERR, "Failed to accept connection with error: %s", strerror(errno));

            goto cleanup;
        }

        char cip[50];
        if (inet_ntop(AF_INET6, &caddr.sin6_addr, cip, 50) == NULL)
        {
            close(cfd);

            if (!grace_exit) syslog(LOG_ERR, "Failed to get client address with error: %s", strerror(errno));

            goto cleanup;
        }

        syslog(LOG_DEBUG, "Accepted connection from %s", cip);

        // interact with connection
        int buffsize = 512;
        char buf[buffsize];

        // dyanmic buffering for incoming data
        char* dynbuffer = NULL;
        unsigned int dynbuffersize = 0;

        while (grace_exit == 0)
        {
            // get data from client
            long int readsize = 0;
            memset(buf, 0, buffsize * sizeof(char));

            readsize = read(cfd, buf, buffsize * sizeof(char));
            if (readsize == 0)
            {
                // client finished sending data
                syslog(LOG_DEBUG, "Closed connection from %s", cip);
                if (dynbuffer != NULL) free(dynbuffer);
                close(cfd);
                break;
            }
            if (readsize < 0)
            {
                if (!grace_exit) syslog(LOG_ERR, "Failed to get data from client with error: %s", strerror(errno));
                if (dynbuffer != NULL) free(dynbuffer);
                close(cfd);
                
                goto cleanup;
            }

            // stream data till \n
            char *ptr = (char *) realloc(dynbuffer, dynbuffersize + readsize + 1);
            if (ptr == NULL)
            {
                if (!grace_exit) syslog(LOG_ERR, "Failed to reallocate dynamic buffer with error: %s", strerror(errno));
                if (dynbuffer != NULL) free(dynbuffer);
                close(cfd);

                goto cleanup;
            }
            dynbuffer = ptr;

            // copy over read data at the end
            memcpy(dynbuffer + dynbuffersize, buf, readsize);
            dynbuffersize += readsize;
            dynbuffer[dynbuffersize] = '\0';

            // check for newline character
            char* retptr;
            int newlinefound = 0;

            while (dynbuffer != NULL && (retptr = strchr(dynbuffer, '\n')) != NULL)
            {
                // new line found
                if (!newlinefound) newlinefound = 1;

                unsigned int length = (retptr - dynbuffer) + 1;

                // write to file
                ret = write(fd, dynbuffer, length * sizeof(char));
                if (ret < 0)
                {
                    if (!grace_exit) syslog(LOG_ERR, "Failed to write to /var/tmp/aesdsocketdata with error: %s", strerror(errno));
                    if (dynbuffer != NULL) free(dynbuffer);
                    close(cfd);
                    
                    goto cleanup;
                }

                // move remaining data to front of buffer
                unsigned int rem_length = dynbuffersize - length;

                if (rem_length > 0)
                {
                    // reallocate and move; length already accounts for \0
                    memmove(dynbuffer, dynbuffer+length, rem_length);
                    dynbuffersize = rem_length;
                    dynbuffer[dynbuffersize] = '\0';
                } else {
                    if (dynbuffer != NULL) free(dynbuffer);
                    dynbuffer = NULL;
                    dynbuffersize = 0;
                }

            }

            if (newlinefound)
            {
                newlinefound = 0;
                // seek back on the file to the beginning
                lseek(fd, 0, SEEK_SET);

                unsigned int fsize = lseek(fd, 0, SEEK_END);

                char* data = (char*) malloc((fsize+1) * sizeof(char));
                if (data == NULL)
                {
                    if (!grace_exit) syslog(LOG_ERR, "Failed to allocate for data with error: %s", strerror(errno));
                    if (dynbuffer != NULL) free(dynbuffer);
                    close(cfd);
                    
                    goto cleanup;
                }

                // send data to client
                lseek(fd, 0, SEEK_SET);

                ret = read(fd, data, fsize * sizeof(char));
                if (ret < 0)
                {
                    if (!grace_exit) syslog(LOG_ERR, "Failed to read from /var/tmp/aesdsocketdata with error: %s", strerror(errno));
                    if (dynbuffer != NULL) free(dynbuffer);
                    if (data != NULL) free(data);
                    close(cfd);
                    
                    goto cleanup;
                }
                data[fsize] = '\0';

                // restore file seek
                lseek(fd, 0, SEEK_END);

                ret = write(cfd, data, fsize * sizeof(char));
                if (ret < 0)
                {
                    if (!grace_exit) syslog(LOG_ERR, "Failed to send data to client with error: %s", strerror(errno));
                    if (dynbuffer != NULL) free(dynbuffer);
                    if (data != NULL) free(data);
                    close(cfd);
                    
                    goto cleanup;
                }

                if (data != NULL) free(data);
            }
        }

        if (dynbuffer != NULL)
        {
            free(dynbuffer);
            dynbuffer = NULL;
            dynbuffersize = 0;
        }
        close(cfd);
    }

cleanup:

    // gracefully exit
    if (sfd != -1) close(sfd);
    if (fd != -1) close(fd);

    // delete data file
    ret = unlink("/var/tmp/aesdsocketdata");
    if (ret < 0)
    {
        syslog(LOG_ERR, "Failed to remove /var/tmp/aesdsocketdata with error: %s", strerror(errno));
        closelog();
        return -1;
    }

    if (grace_exit)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        closelog();
        return 0;
    } else {
        closelog();
        return -1;
    }

    return 0;
}