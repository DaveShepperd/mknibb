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

extern char *optarg;
extern int optind, opterr, optopt;
	   
static int helpEm(const char *progName)
{
	 printf("Usage: %s [-hl] [-f fill] [-t size] [-o outfile] filename\n"
			"Where:\n"
			"-f fill = set fill nibble (default 0) only relevant if -l or -h also provided).\n"
			"-l = include just the low 4 bit nibble (default is to leave both nibbles in place)\n"
			"-h = shift the upper nibble into the low bits\n"
			"-o outfile = path to output file. If not specified input file is clipped in place\n"
			"-t size = size to clip file (must be one of: 32, 64, 128, 256, 512 or 1024.)\n"
			"filename = path to file\n"
			,progName
			);
	 return 1;
}

int main(int argc, char *argv[])
{
	 int sts, ifd, ofd, flags, opt, fill=0;
	 struct stat st;
	 int bufSize, outFnameLen;
	 unsigned char buf[256];
	 const char *inFname, *userOutName;
	 char *endp, *outFname;

	 flags = 0;
	 bufSize = 0;
	 userOutName = NULL;
	 while ( (opt = getopt(argc, argv, "f:hlo:t:")) != -1 )
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
			 if (!endp || *endp || bufSize < 32 || bufSize > 256 || ((bufSize&-bufSize) != bufSize) )
			 {
				 fprintf(stderr,"Invalid size parameter '%s'.\n", argv[1]);
				 return helpEm(argv[0]);
			 }
			 break;
		 default: /* '?' */
			 return helpEm(argv[0]);
		 }
	}
	if (optind < 1)
	{
		fprintf(stderr,"No filename\n");
		return helpEm(argv[0]);
	}
	if ( !bufSize && !flags )
	{
		fprintf(stderr,"No -l, -h or -t provided. Nothing to do.\n");
		return helpEm(argv[0]);
	}
	inFname = argv[optind];
	sts = stat(inFname,&st);
	if (sts)
	{
		fprintf(stderr,"Error stat()'ing '%s': %s\n", inFname, strerror(errno));
		return 1;
	}
	if (st.st_size < (size_t)bufSize)
	{
		fprintf(stderr,"Error: File size of %ld is smaller than %d\n", st.st_size, bufSize);
		return 1;
	}
	if ( st.st_size == bufSize && !flags )
	{
		fprintf(stderr,"Warning: File is already %d bytes and no -l or -h option selected. Nothing to do.\n", bufSize);
		return 1;
	}
	ifd = open(inFname, O_RDONLY);
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
	if ( !userOutName )
		userOutName = inFname;
	outFnameLen = strlen(userOutName) + 8;
	outFname = (char *)malloc(outFnameLen);
	snprintf(outFname,outFnameLen-1,"%sXXXXXX", userOutName);
	outFname[outFnameLen-1] = 0;
	if ( (ofd=mkstemp(outFname)) < 0 )
	{
		fprintf(stderr,"Error: Unable to make tmp filename from '%s': %s\n", outFname, strerror(errno));
		free(outFname);
		return 1;
	}
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
	sts = unlink(userOutName);
	if (sts < 0 && errno != ENOENT )
	{
		fprintf(stderr,"Error (%d) deleting '%s': %s\n", errno, userOutName, strerror(errno));
		unlink(outFname);
		free(outFname);
		return 1;
	}
	sts = rename(outFname,userOutName);
	if (sts < 0)
	{
		fprintf(stderr,"Error renaming '%s' to '%s': %s\n", outFname, userOutName, strerror(errno));
		free(outFname);
		return 1;
	}
	free(outFname);
	return 0;
}

