#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void main(int argc,char **argv)
{
	int fd,cmd;

	if(argc!=2){
		printf("arvc is 2,value is 0 or 1\n");
	}
	cmd = atoi(argv[1]);
	
	if((fd = open("/dev/itop4412_relay_device", O_RDWR|O_NOCTTY|O_NDELAY))<0)
		printf("open %s failed\n");   
	else{
		ioctl(fd,cmd);		
    }
	close(fd);
}





