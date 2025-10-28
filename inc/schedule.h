#define SCHED_MAX 15                /* ex 30 */
#define SCHED_NOTUSED 0
#define SCHED_CONTINUE 1
#define SCHED_PERIODIC 2
#define SCHED_ONETIME 3
#define CORE_QTY 2

#define CORE0 0
#define CORE1 1

typedef struct  {
      short code;
	  void (*func)(void);
	  unsigned int time;
	  short period;
	  } sched_slot;

		

int sched_insert(int core, int code, void (*func)(void), unsigned int x_timer);
int sched_del_by_func(int core, void (*func)(void));
void sched_rep_by_func(int core, int code, void (*func)(void), unsigned int x_timer);
void sched_rep_time_by_func(int core, void (*func)(void), unsigned int x_timer);
void sched_manager(int core);
int sched_find_func(int core, void (*func)(void));
int sched_delete(int core, int slot);
int sched_del_all_func(int core);
unsigned long get_Time(int core);
