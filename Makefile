CC = gcc
CFLAGS = -Wall -ansi

.SILENT:

mknibb: mknibb.c Makefile
	$(CC) $(CFLAGS) -o $@ $<
	
clean: Makefile
	rm -f mknibb

