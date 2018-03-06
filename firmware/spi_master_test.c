#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay = 110;

int spi_open()
{
    int ret;
    int fd = open(device, O_RDWR);
	if (fd < 0)
	{
		perror("can't open device \n");
		return -1;
	}
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
	{
	    perror("can't set spi mode \n");
	    return -1;
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
	{
		perror("can't get spi mode \n");
		return -1;
	}
	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		perror("can't set bits per word \n");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
	    perror("can't get bits per word");
	    return -1;
	}
	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
	    perror("can't set max speed hz");
	    return -1;
	}
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		perror("can't get max speed hz");
		return -1;
	}

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	
	return fd;
}

int spi_send_cmd(int fd, uint8_t cmd, uint8_t idx, uint8_t value)
{
    int ret;
    uint8_t tx[3]; 
    uint8_t rx[3] = {0, };
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = 3,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits
	};
	tx[0] = cmd;
	tx[1] = idx;
	tx[2] = value;
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        perror("can't send spi message");
    	return -1;
    }
    //if(cmd == 255)
    // printf("RX %d %d %d \n", rx[0], rx[1], rx[2]);
    return (int) rx[0];
}

int spi_set(int fd,uint8_t idx, uint8_t v)
{
    return spi_send_cmd(fd,1, idx, v);
}

int spi_get(int fd,uint8_t idx)
{
    // send command
    int ret;
    ret = spi_send_cmd(fd,0,idx,0);
    if(ret == -1) return -1;
    // read back
    return spi_send_cmd(fd,255,255,255);
}

void spi_read_buff(int fd,uint8_t* buf, int size)
{
    int ret;
    uint8_t tx[size];
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)buf,
		.len = size,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits
	};
	spi_send_cmd(fd,2,0,size);
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        perror("can't send spi message");
    	return;
    }
}

void spi_write_buff(int fd, uint8_t* buf, int size)
{
    int i;
    for(i=0; i < size; i++)
        spi_set(fd,i,buf[i]);
    
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	int _cmd;
	int _value;
	int _idx;
	
	if(argc != 3)
	{
	    printf("spi cmd value");
	}
    _idx = atoi(argv[1]);
    _value = atoi(argv[2]);
    
    fd = spi_open();
    
    if(fd == -1) return -1;
    printf("device is opened\n");
    
    uint8_t data[64];
    for(ret = 0; ret < 64; ret++)
        data[ret] = ret +1;
    
    //for(ret = 0)
    spi_write_buff(fd,data,64);
    
	spi_set(fd,_idx,_value);
	
	spi_read_buff(fd,data,64);
	
	for(ret = 0; ret < 64; ret ++)
	{
	    printf("%d ", data[ret]);
	    //data[ret] = ret+1;
	}
	printf("\n");
	
	//spi_write_buff(fd,data,8);
	close(fd);

	return 1;
}
