#include <stm32f4xx.h>
#include <ucos_ii.h>
#include "GLCD.h"
#include "I2C.h"
#include "JOY.h"

#define TASK_STK_SIZE 1024
#define N_TASKS 2

OS_EVENT* tamperbut_sem;
OS_EVENT* done_sem;

OS_STK TaskStk[N_TASKS][TASK_STK_SIZE];


#define  TASK_START_PRIO                        4
#define  TASK_START_STK_SIZE                    512
static   OS_STK      TaskStartStk[TASK_START_STK_SIZE];

void game_main(void* pdata);
void pause(void* pdata);
void LoadingStage(void);

static  void  TaskStart(void *p_arg)
{
	INT8U err;
	INT32U  cnts;

	err = OSTaskCreate(game_main, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 10);
	err = OSTaskCreate(pause, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 11);

	tamperbut_sem = OSSemCreate(0);
	done_sem = OSSemCreate(0);

	OSTaskDel(OS_PRIO_SELF);
}

int main(void)
{

	INT8U  err;

	GLCD_Init();
	I2C_Init();
	JOY_Init();

	GLCD_Clear(Black);
	GLCD_SetTextColor(White);
	GLCD_SetBackColor(Black);

	LoadingStage();

	OSInit();                                                   /* Initialize "uC/OS-II, The Real-Time Kernel"              */

	OSTaskCreateExt(TaskStart,                               /* Create the start task                                    */
		(void *)0,
		(OS_STK *)&TaskStartStk[TASK_START_STK_SIZE - 1],
		TASK_START_PRIO,
		TASK_START_PRIO,
		(OS_STK *)&TaskStartStk[0],
		TASK_START_STK_SIZE,
		(void *)0,
		OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);


	OSStart();                                                  /* Start multitasking (i.e. give control to uC/OS-II)       */

	return 0;
}
