//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: service_timer.h
**
** DESCRIPTION: Async timer routine definitions, prototypes, etc.
**
** This header is platform-independent.  It specifies the TYPES and FUNCTION
** prototypes whose implementations are platform-*dependent*!
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>

//***
// Timer definitions
//***
typedef struct svc_timer_struct
{
	uint64_t f_Ticks;
	uint64_t capturedMS;
	uint8_t  timerRunning;
}ASYNC_TIMER_STRUCT;

#ifdef __cplusplus
extern "C" {
#endif

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                         EXPORTED API PROTOTYPES
/*
   These are definitions platform-independent.  However, their implementations
   are very much platform-dependent.
*/
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

/*
** FUNCTION: TIMER_initMSTimer
**
** DESCRIPTION: Initializes the 1mS timer.
**
** ARGUMENTS:
**
** NOTES:
*/
void TIMER_initMSTimer(void);

/*
** FUNCTION: TIMER_getRunningSeconds
**
** DESCRIPTION: Returns the number of seconds the board has been
**              running.
**
** ARGUMENTS:
**
** NOTES:
*/
uint64_t TIMER_getRunningSeconds(void);

/*
** FUNCTION: TIMER_Start
** DESCRIPTION: Captures the current "Ticks" value that the ISR updates
**              in 1 mS increments.  Used later to determine if a timeout
**              has occurred.
**
*/
unsigned char TIMER_Start(ASYNC_TIMER_STRUCT *pTimer);

/*
** FUNCTION: TIMER_Finished
** DESCRIPTION: Returns a 1 if the timeout period given
**              meets or exceeds the amount of time since
**              the timer was started
**
*/
unsigned char TIMER_Finished(ASYNC_TIMER_STRUCT *pTimer, uint64_t Timeout);

unsigned char TIMER_Running(ASYNC_TIMER_STRUCT* pTimer);
void TIMER_Cancel(ASYNC_TIMER_STRUCT *pTimer);

/*
** FUNCTION: TIMER_GetElapsedMillisecs
**
** DESCRIPTION: Return the difference of milliseconds
**              that have elapsed since "pTImerInfo1" was
**              started.
**
** ARGUMENTS:
**
** NOTES:
*/
uint64_t TIMER_GetElapsedMillisecs (ASYNC_TIMER_STRUCT *pTimerInfo1,
                                    ASYNC_TIMER_STRUCT *pTimerInfo2);

/*
** FUNCTION: SleepMS
** DESCRIPTION: Synchronous sleep for the number of mS called for.
**
*/
void SleepMS(uint64_t Delay);


#ifdef __cplusplus
}
#endif

