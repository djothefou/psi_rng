/*
 * rng - reads a random bit sequence generated by avalanche noise in a
 * pn junction and appends the bits to a fifo file which is read by a websockets server
 *
 *  This code is heavilly based on Giorgio Vazzana code, explained here :
 *  http://holdenc.altervista.org/avalanche/
 *
 * Copyright 2013 Giorgio Vazzana, Andréas Livet
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "daemonize.h"
#include "fifo.h"
#include "qrand.h"

//In microseconds
#define SLEEP_INTERVAL 500
//In milliseconds
#define SAMPLE_DURATION 100

static volatile sig_atomic_t keep_going = 1;

void build_byte(uint32_t bit, uint32_t position, uint8_t *samples){
    //We store numbers as bytes
    static uint8_t current_number = 0;
    //We build an uint8 number with random bits
    if(bit)
        current_number = (current_number << 1) | 0x01;
    else
        current_number = (current_number << 1);

    if((position+1) % 8 == 0){
        samples[(position+1) / 8] = current_number;
        current_number=0;
    }
}

//Send numbers to the websocket server through a fifo
void send_numbers(uint8_t *samples, uint32_t len){
    FILE *fp;

    fp = fopen(FIFO_FILE, "wb");
    if (fp) 
    {
        fwrite(samples, 1, len, fp);
        fclose(fp);
    }
    else 
    {
        perror("error opening fifo");
        exit(1);
    }
}

void signal_handler(int sig)
{
    keep_going = 0;
}

int main(int argc, char *argv[])
{
    uint32_t bit;
    uint32_t i;
    uint32_t nb_bits_samples = (SAMPLE_DURATION * 1000) / SLEEP_INTERVAL;
    uint32_t nb_bytes_samples = nb_bits_samples / 8;
    uint8_t samples[nb_bytes_samples];

    signal(SIGINT, signal_handler);

    if(argc > 1 && strcmp(argv[1], "-d") == 0)
        daemonize();

    create_fifo_and_wait("Waiting for server to start...", "Server started : could start Random numbers generation.");

    qrand_setup();
    while (keep_going)
    {
        for (i = 0; i < nb_bits_samples; i++) 
        {
            bit = qrand();
            build_byte(bit, i, (uint8_t*)&samples);
            usleep(SLEEP_INTERVAL);
        }
        send_numbers((uint8_t*)&samples, nb_bytes_samples);
    }
    qrand_close();
    return 0;
}
