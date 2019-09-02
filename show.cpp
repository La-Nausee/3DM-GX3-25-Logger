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

#define SHOW_ACCEL	  			(1<<0)
#define SHOW_ANGVEL	  			(1<<1)
#define SHOW_MAG  	  			(1<<2)
#define SHOW_EULER     			(1<<3)
#define SHOW_Q		  			(1<<4)
#define SHOW_TIMESTAMP		    (1<<5)
#define SHOW_NONE    		    (1<<6)
#define APP_EXIT_EVENT    			(1<<7)

using namespace std;

std::queue<int> gx3_queue;
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
	int sensor;
	int k;
	float acc[3];
	float ang_vel[3];
	float mag[3];
	float M[9]; //rotation matrix
	float qw,qx,qy,qz;	
	float pitch, roll, yaw;
	double deltaT;
	
	unsigned char *data = (unsigned char*)malloc(DATA_LENGTH);

	while(1)
	{
		if(!gx3_queue.empty())
		{
			sensor = gx3_queue.front();
			switch(sensor)
			{
			case SHOW_ACCEL:
			case SHOW_ANGVEL:
			case SHOW_MAG:
			case SHOW_EULER:
			case SHOW_Q:
			case SHOW_TIMESTAMP:
				gx3_25_start();

				start = true;
				
				break;
			case SHOW_NONE:
				printf("\r\n[INFO] stop showing.\r\n");
				if(start)
					gx3_25_stop();

				start = false;
				break;
			case APP_EXIT_EVENT:
				printf("\r\n[INFO] exit GX3 thread.\r\n");
				
				if(start)
					gx3_25_stop();
				
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
			
			pitch = asin(-M[2])*180.0/M_PI;
			roll = atan2(M[5],M[8])*180.0/M_PI;
			yaw = atan2(M[1],M[0])*180.0/M_PI;

			switch(sensor)
			{
			case SHOW_ACCEL:
				printf("ax: %0.2f, ay: %0.2f, az: %0.2f\r\n", acc[0], acc[1], acc[2]);
				break;
			case SHOW_ANGVEL:
				printf("wx: %0.2f, wy: %0.2f, wz: %0.2f\r\n", ang_vel[0], ang_vel[1], ang_vel[2]);
				break;
			case SHOW_MAG:
				printf("mx: %0.2f, my: %0.2f, mz: %0.2f\r\n", mag[0], mag[1], mag[2]);
				break;
			case SHOW_EULER:
				printf("roll: %0.2f, pitch: %0.2f, yaw: %0.2f\r\n", roll, pitch, yaw);
				break;
			case SHOW_Q:
				printf("q0: %0.2f, q1: %0.2f, q2: %0.2f, q3: %0.2f\r\n", qw, qx, qy, qz);
				break;
			case SHOW_TIMESTAMP:
				printf("timestamp: %0.2f\r\n", deltaT);
				break;
			default:
				break;
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
		cout<<"a: show accelerator's data (g)"<<endl;
		cout<<"g: show gyroscope's data (rad/s)"<<endl;
		cout<<"m: show magnetometer's data (Gauss) "<<endl;
		cout<<"e: show euler angles (degree) "<<endl;
		cout<<"q: show quaternion"<<endl;
		cout<<"t: show timestamp"<<endl;
		cout<<"n: stop showing"<<endl;
		cout<<"x: exit"<<endl;
		cout<<"input your option:";
		key = getchar();
		if(key == 'a')
		{	
			gx3_queue.push(SHOW_ACCEL);
		}
		else if(key == 'g')
		{
			gx3_queue.push(SHOW_ANGVEL);
		}
		else if(key == 'm')
		{
			gx3_queue.push(SHOW_MAG);
		}
		else if(key == 'e')
		{
			gx3_queue.push(SHOW_EULER);
		}
		else if(key == 'q')
		{
			gx3_queue.push(SHOW_Q);
		}
		else if(key == 't')
		{
			gx3_queue.push(SHOW_TIMESTAMP);
		}
		else if(key == 'n')
		{
			gx3_queue.push(SHOW_NONE);
		}
		else if(key == 'x')
		{
			gx3_queue.push(APP_EXIT_EVENT);
			pthread_join(gx3_thread, NULL);
			exit(0);
		}
	}
 
	return 0;
}