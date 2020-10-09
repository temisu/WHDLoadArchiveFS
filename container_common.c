/* Copyright (C) Teemu Suutari */

#include "container_private.h"
#include "container_integration.h"

int container_common_simpleRead(void *dest,uint32_t length,uint32_t offset,struct container_state *container)
{
	int ret=container_integration_fileRead(dest,length,offset,container->file);
	if (ret<0) return ret;
	if (ret!=length) return CONTAINER_ERROR_INVALID_FORMAT;
	return ret;
}

static int container_isLeapYear(uint32_t year)
{
	return (!(year&3U)&&(year%100)?1:!(year%400));
}

static const uint16_t container_daysPerMonthAccum[12]={0,31,59,90,120,151,181,212,243,273,304,334};

uint32_t container_common_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince)
{
	uint32_t seconds,minutes,hours,day,month,year;
	uint32_t daysSinceEpoch;

	seconds=(timestamp&31)<<1;
	minutes=(timestamp>>5)&63;
	hours=(timestamp>>11)&31;
	day=(timestamp>>16)&31;
	month=(timestamp>>21)&15;
	year=((timestamp>>25)&0x7f)+1980;

	if (seconds>=60U||minutes>=60U||hours>=60U||!day||day>31||!month||month>12)
	{
		*minutesSince=0;
		*ticksSince=0;
		return 0;
	}
#if 0
	printf("time %u.%u.%u %u:%u:%u\n",year,month,day,hours,minutes,seconds);
#endif
	/* days since 1978.1.1 */
	daysSinceEpoch=day-1+container_daysPerMonthAccum[month-1];
	if (month>2&&container_isLeapYear(year)) daysSinceEpoch++;
	/* this is slow. could be optimized, but fortunately does not really matter since the max is 127+2+1980... */
	daysSinceEpoch+=(year-1978)*365;
	year=(year+3)&~3U;
	while (year>1980)
	{
		year-=4;
		if (container_isLeapYear(year))
			daysSinceEpoch++;
	}
	*minutesSince=hours*60+minutes;
	*ticksSince=seconds*50;

	return daysSinceEpoch;
}

void container_common_insertFileEntry(struct container_state *container,struct container_cached_file_entry *entry)
{
	if (!container->lastEntry)
	{
		container->firstEntry=entry;
		container->lastEntry=entry;
	} else {
		container->lastEntry->next=entry;
		entry->prev=container->lastEntry;
		container->lastEntry=entry;
	}
}
