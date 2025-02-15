/* Copyright (C) Teemu Suutari */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "archivefs_api.h"
#include "archivefs_integration.h"
#include "testing/CRC32.h"

struct FIB
{
	uint32_t				diskKey;
	uint32_t				dirEntryType;
	uint8_t					filename[108];
	uint32_t				protection;
	uint32_t				entryType;
	uint32_t				size;
	uint32_t				blocks;
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;
	uint8_t					comment[80];
	uint16_t				uid;
	uint16_t				gid;
};

static const char protectionLetters[]="dewrapsh";
static const char *monthNames[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static void printFIB(const struct FIB *fib)
{

	struct tm *tm;
	time_t time;
	int i,len=strlen((char*)fib->filename);
	printf("%s",fib->filename);
	while (len++<24) printf(" ");
	if (fib->dirEntryType==2) printf("    Dir ");
		else printf("%7u ",fib->size);
	for (i=7;i>=0;i--)
		printf("%c",((fib->protection^0xf)&(1<<i))?protectionLetters[i]:'-');

	time=(fib->mtimeDays+2922)*86400+fib->mtimeMinutes*60+fib->mtimeTicks/50;
	tm=gmtime(&time);
	printf(" %02d-%s-%02d %02d:%02d:%02d\n",tm->tm_mday,monthNames[tm->tm_mon],tm->tm_year%100,tm->tm_hour,tm->tm_min,tm->tm_sec);
	if (*fib->comment)
		printf(": %s\n",fib->comment);
}

static int test_registerFunc(const char *path,const void *fib)
{
	printf("Full path '%s'\n",path);
	printFIB(fib);
	return -1;
}

/* just a hack */
static int allocatedFileCount=0;
static void *allocatedFiles[1000];
static const char *allocatedFilenames[1000];
static void *archive;
int toProcess,toSkip;

static void *test_allocFunc(const char *name,uint32_t length)
{
	void *ret;
	uint32_t verify;

	if (!toProcess) return 0;
	if (toSkip)
	{
		toSkip--;
		return (void*)-1;
	}
	toProcess--;

	/* test that different part of the APIs return same info */
	verify=archivefs_getFileSize(archive,name);
	if (verify!=length)
	{
		printf("archivefs_getFileSize returned wrong answer %u (should be %u)\n",verify,length);
	}

	printf("File '%s', Length %u\n",name,length);
	ret=archivefs_malloc(length);
	if (!ret && !length) ret=(void*)0x80000000U;
	allocatedFilenames[allocatedFileCount]=name;
	allocatedFiles[allocatedFileCount++]=ret;
	return ret;
}

static void *test_progressFunc(const char *name,uint32_t length)
{
	void *ret;

	ret=archivefs_malloc(length);
	if (!ret && !length) ret=(void*)0x80000000U;
	allocatedFilenames[allocatedFileCount]=name;
	allocatedFiles[allocatedFileCount++]=ret;
	return ret;
}

static void printProgress(uint32_t current,uint32_t max)
{
	uint32_t i,sep;

	sep=(current*50)/max;
	printf("\033[A[");
	for (i=0;i<sep;i++) printf("#");
	for (i=sep;i<50;i++) printf(" ");
	printf("]\n");
}

int main(int argc,char **argv)
{
	int32_t ret,i,j,length;
	void *verify;

	if (argc<3)
	{
		printf("no command or no archive file defined as a command line parameter\n");
		return 0;
	}


	ret=archivefs_initialize(&archive,argv[2]);
	if (ret)
	{
		printf("archivefs_initialize failed with code %d (%s)\n",ret,archivefs_getErrorString(ret));
		return 0;
	}
	if (!strcmp(argv[1],"filecache"))
	{
		toProcess=1000;
		toSkip=0;
		if (argc>=5)
		{
			toProcess=atoi(argv[3]);
			toSkip=atoi(argv[4]);
		}
		printf("----------------------------------------------------------------------------\n");
		printf("FileCache:\n");
		ret=archivefs_fileCache(archive,test_allocFunc);
		if (ret && ret!=WVFS_ERROR_OPERATION_CANCELED)
		{
			printf("archivefs_fileCache failed with code %d (%s)\n",ret,archivefs_getErrorString(ret));
			return 0;
		}
		for (i=0;i<allocatedFileCount;i++)
		{
			length=archivefs_getFileSize(archive,allocatedFilenames[i]);
			if (length<0)
			{
				printf("getFileSize failed with code %d (%s)\n",length,archivefs_getErrorString(length));
				return 0;
			} else {
				printf("CRC for '%s': 0x%08x\n",allocatedFilenames[i],CRC32(allocatedFiles[i],length));
				verify=archivefs_malloc(length);
				ret=archivefs_fileRead(archive,verify,allocatedFilenames[i],length,0);
				if (ret!=length) printf("archivefs_fileRead failed with code %d\n",ret);
				for (j=0;j<length;j++)
				{
					if (((char*)verify)[j]!=((char*)allocatedFiles[i])[j])
					{
						printf("verify failed for file %s\n",allocatedFilenames[i]);
						return 0;
					}
				}
				archivefs_free(verify);
			}
			if (((size_t)allocatedFiles[i])!=0x80000000U) archivefs_free(allocatedFiles[i]);
		}
		printf("----------------------------------------------------------------------------\n");
	} else if (!strcmp(argv[1],"examine")) {
		printf("----------------------------------------------------------------------------\n");
		printf("Examine:\n");
		ret=archivefs_dirCache(archive,test_registerFunc);
		if (ret && ret!=WVFS_ERROR_OPERATION_CANCELED)
		{
			printf("archivefs_dirCache failed with code %d (%s)\n",ret,archivefs_getErrorString(ret));
			return 0;
		}
		printf("----------------------------------------------------------------------------\n");
	} else if (!strcmp(argv[1],"read")) {
		const char *name;

		if (argc<4) return 0;
		name=argv[3];
		argc-=4;
		argv+=4;
		length=archivefs_getFileSize(archive,name);
		printf("----------------------------------------------------------------------------\n");
		printf("Read:\n");
		if (length<0)
		{
			printf("archivefs_getFileSize() failed with code %d (%s)\n",length,archivefs_getErrorString(length));
			return 0;
		}
		verify=archivefs_malloc(length);
		if (argc<2)
		{
			ret=archivefs_fileRead(archive,verify,name,length,0);
			if (ret!=length)
			{
				printf("archivefs_fileRead failed with code %d %s\n",ret,archivefs_getErrorString(ret));
				return 0;
			}
			printf("CRC for '%s': 0x%08x\n",name,CRC32(verify,length));
		} else {
			while (argc>=2)
			{
				uint32_t offset,partialLength;

				offset=atoi(argv[0]);
				partialLength=atoi(argv[1]);

				ret=archivefs_fileRead(archive,verify,name,partialLength,offset);
				if (ret!=partialLength)
				{
					printf("archivefs_fileRead failed with code %d %s\n",ret,archivefs_getErrorString(ret));
					return 0;
				}
				printf("CRC for '%s' @ (%u:%u): 0x%08x\n",name,offset,partialLength,CRC32(verify,partialLength));

				argc-=2;
				argv+=2;
			}
		}

		archivefs_free(verify);
		printf("----------------------------------------------------------------------------\n");
	} else if (!strcmp(argv[1],"progress")) {
		ret=archivefs_setProgressCallback(archive,printProgress);
		if (ret)
		{
			printf("archivefs_setProgressCallback failed with code %d (%s)\n",ret,archivefs_getErrorString(ret));
			return 0;
		}
		printf("\n");
		printProgress(0,1);
		ret=archivefs_fileCache(archive,test_progressFunc);
		if (ret)
		{
			printf("archivefs_fileCache failed with code %d (%s)\n",ret,archivefs_getErrorString(ret));
			return 0;
		}
		for (i=0;i<allocatedFileCount;i++)
			if ((size_t)(allocatedFiles[i])!=0x80000000U) archivefs_free(allocatedFiles[i]);
	} else {
		return -1;
	}
	archivefs_uninitialize(archive);
	return 0;
}
