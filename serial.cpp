#include <fstream>
#include <iostream>
#include <cassert>
#include <cstring>
#include <queue>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <math.h>

using namespace std;

bool validate_checksum(const unsigned char *data, unsigned short length)
{
  unsigned short chksum = 0;
  unsigned short rchksum = 0;

  for (unsigned short i = 0; i < length - 2; i++)
    chksum += data[i];

  rchksum = data[length - 2] << 8;
  rchksum += data[length - 1];

  return (chksum == rchksum);
}

int main()
{
	int fd;
	string port;
	//speed_t baud_rate;
	struct termios tio;
	
	const char stop[3] = {'\xFA','\x75','\xB4'};
	size_t  length = 0, index = 0;
	char* cmd = (char*) malloc(20);
	unsigned char* responese = (char*) malloc(20);
	
	cout<<"Device Port:";
	cin>>port;
	//cout<<"Baud Rate:";
	//cin>>baud_rate;
	if((fd = open(port.c_str(), O_RDWR)) < 0)
  	{
		cout<<"Failed to open "<<port<<endl;
		return -1;
   	}
	
	memset(&tio, 0, sizeof(tio));
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_cc[VTIME] = 100;
    cfsetispeed(&tio, B460800);
    cfsetospeed(&tio, B460800);
    tcsetattr(fd, TCSANOW, &tio);
	
	write(fd,stop,3);
	
	while(1)
	{
		cout<<"Number of bytes to send(range from 1 to 20, others will exit this program): "<<endl;
		cin>>length;
		if(length == 0 || length > 20)
			break;
		index = 0;
		while(index < length)
		{
			cout<<"Byte["<<index<<"] = ";
			cin>>std::hex>>cmd[index++];
		}
		cout<<"Number of response(range from 1 to 20, others will exit this program): "<<endl;
		cin>>length;
		if(length == 0 || length > 20)
			break;
		read(fd,responese,length);
		if(!validate_checksum(responese,length))
		{
			cout<<"Failed to checksum on message"<<endl;
			continue;
		}
		cout<<"Response in Hex: ";
		for(index = 0; index < length; index++)
			cout<<std::hex<<responese[index]<<" ";
	}
	
	write(fd,stop,3);
	
	free(cmd);
	free(responese);
	close(fd);
	
	return 0;
}
