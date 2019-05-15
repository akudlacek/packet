/*
 * ring_buffer.h
 *
 *  Created on: Aug 2, 2016
 *      Author: akudlacek
 */


#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_


#include <stdint.h>


/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
typedef enum
{
	BUFFER_EMPTY,
	BUFFER_HAS_DATA
} ring_buffer_state_t;

typedef struct
{
	ring_buffer_state_t rb_state;
	volatile uint8_t *rb_buffer_array;
	uint16_t rb_buffer_size;
	uint16_t rb_head;
	uint16_t rb_tail;
	uint16_t rb_max_usage;
} volatile ring_buffer_t;


/**************************************************************************************************
*                                            PROTOTYPES
*************************************************^************************************************/
void ring_buffer_init(ring_buffer_t * const ring_buffer, volatile uint8_t * const buffer_array, uint16_t buffer_array_size);
int8_t ring_buffer_put_data(ring_buffer_t *ring_buffer, uint8_t data_to_put);
int16_t ring_buffer_get_data(ring_buffer_t *ring_buffer);
uint16_t ring_buffer_get_max_usage(ring_buffer_t *ring_buffer);


#endif /* RING_BUFFER_H_ */
