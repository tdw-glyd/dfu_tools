//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: service_timer.c
**
** DESCRIPTION: Timer support module for RSA.
**
**  REVISION HISTORY:
**
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stddef.h>
#include <sys/time.h>
#include <unistd.h>
#include "async_timer.h"

#if defined(_WIN32) || defined(_WIN64)
    #ifdef _WIN32_WINNT
        #undef _WIN32_WINNT
    #endif
    #define _WIN32_WINNT 0x0600 // Windows Vista or later
    #include <windows.h>
    #include <sysinfoapi.h>
    #include <stdio.h>
#endif

static uint64_t mTotalSeconds;
static uint32_t mTimerModuleInited = 0;
volatile static uint64_t mTimerTicks = 0;


//static uint64_t GetTickCount(void);

#define MS_TICKS_PER_SEC         (1000)


// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                    EXPORTED API FUNCTION IMPLEMENTATIONS
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

/**
** FUNCTION: TIMER_initMSTimer
**
** DESCRIPTION: Initializes the 1mS timer.
**
** ARGUMENTS:
**
** NOTES:
*/
void TIMER_initMSTimer(void)
{
    if (!mTimerModuleInited)
    {
        mTimerModuleInited = 1;
    }
}

/*!
** FUNCTION: TIMER_getRunningSeconds
**
** DESCRIPTION: Returns the number of seconds the board has been
**              running.
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
*/
uint64_t TIMER_getRunningSeconds(void)
{
    uint64_t             count;

    TIMER_initMSTimer();
    count = mTotalSeconds;


    return (count);
}

/*!
** FUNCTION: TIMER_Start
**
** DESCRIPTION: Captures the current "Ticks" value that the ISR updates
**              in 1 mS increments.  Used later to determine if a timeout
**              has occurred.
**
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
unsigned char TIMER_Start(ASYNC_TIMER_STRUCT *pTimer)
{
    TIMER_initMSTimer();
    if ( (mTimerModuleInited) && (pTimer) )
    {
        pTimer->capturedMS = GetTickCount64();
        pTimer->timerRunning = 1;
        return (1);
    }

    return (0);
}

/*!
** FUNCTION: TIMER_Finished
**
** DESCRIPTION: Returns a 1 if the timeout period given
**              meets or exceeds the amount of time since
**              the timer was started.
** ARGUMENTS:
**
** NOTES:
*/
#define TIMER_USE_RATIONAL_TIMEOUT    (1)
#if (TIMER_USE_RATIONAL_TIMEOUT==1)
unsigned char TIMER_Finished(ASYNC_TIMER_STRUCT *pTimer, uint64_t Timeout)
{
    uint8_t                 ret = 0;

    if (pTimer)
    {
        uint64_t        lTime = GetTickCount64();
        uint32_t        lDiff = lTime - pTimer->capturedMS;

        ret = (uint8_t) ( lDiff >= Timeout );
    }

    return (ret);
}
#else
unsigned char TIMER_Finished(ASYNC_TIMER_STRUCT *pTimer, uint32_t Timeout)
{
    unsigned long          Elapsed;
    unsigned long          delta;

    TIMER_initMSTimer();
    if ( (mTimerModuleInited) && (pTimer) && (Timeout) )
    {
        Elapsed = GetTickCount();

        /*
        ** Roll-over.
        **
        */
        delta = pTimer->capturedMS;
        delta += Timeout;
        if (Elapsed < pTimer->capturedMS)
        {
            delta = ( 4294967295UL - pTimer->capturedMS ) + Elapsed;
            Elapsed = delta;

            delta = pTimer->capturedMS;
            delta += Timeout;
        }

        /*
        ** Time-out?
        **
        */
        if ( Elapsed >= delta )
        {
            return (1);
        }

        return (0);
    }

    return (1);
}
#endif

/*!
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
                                    ASYNC_TIMER_STRUCT *pTimerInfo2)
{
    ASYNC_TIMER_STRUCT         nowTime;
    ASYNC_TIMER_STRUCT        *timer2 = NULL;

    TIMER_initMSTimer();
    if ( (pTimerInfo1) && (pTimerInfo1->capturedMS > 0) )
    {
        /*
        ** If the caller didn't provide a second timer,
        ** capture the current ticks ourselves and use
        ** that to determine elapsed.
        **
        */
        if (pTimerInfo2 == NULL)
        {
            TIMER_Start(&nowTime);
            timer2 = &nowTime;
        }
        else
        {
            timer2 = pTimerInfo2;
        }

        if (timer2->capturedMS >= pTimerInfo1->capturedMS)
            return (timer2->capturedMS - pTimerInfo1->capturedMS);
        else
            return (pTimerInfo1->capturedMS - timer2->capturedMS);

    }

    return (0);
}

/*!
** FUNCTION: SleepMS
**
** DESCRIPTION: Synchronous blocking sleep for the number of mS called for.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
void SleepMS(uint64_t Delay)
{
    ASYNC_TIMER_STRUCT timer;

    TIMER_initMSTimer();
    if (mTimerModuleInited)
    {
        TIMER_Start(&timer);
        while (!TIMER_Finished(&timer, Delay)) {}
    }

    return;
}

// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                 INTERNAL SUPPORT FUNCTION IMPLEMENTATIONS
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

