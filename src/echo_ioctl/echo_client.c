#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "echo_ioctl.h"

#define ECHO_IOCTL_CLEAR _IO(ECHO_IOCTL_MAGIC, ECHO_IOCTL_CLEAR_CMD)
#define ECHO_IOCTL_REVERSE _IO(ECHO_IOCTL_MAGIC, ECHO_IOCTL_REVERSE_CMD)

#define REVERSE "reverse"
#define CLEAR "clear"
#define DEVICE_NAME "/dev/echo"

int main(int argc, char *argv[]) 
{
    if (argc == 1)
    {
        printf("Missing argument. Synopsis:\n");
        printf("%s <command>\n\n", argv[0]);
        printf("Where <command> can be:\n");
        printf("- reverse\n");
        printf("- clear\n");
        return 0;
    }
    
    int command = 0;

    if (strcmp(argv[1], REVERSE) == 0)
    {
        printf("Sending 'reverse' command\n");
        command = ECHO_IOCTL_REVERSE;
    }
    else if (strcmp(argv[1], CLEAR) == 0)
    {
        printf("Sending 'clear' command\n");
        command = ECHO_IOCTL_CLEAR;
    }
    else
    {
        printf("Unknown command %s\n. Expected either one of: %s, %s", argv[1], REVERSE, CLEAR);
        return -1;
    }

    int fd = open(DEVICE_NAME, O_RDWR);
    if(fd < 0) {
        printf("Cannot open device file: %s\n", DEVICE_NAME);
        return -1;
    }
    int status = ioctl(fd, command);
    close(fd);
    return status;
}