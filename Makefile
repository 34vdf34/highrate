CC=gcc

EXTRA_WARNINGS = -Wall 

LIBS = -lcurl -lxml2 -lsqlite3

CFLAGS=-ggdb $(EXTRA_WARNINGS)

BINS=highrate

all: highrate

highrate: highrate.c log.c ini.c
	 $(CC) $+ $(CFLAGS) $(LIBS) -o $@ -I.

	 
clean:
	rm -rf $(BINS)
	rm *.o
	
