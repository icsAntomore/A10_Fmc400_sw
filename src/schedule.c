/*
 * schedule.c
 *
 *  Created on: Jun 29, 2024
 *      Author: anton
 */

#include "schedule.h"

static sched_slot sched_array[CORE_QTY][SCHED_MAX];

extern volatile unsigned long core0_ticks;
extern volatile unsigned long core1_ticks;


unsigned long get_Time(int core) {
	if (core == 0)
		return core0_ticks;
	else
		return core1_ticks;
}


int sched_insert(int core, int code, void (*func)(void), unsigned int x_timer)
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  if (sched_array[core][i].code == SCHED_NOTUSED)
		 break;
   if (i == SCHED_MAX)
   {
//        print_allarm("SCHEDULER FULL", NOALARM);
	  //ERROR(0);
	  return(0);
   }
   else
   if (x_timer > 0x80000000L)
   {
//        print_allarm("SCHEDULER TIMERR", NOALARM);
	  //ERROR(0);
	  return(0);
   }
   else
   {
	  sched_array[core][i].code = code;
	  sched_array[core][i].func = func;
	  sched_array[core][i].time = get_Time(core) + x_timer;
	  sched_array[core][i].period = x_timer;
	  //printf("\nscheduled = code: %d, x_timer: %lu", code, ticks);
	  return(1);
   }
}

int sched_del_all_func(int core)
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  sched_array[core][i].code = SCHED_NOTUSED;

	return(1);
}

int sched_find_func(int core, void (*func)(void))
{
   int i;
   for (i = 0; i < SCHED_MAX; i++)
	  if (sched_array[core][i].code != SCHED_NOTUSED)
		 if (func == sched_array[core][i].func)
			return(i);
   return(-1);
}

int sched_del_by_func(int core, void (*func)(void))
{
   int i;
   if ((i = sched_find_func(core, func)) >= 0)
	  return(sched_delete(core, i));
   else
	  return(0);
}

void sched_rep_by_func(int core, int code, void (*func)(void), unsigned int x_timer)
{
   sched_del_by_func(core, func);
   sched_insert(core, code, func, (long) x_timer);
}

void sched_rep_time_by_func(int core, void (*func)(void), unsigned int x_timer)
{
   int i;
   if ((i = sched_find_func(core, func)) >= 0)
	  sched_array[core][i].time = get_Time(core)+ x_timer;

}

int sched_delete(int core, int slot)
{
   if (sched_array[core][slot].code == SCHED_NOTUSED)
   {
//	  ERROR(0);
//       print_allarm("DEL NOTUSED SCHED", NOALARM);
	  return(0);
   }
   else
   {
	  sched_array[core][slot].code = SCHED_NOTUSED;
	  return(1);
   }
}

void sched_manager(int core)
{
   int i, f;

   //printf("\nx_timer: %x",get_Time());
   for (i = 0, f = 0; i < SCHED_MAX; i++)
	  switch (sched_array[core][i].code)
	  {
		 case SCHED_CONTINUE:
			(sched_array[core][i].func)();
			f++;
			break;
		 case SCHED_PERIODIC:
			if (get_Time(core)>= sched_array[core][i].time)
			{
			   (sched_array[core][i].func)();
			   sched_array[core][i].time = get_Time(core)+ sched_array[core][i].period;
			}
			f++;
			break;
		 case SCHED_ONETIME:
			if (get_Time(core)>= sched_array[core][i].time)
			{
			   (sched_array[core][i].func)();
			   sched_delete(core, i);
			}
			f++;
			break;
		 case SCHED_NOTUSED:
			break;
		 default:
 /*                     print_allarm("SCHEDULER CASE", NOALARM);        */
		//	ERROR(0);
			sched_delete(core, i);
			break;
	  }
//   if (!f)
//	  ERROR(0);
/*        print_allarm("SCHEDULER EMPTY", NOALARM);     */
}


