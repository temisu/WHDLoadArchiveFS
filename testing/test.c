/* Copyright (C) Teemu Suutari */

#include "container_api.h"
#include "container_integration.h"
#include "testing/CRC32.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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

static const char protectionLetters[8]="dewrapsh";
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
	printf("Directory '%s'\n",path);
	printFIB(fib);
	return -1;
}

/* just a hack */
static int allocatedFileCount=0;
static void *allocatedFiles[1000];
static const char *allocatedFilenames[1000];
static void *container;
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
	verify=container_getFileSize(container,name);
	if (verify!=length)
	{
		printf("container_getFileSize returned wrong answer %u (should be %u)\n",verify,length);
	}

	printf("File '%s', Length %u\n",name,length);
	ret=container_malloc(length);
	allocatedFilenames[allocatedFileCount]=name;
	allocatedFiles[allocatedFileCount++]=ret;
	return ret;
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


	ret=container_initialize(&container,argv[2]);
	if (ret)
	{
		printf("container_initialize failed with code %d (%s)\n",ret,container_getErrorString(ret));
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
		ret=container_fileCache(container,test_allocFunc);
		if (ret)
		{
			printf("container_fileCache failed with code %d (%s)\n",ret,container_getErrorString(ret));
			return 0;
		}
		for (i=0;i<allocatedFileCount;i++)
		{
			length=container_getFileSize(container,allocatedFilenames[i]);
			if (length<0)
			{
				printf("getFileSize failed with code %d (%s)\n",length,container_getErrorString(length));
				return 0;
			} else {
				printf("CRC for '%s': 0x%08x\n",allocatedFilenames[i],CRC32(allocatedFiles[i],length));
				verify=container_malloc(length);
				ret=container_fileRead(container,verify,allocatedFilenames[i],length,0);
				if (ret!=length) printf("container_fileRead failed with code %d\n",ret);
				for (j=0;j<length;j++)
				{
					if (((char*)verify)[j]!=((char*)allocatedFiles[i])[j])
					{
						printf("verify failed for file %s\n",allocatedFilenames[i]);
						return 0;
					}
				}
				container_free(verify);
			}
			container_free(allocatedFiles[i]);
		}
		printf("----------------------------------------------------------------------------\n");
	} else if (!strcmp(argv[1],"examine")) {
		printf("----------------------------------------------------------------------------\n");
		printf("Examine:\n");
		ret=container_examine(container,test_registerFunc);
		if (ret)
		{
			printf("container_examine failed with code %d (%s)\n",ret,container_getErrorString(ret));
			return 0;
		}
		printf("----------------------------------------------------------------------------\n");
	} else if (!strcmp(argv[1],"read")) {
		if (argc<4) return 0;
		printf("----------------------------------------------------------------------------\n");
		printf("Read:\n");
		length=container_getFileSize(container,argv[3]);
		if (length<0)
		{
			printf("container_getFileSize() failed with code %d (%s)\n",length,container_getErrorString(length));
			return 0;
		}
		verify=container_malloc(length);
		ret=container_fileRead(container,verify,argv[3],length,0);
		if (ret!=length)
		{
			printf("container_fileRead failed with code %d %s\n",ret,container_getErrorString(ret));
			return 0;
		}
		printf("CRC for '%s': 0x%08x\n",argv[3],CRC32(verify,length));
		container_free(verify);
		printf("----------------------------------------------------------------------------\n");
	} else {
		return -1;
	}
	container_uninitialize(container);
	return 0;
}
