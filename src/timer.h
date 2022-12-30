/*
 * timer.h
 *
 * Created: 11/23/2022 11:28:44 PM
 *  Author: Arin
 */

/*
 * HOW TO USE
 * declare two variables in your tick interrupt file like this:
 * 
 * static volatile tmr_inst_t tick_ms = 0;
 * const volatile tmr_inst_t * volatile const g_tick_ms_ptr = &tick_ms;
 * 
 * tick_ms will be incremented in your tick interrupt (tick_ms++;)
 * g_tick_ms_ptr will be used by timer as a read only tick variable
 * 
 * 
 */


#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#ifndef TICK_TYPE
#define TICK_TYPE uint32_t
#endif

/**************************************************************************************************
*                                            VARIABLES
*************************************************^************************************************/
extern const volatile TICK_TYPE * volatile const g_tick_ms_ptr;

/**************************************************************************************************
*                                         LOCAL FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief
*
*  \note
******************************************************************************/
inline void tmrReset(TICK_TYPE * const lastTime_ms)
{
	*lastTime_ms = *g_tick_ms_ptr;
}

/******************************************************************************
*  \brief
*
*  \note
******************************************************************************/
inline uint8_t tmrCheck(const TICK_TYPE * const lastTime_ms, const TICK_TYPE timeLimit_ms)
{
	return (*g_tick_ms_ptr - *lastTime_ms) >= timeLimit_ms;
}

/******************************************************************************
*  \brief
*
*  \note
******************************************************************************/
inline uint8_t tmrCheckReset(TICK_TYPE * const lastTime_ms, const TICK_TYPE timeLimit_ms)
{
	if(tmrCheck(lastTime_ms, timeLimit_ms))
	{
		tmrReset(lastTime_ms);
		return 1;
	}
	
	return 0;
}


#endif /* TIMER_H_ */
