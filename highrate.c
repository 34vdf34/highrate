/*
 * highrate - simple text file reader and writer
 * 
 * Copyright (C) 2022  Resilience Theatre 
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 * 
 * ---
 * 
 * Reads position CSV file and writes it to fifo with defined interval.
 * This is used for high rate target simulation with 'edgemap.php'.
 * 
 * 1. Input file format (from gpsbabel conversion): 
 * 
 * [lat],[lon],
 * 
 * 2. Output format (to highrate fifo):
 * 
 * [lat], [lon],[target_name],[target_symbol]
 * 
 * These are read from highrate.ini
 * 
 * target_name: 	CHARLIE-1
 * target_symbol: 	SNGPU-------
 * 
 * 3. Adjust sleep time in ini
 * 
 *   - 50 ms 	20 Hz high rate GPS source
 *   - 100 ms 	10 Hz high rate GPS source
 *   - 200 ms  	5 Hz high rate GPS source
 *   - 1000 ms	1 Hz default GPS source
 *   
 * 4. Generating data
 * 
 * You can use 'gpsbabel' to interpolate 1 s source file (to 10 Hz) with:
 * 
 *  # gpsbabel -i gpx -f [source].gpx -x interpolate,time=0.1  -o csv -F live-10-Hz.csv
 * 
 * You can use 'gpsbabel' to interpolate 1 s source file (to 20 Hz) with:
 * 
 *  # gpsbabel -i gpx -f [source].gpx -x interpolate,time=0.05  -o csv -F live-20-Hz.csv
 * 
 * You can use excellent gwsocket to bridge fifo -> websocket (server):
 * 
 *  # gwsocket --pipein=/tmp/wscontrol
 * 
 * [1] https://github.com/allinurl/gwsocket.git
 * 
 */

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h> 
#include <stdlib.h>
#include <time.h>
#include "log.h"
#include "ini.h"

#define BUF_SIZE 500

char *simulation_file="";
char *simulation_target="";
char *interval_wait="";
char *target_symbol="";

void write_pipe(char *string) {
	FILE *fptr;
	fptr = fopen("/tmp/wscontrol","w");
	if(fptr == NULL)
	{
		log_error("[%d] cannot open pipe file for writing",getpid());
		exit(1);             
	}
	fprintf(fptr,"%s",string);
	fclose(fptr);
}

int main(int argc, char *argv[])
{
	char *ini_file=NULL;
	int c=0;
	int log_level=LOG_INFO;
	while ((c = getopt (argc, argv, "dhi:")) != -1)
	switch (c)
	{
		case 'd':
			log_level=LOG_DEBUG;
			break;
		case 'i':
			ini_file = optarg;
			break;
		case 'h':
			log_info("[%d] highrate - high rate Target simulator ",getpid());
			log_info("[%d] Usage: -i [ini_file] ",getpid());
			log_info("[%d]        -d debug log ",getpid());
			log_info("[%d] ",getpid());
			log_info("[%d] Simulation file is csv file with lat,lon on each row",getpid());
			log_info("[%d] You can use 'gpsbabel' to convert files to csv format:",getpid());
			log_info("[%d] gpsbabel -i gpx -f [input].gpx -o csv -F [output].csv",getpid()); 
			log_info("[%d] Generate target_symbol (to ini file) at: ",getpid()); 
			log_info("[%d] https://spatialillusions.com/unitgenerator/ ",getpid()); 
			return 1;
		break;
			default:
			break;
	}
	if (ini_file == NULL) 
	{
		log_error("[%d] ini file not specified, exiting. ", getpid());
		return 0;
	}
	/* Set log level: LOG_INFO, LOG_DEBUG */
	log_set_level(log_level);
	/* read ini-file */
	ini_t *config = ini_load(ini_file);
	ini_sget(config, "highrate", "simulation_file", NULL, &simulation_file); 
	ini_sget(config, "highrate", "simulation_target", NULL, &simulation_target); 	
	ini_sget(config, "highrate", "interval_wait_ms", NULL, &interval_wait); 	
	ini_sget(config, "highrate", "target_symbol", NULL, &target_symbol); 	
	log_info("[%d] Simulation file: %s ",getpid(),simulation_file); 
	log_info("[%d] Simulation target: %s ",getpid(),simulation_target); 
	log_info("[%d] Interval (ms): %s ",getpid(),interval_wait);
	log_info("[%d] Symbol code: %s ",getpid(),target_symbol);
	
	char outbuf[BUF_SIZE];
	// 
	// Read file 
	// 
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen(simulation_file, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);


	log_info("[%d] Simulation stream started. ",getpid());
    while ((read = getline(&line, &len, fp)) != -1) {
		// Parse LAT, LON
		char *pt;
		pt = strtok (line,",");
		int x = 0;
		char lat[100];
		char lon[100];
		while (pt != NULL) {			
			if (x == 0) {
				strcpy(lat,pt);
			}
			if (x == 1) {
				strcpy(lon,pt);
			}
			x++;
			pt = strtok (NULL, ",");
		}
		log_debug("[%d] writing: %s,%s,%s,%s ",getpid(),lat,lon,simulation_target,target_symbol);  
		memset(outbuf,0,BUF_SIZE);
		sprintf(outbuf,"%s,%s,%s,%s",lat,lon,simulation_target,target_symbol);  
		write_pipe(outbuf);
		x=0;
		// Sleep time from ini file in ms (50,100 or 200 ms)
		if ( atoi(interval_wait) == 50 ) {
			usleep(50000); 		// 20 Hz = 50 ms intervals 
		} 
		if ( atoi(interval_wait) == 100 ) {
			usleep(100000); 	// 10 Hz = 100 ms intervals
		}
		if ( atoi(interval_wait) == 200 ) {
			usleep(200000); 	// 5 Hz = 200 ms intervals
		}
		if ( atoi(interval_wait) == 1000 ) {
			sleep(1); 			// 1 Hz = 1 s intervals
		}
		
    }
    fclose(fp);
    log_info("[%d] Simulation stream closed. ",getpid());
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
    
  return 0;
}
