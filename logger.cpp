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

#define DEV_NAME "/dev/ttyACM0"
#define BAUD_RATE B460800
#define DATA_LENGTH 79

#define NEW_FILE_EVENT    0x03
#define FILE_CLOSE_EVENT  0x04
#define APP_EXIT_EVENT    0x07

using namespace std;

std::queue<char> gx3_queue;
ofstream gx3_logfile;
string gx3_filename;

const char stop[3] = {'\xFA','\x75','\xB4'};
char mode[4] = {'\xD4','\xA3','\x47','\x00'};
const char preset[4] = {'\xD6','\xC6','\x6B','\xCC'};
const char set_timer[8] = {'\xD7','\xC1','\x29','\x01','\x00','\x00','\x00','\x00'};
unsigned char reply[7];

static int fd, i;

static float extract_float(unsigned char* addr)
{
  float tmp;

  *((unsigned char*)(&tmp) + 3) = *(addr);
  *((unsigned char*)(&tmp) + 2) = *(addr+1);
  *((unsigned char*)(&tmp) + 1) = *(addr+2);
  *((unsigned char*)(&tmp)) = *(addr+3);

  return tmp;
}

static int extract_int(unsigned char* addr)
{
  int tmp;

  *((unsigned char*)(&tmp) + 3) = *(addr);
  *((unsigned char*)(&tmp) + 2) = *(addr+1);
  *((unsigned char*)(&tmp) + 1) = *(addr+2);
  *((unsigned char*)(&tmp)) = *(addr+3);

  return tmp;
}

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

void serial_init(int fd) {
    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_cc[VTIME] = 100;
    // ボーレートの設定
    cfsetispeed(&tio, BAUD_RATE);
    cfsetospeed(&tio, BAUD_RATE);
    // デバイスに設定を行う
    tcsetattr(fd, TCSANOW, &tio);
}

void gx3_25_start()
{
    //fd = open(DEV_NAME, O_RDWR | O_NONBLOCK );
    //fd = open(DEV_NAME, O_RDWR|O_NOCTTY|O_SYNC);
    fd = open(DEV_NAME, O_RDWR);
    if(fd < 0)
    {
        printf("Failed to open serial port\r\n");
        exit(1);
    }
    serial_init(fd); // シリアルポートの初期化
    
    write(fd,stop,3);
    usleep(100);

check_mode:
    write(fd,mode,4);
    i = read(fd,reply,4);
    if(!validate_checksum(reply,4))
    {
        printf("Recheck mode\r\n");
        goto check_mode;
    }
    //printf("check mode reply\r\n");
    
    if(reply[2] != '\x01')
    {
        mode[3] = '\x01';
        write(fd,mode,4);
        i = read(fd,reply,4);
        //printf("set mode to active reply\r\n");
        if(!validate_checksum(reply,4))
        {
            printf("Failed to set mode to active\r\n");
            close(fd);
            exit(1);
        }
    }
    
    write(fd,preset,4);
    i = read(fd,reply,4);
    //printf("set continuous mode preset reply\r\n");
    if(!validate_checksum(reply,4))
    {
        printf("Failed to set continuous mode preset\r\n");
        close(fd);
        exit(1);
    }
    
    mode[3] = '\x02';
    write(fd,mode,4);
    i = read(fd,reply,4);
    //printf("set mode to continous output reply\r\n");
    if(!validate_checksum(reply,4))
    {
        printf("Failed to set mode to continous output\r\n");
        close(fd);
        exit(1);
    }
    
    write(fd,set_timer,8);
    i = read(fd,reply,7);
}

void gx3_25_stop()
{
	write(fd,stop,3);
	
	usleep(100);
	
	close(fd);
}

void *gx3_log_thread(void *threadid)
{
	long tid;
	tid = (long)threadid;
	
	bool start = false;
	int k;
	float acc[3];
	float ang_vel[3];
	float mag[3];
	float M[9]; //rotation matrix
	float qw,qx,qy,qz;	
	double deltaT;
	
	unsigned char *data = (unsigned char*)malloc(DATA_LENGTH);
	
	while(1)
	{
		if(!gx3_queue.empty())
		{
			switch(gx3_queue.front())
			{
			case NEW_FILE_EVENT:
				printf("\r\n[INFO] create new GX3 log file.\r\n");
				if(gx3_logfile.is_open())
					gx3_logfile.close();
				gx3_logfile.open(gx3_filename.c_str());
				assert(!gx3_logfile.fail());
				//write headers
				if(gx3_logfile.is_open())
				{
					gx3_logfile<<"timestamp,";
					gx3_logfile<<"ax(g),ay,az,";
					gx3_logfile<<"wx(rad/s),wy,wz,";
					gx3_logfile<<"mx(Gauss),my,mz,";
					gx3_logfile<<"qw,qx,qy,qz,";
					gx3_logfile<<endl;
				}
				
				gx3_25_start();
				printf("sensor ready");
				
				start = true;
				
				break;
			case FILE_CLOSE_EVENT:
				printf("\r\n[INFO] close GX3 log file.\r\n");
				
				if(start)
					gx3_25_stop();
				
				if(gx3_logfile.is_open())
					gx3_logfile.close();
				
				start = false;
				break;
			case APP_EXIT_EVENT:
				printf("\r\n[INFO] exit GX3 thread.\r\n");
				
				if(start)
					gx3_25_stop();
				
				if(gx3_logfile.is_open())
					gx3_logfile.close();
				
				start = false;
				
				free(data);
				pthread_exit(NULL);
				break;
			default:
				break;
			}
			gx3_queue.pop();
		}
		
		if(start)
		{
			i = read(fd,data,DATA_LENGTH);

			if(!validate_checksum(data,DATA_LENGTH))
			{
				printf("Failed to checksum on message\r\n");
				continue;
			}
			k = 1;
			for (unsigned int j = 0; j < 3; j++, k += 4)
				acc[j] = -extract_float(&(data[k]));
			for (unsigned int j = 0; j < 3; j++, k += 4)
				ang_vel[j] = -extract_float(&(data[k]));
			for (unsigned int j = 0; j < 3; j++, k += 4)
				mag[j] = -extract_float(&(data[k]));
			for (unsigned int j = 0; j < 9; j++, k += 4)
				M[j] = extract_float(&(data[k]));
			deltaT = extract_int(&(data[k])) / 62500.0;

			qw = sqrt((1.0+M[0]+M[4]+M[8])/2.0);

			qx = (M[7]-M[5])/(4.0*qw);

			qy = (M[2]-M[6])/(4.0*qw);

			qz = (M[3]-M[1])/(4.0*qw);
			
			if(gx3_logfile.is_open())
			{
				gx3_logfile<<deltaT<<",";
				gx3_logfile<<acc[2]<<","<<acc[1]<<","<<acc[0]<<",";
				gx3_logfile<<ang_vel[2]<<","<<ang_vel[1]<<","<<ang_vel[0]<<",";
				gx3_logfile<<mag[2]<<","<<mag[1]<<","<<mag[0]<<",";
				gx3_logfile<<qw<<","<<qx<<","<<qy<<","<<qz;
				gx3_logfile<<endl;
			}
		}
	}
}

int main()
{
	char key = 0;
	pthread_t gx3_thread;
	
	pthread_create(&gx3_thread,NULL,gx3_log_thread,(void *)1234);

	while(1)
	{
		cout<<endl;
		cout<<"c: create new file for logging data"<<endl;
		cout<<"s: close and save data file"<<endl;
		cout<<"q: save data file and exit the application"<<endl;
		cout<<"Input your selection:";
		key = getchar();
		if(key == 'c')
		{	
			string str;
			cout<<"Input the new filename:";
			//getline(cin,filename);
			cin>>str;
			gx3_filename.clear();
			//gx3_filename = "gx3_";
			gx3_filename.append(str);
			gx3_filename.append(".txt");
			gx3_queue.push(NEW_FILE_EVENT);
		}
		else if(key == 's')
		{
			gx3_queue.push(FILE_CLOSE_EVENT);
		}
		else if(key == 'q')
		{
			gx3_queue.push(APP_EXIT_EVENT);
			pthread_join(gx3_thread, NULL);
			exit(0);
		}
	}
 
	return 0;
}
