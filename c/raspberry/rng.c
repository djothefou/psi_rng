/**
	This code is heavilly based on Giorgio Vazzana code, explained here :
	http://holdenc.altervista.org/avalanche/

	I just modify it a bit and add a RASPBERRY option to test the program on my computer.
*/

/*
 * avalanche - reads a random bit sequence generated by avalanche noise in a
 * pn junction and appends the bits to a file every hour
 * If SIGINT is received, append the sequence acquired so far and exit
 *
 * Copyright 2012 Giorgio Vazzana
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Compile: gcc -Wall -O avalanche_hour.c -o avalanche_hour */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

//Wether or not we are on a Raspberry, if not use software generated number
//#define RASPBERRY 1

// GPIO Avalanche input
#define AVALANCHE_IN  4
#define LED           9
//We write on the fifo file each 20ms
//NEED TO BE A MULTIPLE OF 8
#define NSAMPLES      (8*5)
//In micro seconds
#define DELAY         500
#define FIFO_FILE "../rng_fifo"

// GPIO registers address
#define BCM2708_PERI_BASE  0x20000000
#define GPIO_BASE          (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define BLOCK_SIZE         (256)

// GPIO setup macros. Always use GPIO_IN(x) before using GPIO_OUT(x) or GPIO_ALT(x,y)
#define GPIO_IN(g)    *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define GPIO_OUT(g)   *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET(g) *(gpio+7)  = 1<<(g)  // sets   bits which are 1, ignores bits which are 0
#define GPIO_CLR(g) *(gpio+10) = 1<<(g)  // clears bits which are 1, ignores bits which are 0
#define GPIO_LEV(g) (*(gpio+13) >> (g)) & 0x00000001

int                mem_fd;
void              *gpio_map;
volatile uint32_t *gpio;
static volatile sig_atomic_t keep_going = 1;

void signal_handler(int sig)
{
	keep_going = 0;
}

#ifdef RASPBERRY
//
// Set up a memory regions to access GPIO
//
void setup_io()
{
	/* open /dev/mem */
	mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
	if (mem_fd == -1) {
		perror("Cannot open /dev/mem");
		exit(1);
	}

	/* mmap GPIO */
	gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
	if (gpio_map == MAP_FAILED) {
		perror("mmap() failed");
		exit(1);
	}

	// Always use volatile pointer!
	gpio = (volatile uint32_t *)gpio_map;

	// Configure GPIOs
	GPIO_IN(AVALANCHE_IN); // must use GPIO_IN before we can use GPIO_OUT

	GPIO_IN(LED);
//	GPIO_OUT(LED);
}

//
// Release GPIO memory region
//
void close_io()
{
	int ret;

	/* munmap GPIO */
	ret = munmap(gpio_map, BLOCK_SIZE);
	if (ret == -1) {
		perror("munmap() failed");
		exit(1);
	}

	/* close /dev/mem */
	ret = close(mem_fd);
	if (ret == -1) {
		perror("Cannot close /dev/mem");
		exit(1);
	}
}
#endif
int main(int argc, char *argv[])
{
	uint32_t i, j, bit;
	uint32_t nb_numbers = 0;
	uint8_t *samples;
	FILE *fp;
	//We store numbers as bytes
	uint8_t current_number = 0;

#ifdef RASPBERRY
	// Setup gpio pointer for direct register access
	setup_io();
#endif

	samples = calloc(NSAMPLES/8, sizeof(*samples));
	if (!samples) {
		printf("Error on calloc()\n");
		exit(1);
	}

	signal(SIGINT, signal_handler);

	printf("Start Random numbers generation.\n");

    /* Create the FIFO if it does not exist */
    umask(0);
    mknod(FIFO_FILE, S_IFIFO|0666, 0);

	while (keep_going) {
		for (i = 0; i < NSAMPLES; i++) {
			if (!keep_going)
				break;
#ifdef RASPBERRY
			bit = GPIO_LEV(AVALANCHE_IN);
#else
			bit = rand() % 2;
#endif
			if(bit){
				current_number = (current_number << 1) | 0x01;
			}
			else{
				current_number = (current_number << 1);
			}

			if((i+1) % 8 == 0){
				samples[nb_numbers] = current_number;
				current_number=0;
				nb_numbers++;
			}
/*
			if (bit)
				GPIO_SET(LED);
			else
				GPIO_CLR(LED);
*/
			usleep(DELAY);
		}

		printf("Trying to open Fifo.\n");
		fp = fopen(FIFO_FILE, "a");
		printf("After trying to open Fifo.\n");
		if (fp) {
			printf("Fifo is open.\n");
			for (j = 0; j < nb_numbers; j++)
				fprintf(fp, "%u", samples[j]);
			fclose(fp);
			printf("Writing %d numbers", nb_numbers);
		}
		else{
			perror("Open fifo error");
		}

		nb_numbers = 0;
	}

#ifdef RASPBERRY
	close_io();
#endif

	return 0;
}
