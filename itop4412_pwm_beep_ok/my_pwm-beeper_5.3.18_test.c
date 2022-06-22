
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

#include <linux/input.h>
#include <linux/kernel.h>

struct input_event_
{
	struct timeval time;
	unsigned short int type;
	unsigned short int code;
	signed int value;
};

static void help(void)
{
	printf("Usage:./%s 1 to ON\n", __FILE__);
	printf("Usage:./%s 0 to OFF\n", __FILE__);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		help();
		return 1;
	}
	int fd = open("/dev/input/event2", O_RDWR);
	if (fd < 0)
	{
		perror("[open]");
		return 1;
	}
	int duty = atoi(argv[1]);
	struct input_event_ event;
	event.type = EV_SND;
	event.code = SND_BELL;
	event.value = duty;
	if (write(fd, &event, sizeof(event)) < 0)
	{
		perror("[write]");
		goto error;
	}
	close(fd);
	printf("done!\n");
	return 0;
error:
	close(fd);
	return 1;
}

