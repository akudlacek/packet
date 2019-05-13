/*EXAMPLE MAIN*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <windows.h>
#include "./ring_buffer/ring_buffer.h"

#include "packet.h"


/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#define PACKET_RX_TIMEOUT_MS 500


/**************************************************************************************************
*                                         LOCAL PROTOTYPES
*************************************************^************************************************/
static int16_t a_rx_byte(void);
static void a_tx_data(const uint8_t * const data, uint32_t length);
static int16_t b_rx_byte(void);
static void b_tx_data(const uint8_t * const data, uint32_t length);

static void packet_cmd_handler(packet_inst_t *packet_inst, packet_rx_t packet_rx);


/**************************************************************************************************
*                                            VARIABLES
*************************************************^************************************************/
static volatile uint32_t sys_tick_ms = 0;
static const volatile uint32_t * const sys_tick_ms_ptr = &sys_tick_ms;

static packet_inst_t a_packet_inst;
static packet_inst_t b_packet_inst;

static ring_buffer_t a_buff;
static ring_buffer_t b_buff;

static volatile uint8_t a_arr[1000];
static volatile uint8_t b_arr[1000];

static uint8_t payload_sent[4];


/**************************************************************************************************
*                                            FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief MAIN
*
*  \note
******************************************************************************/
int main(void)
{
	uint32_t last_tick_ms;
	packet_conf_t packet_conf;

	sys_tick_ms = GetTickCount();
	last_tick_ms = *sys_tick_ms_ptr;

	ring_buffer_init(&a_buff, a_arr, sizeof(a_arr));
	ring_buffer_init(&b_buff, b_arr, sizeof(b_arr));

	/*Packet init*/
	packet_get_config_defaults(&packet_conf);
	packet_conf.tick_ms_ptr = sys_tick_ms_ptr;
	packet_conf.cmd_handler_fptr = (void *)packet_cmd_handler;
	//using built in crc-16
	packet_conf.clear_buffer_timeout = PACKET_RX_TIMEOUT_MS;
	packet_conf.enable = PACKET_ENABLED;

	//a specific
	packet_conf.rx_byte_fptr = a_rx_byte;
	packet_conf.tx_data_fprt = a_tx_data;
	packet_conf.cmd_handler_fptr = (void *)packet_cmd_handler;
	packet_init(&a_packet_inst, packet_conf);

	//b specific
	packet_conf.rx_byte_fptr = b_rx_byte;
	packet_conf.tx_data_fprt = b_tx_data;
	packet_conf.cmd_handler_fptr = (void *)packet_cmd_handler;
	packet_init(&b_packet_inst, packet_conf);

	printf("hello\r\n");

	while(1)
	{
		sys_tick_ms = GetTickCount();

		packet_task(&a_packet_inst);
		packet_task(&b_packet_inst);

		if((*sys_tick_ms_ptr - last_tick_ms) >= 1000)
		{
			packet_tx_32(&a_packet_inst, 0x00, *sys_tick_ms_ptr);

			printf("TIME mS: %lu\r\n", *sys_tick_ms_ptr);
			last_tick_ms = *sys_tick_ms_ptr;
		}
		
	}

	return 0;
}


/**************************************************************************************************
*                                         LOCAL FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief A RX byte
*
*  \note returns data or -1 if no data
******************************************************************************/
static int16_t a_rx_byte(void)
{
	int16_t rx_character;

	rx_character = ring_buffer_get_data(&b_buff);

	//returns received data or -1 for no data
	return rx_character;
}

/******************************************************************************
*  \brief A TX data array
*
*  \note
******************************************************************************/
static void a_tx_data(const uint8_t * const data, uint32_t length)
{
	uint32_t i = 0;

	for(i = 0; i < length; i++)
	{
		ring_buffer_put_data(&a_buff, data[i]);
	}
}

/******************************************************************************
*  \brief B RX byte
*
*  \note returns data or -1 if no data
******************************************************************************/
static int16_t b_rx_byte(void)
{
	int16_t rx_character;

	rx_character = ring_buffer_get_data(&a_buff);

	//returns received data or -1 for no data
	return rx_character;
}

/******************************************************************************
*  \brief B TX data array
*
*  \note
******************************************************************************/
static void b_tx_data(const uint8_t * const data, uint32_t length)
{
	uint32_t i = 0;

	for(i = 0; i < length; i++)
	{
		ring_buffer_put_data(&b_buff, data[i]);
	}
}

/******************************************************************************
*  \brief Packet command handler
*
*  \note
******************************************************************************/
static void packet_cmd_handler(packet_inst_t *packet_inst, packet_rx_t packet_rx)
{
	float *float_data_ptr;
	uint32_t uint32_data;

	/*Have to copy data to local variable instead of casting because it was causing an exception to occur*/

	/*Covert array to uint32_t - Little endian*/
	uint32_data = ((uint32_t)packet_rx.payload[3] << 24);
	uint32_data |= ((uint32_t)packet_rx.payload[2] << 16);
	uint32_data |= ((uint32_t)packet_rx.payload[1] << 8);
	uint32_data |= ((uint32_t)packet_rx.payload[0]);

	/*Point float data to uint32_t data - have to do this because casting as float changes value*/
	float_data_ptr = (float *)&uint32_data;

	switch(packet_rx.id)
	{
	case 0x00:
		printf("RX DATA: %lu\r\n", uint32_data);
		break;

	default:
		packet_tx_8(packet_inst, 0xFF, 0xFF); //Send unsupported command error
		break;
	}
}
