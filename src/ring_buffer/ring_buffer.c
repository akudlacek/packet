/*
 * ring_buffer.c
 *
 *  Created on: Aug 2, 2016
 *      Author: akudlacek
 */


#include "ring_buffer.h"


/******************************************************************************
 * THIS RING BUFFER THROWS OUT OLD DATA IF OVERFLOWED
 * AND RETURNS -1 WHEN THIS HAPPENS
 *
 * HOW TO USE
 *
 * 1.DECLARE BUFFER MEMORY
 * #define UART_RING_BUFFER_SIZE_BYTE 8
 * uint8_t uart_tx_array[UART_RING_BUFFER_SIZE_BYTE] = {0};
 * uint8_t uart_rx_array[UART_RING_BUFFER_SIZE_BYTE] = {0};
 *
 * 2.DECLARE RING BUFFER STRUCTS
 * ring_buffer_t uart_tx_buffer;
 * ring_buffer_t uart_rx_buffer;
 *
 * 3.INITIALIZE RING BUFFER STRUCTS
 * ring_buffer_init(&uart_tx_buffer, uart_tx_array, UART_RING_BUFFER_SIZE_BYTE);
 * ring_buffer_init(&uart_rx_buffer, uart_rx_array, UART_RING_BUFFER_SIZE_BYTE);
 *
 * 4.USE PUT AND GET FUNCTIONS AS NEEDED
 * ring_buffer_put_data(&uart_rx_buffer, data);
 * data = ring_buffer_get_data(&uart_rx_buffer);
 *
 *****************************************************************************/


/**************************************************************************************************
*                                         LOCAL PROTOTYPES
*************************************************^************************************************/
static inline void set_peak_usage(ring_buffer_t *ring_buffer, uint16_t usage);


/**************************************************************************************************
*                                            FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief Initialize ring buffer with user defined memory
*
*  \note Must initialize the struct before using ring buffer put and get
******************************************************************************/
void ring_buffer_init(ring_buffer_t * const ring_buffer, volatile uint8_t * const buffer_array, uint16_t buffer_array_size)
{
	ring_buffer->rb_state = BUFFER_EMPTY;
	ring_buffer->rb_buffer_array = buffer_array;
	ring_buffer->rb_buffer_size = buffer_array_size;
	ring_buffer->rb_head = 0;
	ring_buffer->rb_tail = 0;
	ring_buffer->rb_max_usage = 0;
}

/******************************************************************************
*  \brief Put data into buffer
*
*  \note Takes in ring_buffer_struct and data you want to put into buffer
*        returns 0 for no error and -1 for buffer overflow where oldest data
*        was dumped
******************************************************************************/
int8_t ring_buffer_put_data(ring_buffer_t *ring_buffer, uint8_t data_to_put)
{
     /*0 for no error, -1 for overflow*/
	int8_t overflow_flag = 0;

	switch(ring_buffer->rb_state)
	{
	case BUFFER_EMPTY:
		ring_buffer->rb_buffer_array[ring_buffer->rb_head] = data_to_put; //put data to buffer
		ring_buffer->rb_head++;
		ring_buffer->rb_state = BUFFER_HAS_DATA;                          //change state
		break;

	case BUFFER_HAS_DATA:
		/*if buffer is not full*/
		if(ring_buffer->rb_head != ring_buffer->rb_tail)
		{
			ring_buffer->rb_buffer_array[ring_buffer->rb_head] = data_to_put; //put data to buffer
			ring_buffer->rb_head++;
		}
		/*if buffer full toss out oldest data*/
		else
		{
			ring_buffer->rb_tail++;                                           //this dumps oldest data
			ring_buffer->rb_buffer_array[ring_buffer->rb_head] = data_to_put; //put data to buffer
			ring_buffer->rb_head++;
			overflow_flag = -1;                                               //set buffer overflow flag
		}
		break;
	}

	/*Wrap tail*/
	if(ring_buffer->rb_tail == ring_buffer->rb_buffer_size)
		ring_buffer->rb_tail = 0;
    /*Wrap head*/
	if(ring_buffer->rb_head == ring_buffer->rb_buffer_size)
		ring_buffer->rb_head = 0;

    /*Check peak usage, note: cannot show above ring buffer size*/
    if(ring_buffer->rb_head > ring_buffer->rb_tail)
    {
        /*head > tail*/
        set_peak_usage(ring_buffer, ring_buffer->rb_head - ring_buffer->rb_tail);
    }
    else
    {
        /*head < tail*/
        set_peak_usage(ring_buffer, (uint16_t)(((uint32_t)ring_buffer->rb_head + (uint32_t)ring_buffer->rb_buffer_size) - ring_buffer->rb_tail));
    }

	return overflow_flag;
}

/******************************************************************************
*  \brief Get data from buffer
*
*  \note Takes in ring_buffer_struct, returns data or -1 for buffer empty
******************************************************************************/
int16_t ring_buffer_get_data(ring_buffer_t *ring_buffer)
{
    /*-1 for empty*/
	int16_t return_data = 0;

	switch(ring_buffer->rb_state)
	{
	case BUFFER_EMPTY:
		return -1;            //if empty nothing to do so return -1 indicating empty buffer

	case BUFFER_HAS_DATA:
		return_data = ring_buffer->rb_buffer_array[ring_buffer->rb_tail]; //get from buffer
		ring_buffer->rb_tail++;
		/*Wrap tail*/
		if(ring_buffer->rb_tail == ring_buffer->rb_buffer_size)
			ring_buffer->rb_tail = 0;

		/*check if buffer is empty*/
		if(ring_buffer->rb_head == ring_buffer->rb_tail)
		{
			ring_buffer->rb_state = BUFFER_EMPTY;       //change state
		}
		break;
	}

	return return_data;
}

/******************************************************************************
*  \brief Get ring buffer max usage
*
*  \note returns the ring buffers current max usage
*        Only able to see up to rb_buffer_size, so cannot see how many over
*        the max was used.
******************************************************************************/
uint16_t ring_buffer_get_max_usage(ring_buffer_t *ring_buffer)
{
    return ring_buffer->rb_max_usage;
}


/**************************************************************************************************
*                                         LOCAL FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief Set ring buffer max usage
*
*  \note Only changes if exceeds previous max
******************************************************************************/
static inline void set_peak_usage(ring_buffer_t *ring_buffer, uint16_t usage)
{
    if(usage > ring_buffer->rb_max_usage)
        ring_buffer->rb_max_usage = usage;
}
