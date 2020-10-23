/*EXAMPLE MAIN*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>

#include "./ring_buffer/ring_buffer.h"

#include "packet.h"


/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#define PACKET_RX_TIMEOUT_MS 500


/**************************************************************************************************
*                                         LOCAL PROTOTYPES
*************************************************^************************************************/
static int16_t  a_rx_byte(void);
static void     a_tx_data(const uint8_t * const data, uint32_t length);
static int16_t  b_rx_byte(void);
static void     b_tx_data(const uint8_t * const data, uint32_t length);

static void     packet_cmd_handler(packet_inst_t *packet_inst, packet_rx_t packet_rx);
static uint64_t rand_range(uint64_t min, uint64_t max);
static uint64_t rand_uint64_slow(void);


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

static char     str_val[] = "test";
static uint8_t  uint8_val;
static int8_t   int8_val;
static uint16_t uint16_val;
static int16_t  int16_val;
static uint32_t uint32_val;
static int32_t  int32_val;
static uint64_t uint64_val;
static int64_t  int64_val;
static float    float_val;
static double   double_val;


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
	uint32_t cmd_time_val_ms = 100;
	packet_conf_t packet_conf;
	uint32_t id = 0;

	uint32_t i;
	uint8_t tmp_data_arr[RX_BUFFER_LEN_BYTES];

	/* initialize random seed: */
	srand((unsigned int)time(NULL));

	uint8_val    = (uint8_t)rand_range(0, UINT8_MAX);
	int8_val     = (int8_t)rand_range(INT8_MIN, INT8_MAX);
	uint16_val   = (uint16_t)rand_range(0, UINT16_MAX);
	int16_val    = (int16_t)rand_range(INT16_MIN, INT16_MAX);
	uint32_val   = (uint32_t)rand_range(0, UINT32_MAX);
	int32_val    = (int32_t)rand_range(INT32_MIN, INT32_MAX);
	uint64_val   = (uint64_t)rand_uint64_slow();
	int64_val    = (int64_t)rand_uint64_slow();
	float_val    = (float)rand()/(float)rand_range(1, UINT16_MAX);
	double_val   = (double)rand()/(double)rand_range(1, UINT16_MAX);

	sys_tick_ms  = GetTickCount();
	last_tick_ms = *sys_tick_ms_ptr;

	ring_buffer_init(&a_buff, a_arr, sizeof(a_arr));
	ring_buffer_init(&b_buff, b_arr, sizeof(b_arr));

	/*Packet init*/
	packet_get_config_defaults(&packet_conf);
	packet_conf.tick_ptr = sys_tick_ms_ptr;
	//using built in crc-16
	packet_conf.clear_buffer_timeout = PACKET_RX_TIMEOUT_MS;
	packet_conf.enable = PACKET_ENABLED;

	//a specific
	packet_conf.rx_byte_fptr = a_rx_byte;
	packet_conf.tx_data_fprt = a_tx_data;
	packet_init(&a_packet_inst, packet_conf);

	//b specific
	packet_conf.rx_byte_fptr = b_rx_byte;
	packet_conf.tx_data_fprt = b_tx_data;
	packet_init(&b_packet_inst, packet_conf);

	printf("hello\r\n");

	while(1)
	{
		sys_tick_ms = GetTickCount();

		packet_task(&a_packet_inst, packet_cmd_handler);
		packet_task(&b_packet_inst, packet_cmd_handler);

		/*test tx, rx, encode, and decode*/
		if((*sys_tick_ms_ptr - last_tick_ms) >= cmd_time_val_ms)
		{
			switch(id)
			{
				/*Testing basic functions - BEGIN*/
				case 0:
					packet_tx_raw(&a_packet_inst, id, (uint8_t *)str_val, (uint8_t)sizeof(str_val));
					break;
				case 1:
					packet_tx_8(&a_packet_inst, id, uint8_val);
					break;
				case 2:
					packet_tx_8(&a_packet_inst, id, int8_val);
					break;
				case 3:
					packet_tx_16(&a_packet_inst, id, uint16_val);
					break;
				case 4:
					packet_tx_16(&a_packet_inst, id, int16_val);
					break;
				case 5:
					packet_tx_32(&a_packet_inst, id, uint32_val);
					break;
				case 6:
					packet_tx_32(&a_packet_inst, id, int32_val);
					break;
				case 7:
					packet_tx_64(&a_packet_inst, id, uint64_val);
					break;
				case 8:
					packet_tx_64(&a_packet_inst, id, int64_val);
					break;
				case 9:
					packet_tx_float_32(&a_packet_inst, id, float_val);
					break;
				case 10:
					packet_tx_double_64(&a_packet_inst, id, double_val);
					break;
				/*Testing basic functions - END*/

				//send packet with checksum error
				case 11:
					//13 is the number of bytes in this packet
					//Sends normal packet
					packet_tx_double_64(&a_packet_inst, id, double_val);

					//Retrieve that packet
					for(i = 0; i < 13; i++)
					{
						tmp_data_arr[i] = (uint8_t)b_rx_byte();
					}

					//corrupt a byte
					tmp_data_arr[(uint8_t)rand_range(0,12)] ^= 0xFF;

					//put bad packet back
					a_tx_data((uint8_t *)tmp_data_arr, 13);
					break;

				//send partial packet and ensure it times out
				case 12:
					//13 is the number of bytes in this packet
					//Sends normal packet
					packet_tx_double_64(&a_packet_inst, id, double_val);

					//Retrieve that packet
					for(i = 0; i < 13; i++)
					{
						tmp_data_arr[i] = (uint8_t)b_rx_byte();
					}

					//put packet missing one byte back
					a_tx_data((uint8_t *)tmp_data_arr, 12);

					//set cmd send time to greater than timeout val so the timeout can occur before next packet is sent thus causing a checksum error
					cmd_time_val_ms = b_packet_inst.conf.clear_buffer_timeout + 5;
					break;

					//Sends normal packet
				case 13:

					packet_tx_double_64(&a_packet_inst, id, double_val);
					break;

					//send example packet
				case 14:
					/* Example:
					* ID = 0xDEAD
					* LEN = 2
					* PAYLOAD = 0xBEEF
					* CRC = 0x7419
					*
					* Packet sent
					*  0   1   2   3   4   5   6
					* [DE][AD][02][BE][EF][74][19] */
					packet_tx_16(&a_packet_inst, 0xDEAD, 0xBEEF);
					break;

				default:
					break;
			}

			if(id <= 14)
			{
				id++;
			}
			else
			{
				break; //leave timed testing loop
			}


			last_tick_ms = *sys_tick_ms_ptr;
		}

	}

	printf("TESTING FINISHED\r\n");

	while(1)
	{
		packet_task(&a_packet_inst, packet_cmd_handler);
		packet_task(&b_packet_inst, packet_cmd_handler);
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
	uint8_t successful = 0;
	static uint16_t last_id;

	//B received
	if(packet_inst == &b_packet_inst)
	{
		//echo back to a
		packet_tx_raw(&b_packet_inst, packet_rx.id, packet_rx.payload, packet_rx.len);
	}

	//A received
	else if(packet_inst == &a_packet_inst)
	{
		switch(packet_rx.id)
		{
			case 0:
				if(memcmp(packet_rx.payload, str_val, sizeof(str_val)) == 0)
					successful = 1;
				break;
			case 1:
				if(uint8_val == packet_rx.payload[0])
					successful = 1;
				break;
			case 2:
				if(int8_val == (int8_t)packet_rx.payload[0])
					successful = 1;
				break;
			case 3:
				if(uint16_val == packet_payload_uint16(packet_rx))
					successful = 1;
				break;
			case 4:
				if(int16_val == packet_payload_int16(packet_rx))
					successful = 1;
				break;
			case 5:
				if(uint32_val == packet_payload_uint32(packet_rx))
					successful = 1;
				break;
			case 6:
				if(int32_val == packet_payload_int32(packet_rx))
					successful = 1;
				break;
			case 7:
				if(uint64_val == packet_payload_uint64(packet_rx))
					successful = 1;
				break;
			case 8:
				if(int64_val == packet_payload_int64(packet_rx))
					successful = 1;
				break;
			case 9:
				if(float_val == packet_payload_float_32(packet_rx))
					successful = 1;
				break;
			case 10:
				if(double_val == packet_payload_double_64(packet_rx))
					successful = 1;
				break;

			case 11:
				//id 11 should NOT get here
				successful = 0;
				printf("CHECKSUM FAIL ");
				break;

			case 12:
				//id 12 should NOT get here
				successful = 0;
				printf("TIMEOUT FAIL ");
				break;

			case 13:
				if(double_val == packet_payload_double_64(packet_rx))
					successful = 1;
				break;

			case 0xDEAD:
				if(packet_rx.len == 2 && packet_payload_uint16(packet_rx) == 0xBEEF && packet_rx.crc_16_checksum == 0x7419)
					successful = 1;
				break;

			case PCKT_ID_ERR_CHECKSUM:
				successful = 2;
				if(last_id == 10)
				{
					//id 11 should return an error
					printf("TEST: 11 SUCCESS\r\n");
				}
				else
				{
					printf("PCKT_ID_ERR_CHECKSUM UNKNOWN SOURCE\r\n");
				}
				break;

			case PCKT_ID_ERR_TIMEOUT:
				successful = 2;
				if((last_id == 11) || (last_id == PCKT_ID_ERR_CHECKSUM))
				{
					//if last id was 11 then 11 failed test, if PCKT_ID_ERR_CHECKSUM was last then id 11 passed, FYI
					//id 12 should return an error
					printf("TEST: 12 SUCCESS\r\n");
				}
				else
				{
					printf("PCKT_ID_ERR_TIMEOUT UNKNOWN SOURCE\r\n");
				}
				break;

			default:
				printf("BAD ID ERROR ON A\r\n");
				break;
		}

		//if successful not 0 or 1 then message is handled in the case above
		if(successful < 2)
		{
			printf("TEST: %u %s\r\n", packet_rx.id, (successful == 1 ? "SUCCESS" : "FAIL"));
		}

		last_id = packet_rx.id; //record last id to verify proper test response
	}
}

/******************************************************************************
* Generates random number within a range
******************************************************************************/
static uint64_t rand_range(uint64_t min, uint64_t max)
{
	return rand() % (max - min + 1) + min;
}

/******************************************************************************
* 64 bit rand
******************************************************************************/
static uint64_t rand_uint64_slow(void)
{
	uint64_t r = 0;
	for(int i = 0; i < 64; i++)
	{
		r = r * 2 + rand() % 2;
	}
	return r;
}
