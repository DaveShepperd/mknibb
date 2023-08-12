/* mknibb.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>

#ifndef O_BINARY
	#define O_BINARY (0)
#endif

extern char *optarg;
extern int optind, opterr, optopt;

static int helpEm(const char *progName)
{
	printf("Usage: %s [-hlv] [-f fill] [-s offset] [-t size] [-o outfile] filename\n"
		   "Where:\n"
		   "-f fill = set fill nibble (default 0) relevant only if -l or -h also provided).\n"
		   "-h = shift the upper nibble into the low bits\n"
		   "-l = include just the low 4 bit nibble (default is to leave both nibbles in place)\n"
		   "     Note: If neither -l nor -h is specified, bytes are left unmolested.\n"
		   "-n = Do not create a .bak file\n"
		   , progName);
	printf("-o outfile = path to output file. If not specified input file is clipped in place.\n"
		   "       (orignal file renamed with .bak unless -n is also provided)\n"
		   "-s offset = skip to offset in input file\n"
		   "-t size = size in bytes to clip file\n"
		   "-v = increase verbosity\n"
		   "filename = path to file\n"
		  );
	return 1;
}

int main(int argc, char *argv[])
{
	int sts, ifd, ofd, flags, opt, fill = 0, verbose = 0, noBak=0;
	struct stat st;
	int bufSize, outFnameLen, offset;
	const char *inFname, *cmdOutName, *userOutName;
	char *inBackupName;
	char *endp, *outFname;
	unsigned char *buf = NULL;

	flags = 0;
	bufSize = 0;
	offset = 0;
	userOutName = NULL;
	cmdOutName = NULL;
	inBackupName = NULL;
	while ( (opt = getopt(argc, argv, "f:hlno:s:t:v")) != -1 )
	{
		switch (opt)
		{
		case 'f':
			endp = NULL;
			fill = strtol(optarg, &endp, 0);
			if ( !endp || *endp || fill > 15 || fill < 0 )
			{
				fprintf(stderr, "Invalid fill parameter '%s'. Expected 0<=fill<=15.\n", argv[1]);
				return helpEm(argv[0]);
			}
			break;
		case 'l':
			flags = 1;
			break;
		case 'h':
			flags = 2;
			break;
		case 'n':
			noBak = 1;
			break;
		case 'o':
			cmdOutName = optarg;
			break;
		case 's':
			endp = NULL;
			offset = strtol(optarg, &endp, 0);
			if ( !endp || *endp )
			{
				fprintf(stderr, "Invalid -s offset parameter '%s'.\n", optarg);
				return helpEm(argv[0]);
			}
			break;
		case 't':
			endp = NULL;
			bufSize = strtol(optarg, &endp, 0);
			if ( !endp || *endp )
			{
				fprintf(stderr, "Invalid -t size parameter '%s'.\n", optarg);
				return helpEm(argv[0]);
			}
			break;
		case 'v':
			++verbose;
			break;
		default: /* '?' */
			return helpEm(argv[0]);
		}
	}
	if ( optind < 1 )
	{
		fprintf(stderr, "No input filename\n");
		return helpEm(argv[0]);
	}
	if ( !bufSize && !flags && !offset )
	{
		fprintf(stderr, "No -l, -h, -t or -s provided. Nothing to do.\n");
		return helpEm(argv[0]);
	}
	inFname = argv[optind];
	if ( verbose )
		printf("Checking on input file: %s\n", inFname);
	sts = stat(inFname, &st);
	if ( sts )
	{
		fprintf(stderr, "Error stat()'ing '%s': %s\n", inFname, strerror(errno));
		return 1;
	}
	if ( !bufSize )
		bufSize = st.st_size;
	buf = (unsigned char *)malloc(bufSize);
	if ( !buf )
	{
		fprintf(stderr,"Ran out of memory malloc()'ing %d bytes\n",bufSize);
		return 1;
	}
	ifd = open(inFname, O_RDONLY | O_BINARY);
	if ( ifd < 0 )
	{
		fprintf(stderr, "Error open()'ing '%s' for input: %s\n", inFname, strerror(errno));
		free(buf);
		return 1;
	}
	if ( offset )
	{
		if ( verbose )
			printf("Seeking to offset 0x%X in input file\n", offset);
		sts = lseek(ifd, offset, SEEK_SET);
		if ( sts != offset )
		{
			fprintf(stderr,"Failed to seek to offset 0x%X: %s\n", offset, strerror(errno));
			close(ifd);
			free(buf);
			return 1;
		}
	}
	sts = read(ifd, buf, bufSize);
	if ( sts != bufSize )
	{
		fprintf(stderr, "Error reading from '%s'. Expected %d bytes got %d: %s\n", inFname, bufSize, sts, strerror(errno));
		close(ifd);
		free(buf);
		return 1;
	}
	close(ifd);
	if ( verbose )
		printf("Read %d bytes from input file\n", sts);
	if ( !cmdOutName )
	{
		if ( !noBak )
		{
			sts = strlen(inFname) + 5;
			inBackupName = (char *)calloc(sts, 1);
			if ( !inBackupName )
			{
				fprintf(stderr, "Ran out of memory malloc()'ing %d bytes\n", sts);
				free(buf);
				return 1;
			}
			snprintf(inBackupName, sts, "%s.bak", inFname);
		}
		if ( verbose )
			printf("Defaulting output file to: %s\n", inFname);
		userOutName = inFname;
	}
	else
		userOutName = cmdOutName;
	outFnameLen = strlen(userOutName) + 8;
	outFname = (char *)calloc(outFnameLen,1);
#if _WIN32
	snprintf(outFname, outFnameLen - 1, "%s-tmp", userOutName);
	ofd = open(outFname,O_CREAT|O_WRONLY|O_BINARY,st.st_mode);
#else
	snprintf(outFname, outFnameLen - 1, "%sXXXXXX", userOutName);
	ofd = mkstemp(outFname);
#endif
	if ( ofd < 0 )
	{
		fprintf(stderr, "Error: Unable to make tmp filename from '%s': %s\n", outFname, strerror(errno));
		free(outFname);
		free(buf);
		return 1;
	}
	if ( verbose )
		printf("Opened temp file %s for output\n", outFname);
	if ( flags )
	{
		fill <<= 4;
		for ( sts = 0; sts < bufSize; ++sts )
		{
			if ( (flags & 2) )
				buf[sts] >>= 4;
			buf[sts] &= 0x0F;
			buf[sts] |= fill;
		}
	}
	if ( verbose )
		printf("Writing %d bytes to output\n", bufSize);
	sts = write(ofd, buf, bufSize);
	if ( sts != bufSize )
	{
		fprintf(stderr, "Error writing to '%s'. Expected to write %d, wrote %d: %s\n", outFname, bufSize, sts, strerror(errno));
		close(ofd);
		unlink(outFname);
		free(outFname);
		free(buf);
		return 1;
	}
	close(ofd);
	if ( verbose )
		printf("Wrote %d bytes to output.\n", bufSize);
	if ( inBackupName )
	{
		if ( verbose )
			printf("Pre-deleting old backup file: %s\n", inBackupName);
		sts = unlink(inBackupName);
		if ( sts < 0 && errno != ENOENT )
		{
			fprintf(stderr, "Error deleting %s: %s\n", inBackupName, strerror(errno));
			free(inBackupName);
			free(buf);
			return 1;
		}
		if ( verbose )
			printf("Rename %s to %s ...\n", inFname, inBackupName);
		sts = rename(inFname, inBackupName);
		if ( sts < 0 )
		{
			fprintf(stderr, "Error renaming %s to %s: %s\n", inFname, inBackupName, strerror(errno));
			free(inBackupName);
			free(buf);
			return 1;
		}
		free(inBackupName);
	}
	if ( verbose )
		printf("Predelting %s\n", userOutName);
	sts = unlink(userOutName);
	if ( sts < 0 && errno != ENOENT )
	{
		fprintf(stderr, "Error (%d) deleting '%s': %s\n", errno, userOutName, strerror(errno));
		unlink(outFname);
		free(outFname);
		free(buf);
		return 1;
	}
	if ( verbose )
		printf("Renaming temp file %s to %s\n", outFname, userOutName);
	sts = rename(outFname, userOutName);
	if ( sts < 0 )
	{
		fprintf(stderr, "Error renaming '%s' to '%s': %s\n", outFname, userOutName, strerror(errno));
		free(outFname);
		free(buf);
		return 1;
	}
#if !_WIN32
	if ( verbose )
		printf("Set the file mode bits\n");
	sts = chmod(userOutName, st.st_mode);
	if ( sts < 0 )
	{
		fprintf(stderr, "Error setting file mode bits on '%s': %s\n", userOutName, strerror(errno));
		free(outFname);
		free(buf);
		return 1;
	}
#endif
	free(outFname);
	free(buf);
	return 0;
}

