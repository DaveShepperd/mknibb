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

#define MAX_BUFSIZE (8192)

extern char *optarg;
extern int optind, opterr, optopt;
	   
static int helpEm(const char *progName)
{
	 printf("Usage: %s [-hlv] [-f fill] [-t size] [-o outfile] filename\n"
			"Where:\n"
			"-f fill = set fill nibble (default 0) only relevant if -l or -h also provided).\n"
			"-l = include just the low 4 bit nibble (default is to leave both nibbles in place)\n"
			"-h = shift the upper nibble into the low bits\n"
			,progName);
	 printf("-o outfile = path to output file. If not specified input file is clipped in place (orignal file renamed with .bak)\n"
			"-t size = size in bytes to clip file (must be integer power of 2. I.e.: 32, 64, 128, etc.; max of %d)\n"
			"-v = increase verbosity\n"
			"filename = path to file\n"
			,MAX_BUFSIZE
			);
	 return 1;
}

static unsigned char buf[MAX_BUFSIZE];	/* Put this outside to keep the stack from getting too deep. */

int main(int argc, char *argv[])
{
	 int sts, ifd, ofd, flags, opt, fill=0, verbose=0;
	 struct stat st;
	 int bufSize, outFnameLen;
	 const char *inFname, *userOutName;
	 char *endp, *outFname, *inBackupName=NULL;

	 flags = 0;
	 bufSize = 0;
	 userOutName = NULL;
	 while ( (opt = getopt(argc, argv, "f:hlo:t:v")) != -1 )
	 {
		 switch (opt)
		 {
		 case 'f':
			 endp = NULL;
			 fill = strtol(optarg,&endp,0);
			 if (!endp || *endp || fill > 15 || fill < 0 )
			 {
				 fprintf(stderr,"Invalid fill parameter '%s'. Expected 0<=fill<=15.\n", argv[1]);
				 return helpEm(argv[0]);
			 }
			 break;
		 case 'l':
			 flags = 1;
			 break;
		 case 'h':
			 flags = 2;
			 break;
		 case 'o':
			 userOutName = optarg;
			 break;
		 case 't':
			 endp = NULL;
			 bufSize = strtol(optarg,&endp,0);
			 if (!endp || *endp || bufSize < 32 || bufSize > MAX_BUFSIZE || ((bufSize&-bufSize) != bufSize) )
			 {
				 fprintf(stderr,"Invalid -t size parameter '%s'.\n", optarg);
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
	if (optind < 1)
	{
		fprintf(stderr,"No input filename\n");
		return helpEm(argv[0]);
	}
	if ( !flags )
	{
		fprintf(stderr,"No -l, -h or -t provided. Nothing to do.\n");
		return helpEm(argv[0]);
	}
	inFname = argv[optind];
	if ( verbose )
		printf("Checking on input file: %s\n", inFname);
	sts = stat(inFname, &st);
	if (sts)
	{
		fprintf(stderr,"Error stat()'ing '%s': %s\n", inFname, strerror(errno));
		return 1;
	}
	if ( !bufSize )
	{
		bufSize = st.st_size;
		if ( verbose )
			printf("Defaulting file size to %d\n", bufSize);
		if ( ((bufSize&-bufSize) != bufSize) )
		{
			fprintf(stderr,"File size not a multiple of power of 2. size=0x%X\n", bufSize);
			return 1;
		}
		if ( bufSize < 32 || bufSize > (int)sizeof(buf) )
		{
			fprintf(stderr,"Input file size is %ld. Cannot be less than 32 or more than %ld\n", st.st_size, sizeof(buf));
			return 1;
		}
	}
	if ( st.st_size < (size_t)bufSize )
	{
		fprintf(stderr,"Error: File size of %ld is smaller than %d\n", st.st_size, bufSize);
		return 1;
	}
	if ( st.st_size == bufSize && !flags )
	{
		fprintf(stderr,"Warning: File is already %d bytes and no -l or -h option selected. Nothing to do.\n", bufSize);
		return 1;
	}
	ifd = open(inFname, O_RDONLY|O_BINARY);
	if (ifd < 0)
	{
		fprintf(stderr,"Error open()'ing '%s' for input: %s\n", inFname, strerror(errno));
		return 1;
	}
	sts = read(ifd,buf,bufSize);
	if (sts != bufSize)
	{
		fprintf(stderr,"Error reading from '%s'. Expected %d bytes got %d: %s\n", inFname, bufSize, sts, strerror(errno));
		close(ifd);
		return 1;
	}
	close(ifd);
	if ( verbose )
		printf("Read %d bytes from input file\n", sts);
	if ( !userOutName )
	{
		sts = strlen(inFname)+5;
		inBackupName = (char *)calloc(sts,1);
		if ( !inBackupName )
		{
			fprintf(stderr,"Ran out of memory malloc()'ing %d bytes\n", sts);
			return 1;
		}
		snprintf(inBackupName,sts,"%s.bak",inFname);
		if ( verbose )
			printf("Pre-deleting old backup file: %s\n", inBackupName);
		sts = unlink(inBackupName);
		if ( sts < 0 && errno != ENOENT )
		{
			fprintf(stderr,"Error deleting %s: %s\n", inBackupName, strerror(errno));
			return 1;
		}
		if ( verbose )
			printf("Rename %s to %s ...\n", inFname,inBackupName);
		sts = rename(inFname,inBackupName);
		if ( sts < 0 )
		{
			fprintf(stderr,"Error renaming %s to %s: %s\n", inFname, inBackupName, strerror(errno));
			return 1;
		}
		if ( verbose )
			printf("Defaulting output file to: %s\n", inFname);
		userOutName = inFname;
		free(inBackupName);
	}
	outFnameLen = strlen(userOutName) + 8;
	outFname = (char *)malloc(outFnameLen);
	snprintf(outFname,outFnameLen-1,"%sXXXXXX", userOutName);
	outFname[outFnameLen-1] = 0;
	if ( (ofd = mkostemp(outFname,O_BINARY)) < 0 )
	{
		fprintf(stderr,"Error: Unable to make tmp filename from '%s': %s\n", outFname, strerror(errno));
		free(outFname);
		return 1;
	}
	if ( verbose )
		printf("Opened temp file %s for output\n", outFname);
	if ( flags )
	{
		fill <<= 4;
		for (sts=0; sts < bufSize; ++sts)
		{
			if ( (flags&2) )
				buf[sts] >>= 4;
			buf[sts] &= 0x0F;
			buf[sts] |= fill;
		}
	}
	if ( verbose )
		printf("Writing %d bytes to output\n", bufSize);
	sts = write(ofd, buf, bufSize);
	if (sts != bufSize)
	{
		fprintf(stderr,"Error writing to '%s'. Expected to write %d, wrote %d: %s\n", outFname, bufSize, sts, strerror(errno));
		close(ofd);
		unlink(outFname);
		free(outFname);
		return 1;
	}
	close(ofd);
	if ( verbose )
		printf("Wrote %d bytes to output.\nPredelting %s\n", bufSize, userOutName);
	sts = unlink(userOutName);
	if (sts < 0 && errno != ENOENT )
	{
		fprintf(stderr,"Error (%d) deleting '%s': %s\n", errno, userOutName, strerror(errno));
		unlink(outFname);
		free(outFname);
		return 1;
	}
	if ( verbose )
		printf("Renaming temp file %s to %s\n", outFname, userOutName);
	sts = rename(outFname, userOutName);
	if (sts < 0)
	{
		fprintf(stderr,"Error renaming '%s' to '%s': %s\n", outFname, userOutName, strerror(errno));
		free(outFname);
		return 1;
	}
	if ( verbose )
		printf("Set the file mode bits\n");
	sts = chmod(userOutName,st.st_mode);
	if (sts < 0)
	{
		fprintf(stderr,"Error setting file mode bits on '%s': %s\n", userOutName, strerror(errno));
		free(outFname);
		return 1;
	}
	free(outFname);
	return 0;
}

