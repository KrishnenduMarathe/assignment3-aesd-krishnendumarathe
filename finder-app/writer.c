#include <stdio.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char** argv)
{
    // Open connection to syslog
    openlog(NULL, 0, LOG_USER);

    if (argc != 3)
    {
        syslog(LOG_ERR, "Wrong number of Arguments");
        closelog();
        return 1;
    }

    FILE* fptr = fopen(argv[1], "w");
    if (fptr == NULL)
    {
        syslog(LOG_ERR, "Failed to create file: %m");
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
    int err = fprintf(fptr, "%s\n", argv[2]);
    if (err < 0)
    {
        syslog(LOG_ERR, "Failed to write to file: %m");
        closelog();
        return 1;
    }

    fclose(fptr);

    return 0;
}