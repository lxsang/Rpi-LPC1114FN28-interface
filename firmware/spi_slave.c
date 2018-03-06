
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cpu.h>

#include "delay.h"

#define BUF_SIZE 64
#define CMD_SIZE 3
#define M_FREQ 100
#define HIGH 0x1
#define LOW 0x0
#define FWD 0x1
#define BWD 0x2
#define ML_P (data_buffer + 4)
#define MR_P (data_buffer + 6)
#define SN_P (data_buffer + 2)
typedef struct{
    uint8_t vio;
    __IO uint32_t d1;
    __IO uint32_t d2;
} motor_t;

typedef struct{
    __IO uint32_t trig;
    __IO uint32_t echo;
} sonar_t;

uint8_t data_buffer[BUF_SIZE];
uint8_t cmd[CMD_SIZE];


motor_t ml = {
    .vio = 5,
    .d1 = PIO0_3,
    .d2 = PIO0_10
};

motor_t mr = {
    .vio = 6,
    .d1 = PIO1_1,
    .d2 = PIO1_5
};

sonar_t sonar = {
    .trig = PIO1_9,
    .echo = PIO1_8
};

int motor_init(motor_t m)
{
    // init pwm chanel
    if (pwm_init(m.vio, M_FREQ))
    {
      printf("ERROR: pwm_init() failed, %s\n", strerror(errno));
      return -1;
    }
    gpio_configure(m.d1, GPIO_MODE_OUTPUT);
    gpio_configure(m.d2, GPIO_MODE_OUTPUT);
    gpio_write(m.d1, LOW);
    gpio_write(m.d2, LOW);
    return 0;
}

void motor_set_speed(motor_t m, uint8_t s){
    int v = (int)s*  257;
    pwm_set(m.vio, v);
}
void motor_set_direction(motor_t m, uint8_t d)
{
    if(d == FWD)
    {
        gpio_write(m.d1, HIGH);
        gpio_write(m.d2, LOW);
    }
    else
    {
        gpio_write(m.d2, HIGH);
        gpio_write(m.d1, LOW);
    }
}

void sonar_init(sonar_t s)
{
    gpio_configure(s.trig, GPIO_MODE_OUTPUT);
    gpio_configure(s.echo, GPIO_MODE_INPUT);
}
void sonar_read(sonar_t s)
{
    int counter = 0;
    int max = 60000; //us
    //first reset the trigger
    gpio_write(s.trig,LOW);
    delay_us(5);
    // trigger is high for 10us to active the sensor
    gpio_write(s.trig,HIGH);
    delay_us(10);
    gpio_write(s.trig,LOW);
    //enable the system tick, enable interrup, use system clock
    SysTick->CTRL |= (1<<0)+(1<<2);
    SysTick->LOAD = DEFAULT_CPU_FREQ/100000 - 1;
    SysTick->VAL = 5;
    while(counter*10<max)
    {
        while(SysTick->VAL > 5){if(gpio_read(s.echo)==HIGH) break;}
        if(gpio_read(s.echo)==HIGH) break;
        counter++;
    }
    SysTick->CTRL &= ~((1<<0)+(1<<2));
    //echo is High, get the pulse width
    counter = 0;
    SysTick->CTRL |= (1<<0)+(1<<2);
    SysTick->LOAD = DEFAULT_CPU_FREQ/100000 - 1;
    SysTick->VAL = 5;
    while(gpio_read(s.echo) == HIGH )
    {
        while(SysTick->VAL > 5);
        counter++;
    }
    SysTick->CTRL &= ~((1<<0)+(1<<2));
    //convert to distance in cm
    counter = counter*10/57;
    *(SN_P) = counter & 0xFF; // low bytes
    *(SN_P+1) = (counter >> 8) & 0xFF; // high byte
}

void SPI0_IRQHandler(void)
{
    uint8_t *p;
    int i;
    p = (uint8_t *) &cmd;

    for (i = 0; i < CMD_SIZE; i++)
    {
        while (!(LPC_SSP0->SR & 0x04));
        *p++ = LPC_SSP0->DR;
    }
    
    switch(cmd[0])
    {
        case 0: // read by index
            while (!(LPC_SSP0->SR & 0x02));
            LPC_SSP0->DR = data_buffer[cmd[1]];
            break;
        case 1: // write by index
            data_buffer[cmd[1]] = cmd[2];
            break;
        case 2: // read all data 
            for(i = cmd[1]; i < cmd[2];i++)
            {
                while (!(LPC_SSP0->SR & 0x02));
                LPC_SSP0->DR = data_buffer[i];
            }
            break;
        default:;
    }

    // Drain the receive FIFO

    while (LPC_SSP0->SR & 0x04) (void) LPC_SSP0->DR;
}

void UART_IRQHandler(void)
{
    uint8_t c=0;
    int i;
    //gets(buf);
    __disable_irq();
    if(LPC_UART->LSR & 0x01)
        c = LPC_UART->RBR;
    printf("Command:%c \n", c);
    switch(c)
    {
        case 'd': // data 
            for(i = 0; i < BUF_SIZE; i++)
            {
                printf("%d ", data_buffer[i]);
                if((i+1)%8 == 0)
                    printf("\n");
            }
            break;
        case 'c': // clear 
            for(i = 0; i < BUF_SIZE;i++) data_buffer[i] = 0;
            break;
        default:;
            printf("Unknow command. Enter c (clear) or d (display)\n");
    }
    __enable_irq();
}

int main(void)
{
    int status;
    cpu_init(DEFAULT_CPU_FREQ);
    serial_stdio(CONSOLE_PORT);
    //init spi
    if ((status = spi_slave_init(0, 8, 3, SPI_MSBFIRST)))
    {
        printf("ERROR: spi_master_init() failed at line %d, %s\n", status, strerror(errno));
        exit(1);
    }
    
    NVIC_EnableIRQ(SSP0_IRQn);
    // enable interrupt
    LPC_SSP0->IMSC |= (1<<1);
    
    //uart interrupt
    NVIC_EnableIRQ(UART_IRQn);
    LPC_UART->IER |= 1<<0;
    
    motor_init(ml);
    motor_init(mr);
    sonar_init(sonar);
    
    for (;;)
    {
        motor_set_direction(ml,*ML_P);
        motor_set_direction(mr,*MR_P);
        motor_set_speed(ml, *(ML_P+1));
        motor_set_speed(mr, *(MR_P+1));
        sonar_read(sonar);
        delay_ms(200);
    }
}
