#include <stdio.h>
#include "conion.h"

#ifdef STD_CONSOLE_INPUT
void initialize_console_io()
{
}

void restore_console_io()
{
}

int kbhit(void)
{
    return 0;
}
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>

static int initialized = 0;
static struct termios oldt;

void initialize_console_io(void)
{
    struct termios newt;

    if (initialized) return;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_iflag &= ~(ICRNL | INLCR);
    newt.c_oflag &= ~OPOST;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    initialized = 1;
}

void restore_console_io(void)
{
    if (!initialized) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    initialized = 0;
}

int  kbhit(void)
{
    fd_set set;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    return select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0;
}
#endif

int  getch(void)
{
    return getchar();
}
