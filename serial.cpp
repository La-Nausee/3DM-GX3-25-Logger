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

const speed_t baud_rate[] = {B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600,\
			    B19200, B38400, B57600, B115200, B230400, B460800};
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
	int tmp;
	unsigned int baud_sel = 0;
	string port;
	struct termios tio;
	
	const char stop[3] = {'\xFA','\x75','\xB4'};
	int  length = 0, index = 0;
	char* cmd = (char*) malloc(20);
	unsigned char* responese = (unsigned char*) malloc(20);
	
	cout<<"Device Port:";
	cin>>port;
	cout<<"Baudrate:"<<endl;
	cout<<"0:B0,      1:B50,      2:B75,      3:B100"<<endl;
	cout<<"4:B134,    5:B150,     6:B200,     7:B300"<<endl;
	cout<<"8:B600,    9:B1200,    10:B1800,   11:B2400"<<endl;
	cout<<"12:B4800,  13:B9600,   14:B19200,  15:B38400"<<endl;
	cout<<"16:B57600, 17:B115200, 18:B230400, 19:B460800"<<endl;
	cout<<"Your Selection:";
	cin>>std::dec>>baud_sel;
	if(baud_sel > 19)
	{
		cout<<"Invalid parameter:"<<std::dec<<baud_sel<<endl;
		return -1;
	}
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
    	cfsetispeed(&tio, baud_rate[baud_sel]);
    	cfsetospeed(&tio, baud_rate[baud_sel]);
    	tcsetattr(fd, TCSANOW, &tio);
	
	write(fd,stop,3);
	
	while(1)
	{
		cout<<"Number of bytes to send(range from 1 to 20, others will exit this program): ";
		cin>>std::dec>>length;
		if(length == 0 || length > 20)
			break;
		index = 0;
		while(index < length)
		{
			cout<<"Byte["<<std::dec<<index<<"] = ";
			cin>>std::hex>>tmp;
			cmd[index++] = char(tmp);	
		}
		cout<<"Your data: "<<endl;
		for(index = 0; index < length; index++)
			cout<<std::hex<<int(cmd[index])<<" ";
		cout<<endl;
		cout<<"Number of response(range from 1 to 20, others will exit this program): ";
		cin>>std::dec>>length;
		if(length > 20)
			break;
		write(fd,cmd,index);
		length = read(fd,responese,length);
		if(!validate_checksum(responese,length))
		{
			cout<<"Failed to checksum on message"<<endl;
			continue;
		}
		cout<<"Response in Hex: "<<endl;
		for(index = 0; index < length; index++)
			cout<<std::hex<<int(responese[index])<<" ";
		cout<<endl;
	}
	
	write(fd,stop,3);
	
	free(cmd);
	free(responese);
	close(fd);
	
	return 0;
}
