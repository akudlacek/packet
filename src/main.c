/*EXAMPLE MAIN*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>
#include <math.h>

#include "./ring_buffer/ring_buffer.h"

#include "packet.h"


/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#define PACKET_RX_TIMEOUT_MS 10


typedef union bit8_dat_t
{
	uint8_t _uint;
	int8_t _int;
} bit8_dat_t;

typedef union bit16_dat_t
{
	uint16_t _uint;
	int16_t _int;
} bit16_dat_t;

typedef union bit32_dat_t
{
	uint32_t _uint;
	int32_t _int;
	float _flt;
} bit32_dat_t;

typedef union bit64_dat_t
{
	uint64_t _uint;
	int64_t _int;
	double _dbl;
} bit64_dat_t;


/**************************************************************************************************
*                                         LOCAL PROTOTYPES
*************************************************^************************************************/
static int16_t  a_rx_byte(void);
static void     a_tx_data(const uint8_t * const data, uint32_t length);
static int16_t  b_rx_byte(void);
static void     b_tx_data(const uint8_t * const data, uint32_t length);

static void     packet_cmd_handler(pckt_inst_t *pckt_inst, pckt_rx_t pckt_rx);
static uint64_t rand_range        (uint64_t min, uint64_t max);
static uint64_t rand_uint64_slow  (void);
static uint32_t rand_uint32_slow  (void);
static uint16_t rand_uint16_slow  (void);
static void     gen_rand_vals     (void);


/**************************************************************************************************
*                                            VARIABLES
*************************************************^************************************************/
static volatile uint32_t sys_tick_ms = 0;
static const volatile uint32_t * const sys_tick_ms_ptr = &sys_tick_ms;

static pckt_inst_t a_pckt_inst;
static pckt_inst_t b_pckt_inst;

static ring_buffer_t a_buff;
static ring_buffer_t b_buff;

static volatile uint8_t a_arr[1000];
static volatile uint8_t b_arr[1000];

static unsigned char str_val[] = "test";
static  uint8_t      uint8_val;
static   int8_t      int8_val;
static uint16_t      uint16_val;
static  int16_t      int16_val;
static uint32_t      uint32_val;
static  int32_t      int32_val;
static uint64_t      uint64_val;
static  int64_t      int64_val;
static    float      float_val;
static   double      double_val;


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
	uint32_t cmd_time_val_ms = PACKET_RX_TIMEOUT_MS;
	pckt_conf_t pckt_conf;
	uint32_t id = 0;

	uint32_t i;
	uint8_t tmp_data_arr[RX_BUFFER_LEN_BYTES] = {0};

	/* initialize random seed: */
	srand((unsigned int)time(NULL));

	gen_rand_vals();

	sys_tick_ms  = GetTickCount();
	last_tick_ms = *sys_tick_ms_ptr;

	ring_buffer_init(&a_buff, a_arr, sizeof(a_arr));
	ring_buffer_init(&b_buff, b_arr, sizeof(b_arr));

	/*Packet init*/
	pckt_get_config_defaults(&pckt_conf);
	pckt_conf.tick_ptr = sys_tick_ms_ptr;
	//using built in crc-16
	pckt_conf.clear_buffer_timeout = PACKET_RX_TIMEOUT_MS;
	pckt_conf.enable = PCKT_ENABLED;

	//a specific
	pckt_conf.rx_byte_fptr = a_rx_byte;
	pckt_conf.tx_data_fprt = a_tx_data;
	pckt_init(&a_pckt_inst, pckt_conf);

	//b specific
	pckt_conf.rx_byte_fptr = b_rx_byte;
	pckt_conf.tx_data_fprt = b_tx_data;
	pckt_init(&b_pckt_inst, pckt_conf);

	printf("hello\r\n");

	while(1)
	{
		sys_tick_ms = GetTickCount();

		pckt_task(&a_pckt_inst, packet_cmd_handler);
		pckt_task(&b_pckt_inst, packet_cmd_handler);

		/*test tx, rx, encode, and decode*/
		if((sys_tick_ms - last_tick_ms) >= cmd_time_val_ms)
		{
			switch(id)
			{
				/*Testing basic functions - BEGIN*/
				case 0:
					pckt_tx_raw(&a_pckt_inst, id, str_val, sizeof(str_val));
					break;
				case 1:
					pckt_tx_u8(&a_pckt_inst, id, uint8_val);
					break;
				case 2:
					pckt_tx_s8(&a_pckt_inst, id, int8_val);
					break;
				case 3:
					pckt_tx_u16(&a_pckt_inst, id, uint16_val);
					break;
				case 4:
					pckt_tx_s16(&a_pckt_inst, id, int16_val);
					break;
				case 5:
					pckt_tx_u32(&a_pckt_inst, id, uint32_val);
					break;
				case 6:
					pckt_tx_s32(&a_pckt_inst, id, int32_val);
					break;
				case 7:
					pckt_tx_u64(&a_pckt_inst, id, uint64_val);
					break;
				case 8:
					pckt_tx_s64(&a_pckt_inst, id, int64_val);
					break;
				case 9:
					pckt_tx_flt32(&a_pckt_inst, id, float_val);
					break;
				case 10:
					pckt_tx_dbl64(&a_pckt_inst, id, double_val);
					break;
				/*Testing basic functions - END*/

				//send packet with checksum error
				case 11:
					//13 is the number of bytes in this packet
					//Sends normal packet
					pckt_tx_dbl64(&a_pckt_inst, id, double_val);

					//Retrieve that packet
					for(i = 0; i < 13; i++)
					{
						tmp_data_arr[i] = (uint8_t)b_rx_byte();
					}

					//corrupt a byte
					tmp_data_arr[12] ^= 0xFF;

					//put bad packet back
					a_tx_data(tmp_data_arr, 13);
					break;

				//send partial packet and ensure it times out
				case 12:
					//send incomplete packet
					tmp_data_arr[0] = 'a';
					a_tx_data(tmp_data_arr, 1);

					//delay for timeout to occur.
					while((sys_tick_ms - last_tick_ms) <= PACKET_RX_TIMEOUT_MS * 2)
					{
						sys_tick_ms = GetTickCount();
					}
					break;

					//Sends normal packet
				case 13:

					pckt_tx_dbl64(&a_pckt_inst, id, double_val);
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
					pckt_tx_u16(&a_pckt_inst, 0xDEAD, 0xBEEF);
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
				//break; //leave timed testing loop

				//restart testing
                id = 0;

                gen_rand_vals();
			}


			last_tick_ms = sys_tick_ms;
		}

	}

	printf("TESTING FINISHED\r\n");

	while(1)
	{
		pckt_task(&a_pckt_inst, packet_cmd_handler);
		pckt_task(&b_pckt_inst, packet_cmd_handler);
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
static void packet_cmd_handler(pckt_inst_t *pckt_inst, pckt_rx_t pckt_rx)
{
	uint8_t successful = 0;
	static uint16_t last_id = 0;

	uint8_t   u8_rx_val;
	int8_t    s8_rx_val;
	uint16_t  u16_rx_val;
	int16_t   s16_rx_val;
	uint32_t  u32_rx_val;
	int32_t   s32_rx_val;
	uint64_t  u64_rx_val;
	int64_t   s64_rx_val;
	float     flt32_rx_val;
	double    dbl64_rx_val;


	//B received
	if(pckt_inst == &b_pckt_inst)
	{
		//echo back to a
		pckt_tx_raw(&b_pckt_inst, pckt_rx.id, pckt_rx.payload, pckt_rx.len);
	}

	//A received
	else if(pckt_inst == &a_pckt_inst)
	{
		switch(pckt_rx.id)
		{
			case 0:
				if(memcmp(pckt_rx.payload, str_val, sizeof(str_val)) == 0) successful = 1;
				break;

			case 1:
				if(pckt_rx_u8(&a_pckt_inst, &u8_rx_val) == PCKT_INVALID_LEN) break;
				if(uint8_val == u8_rx_val) successful = 1;
				break;

			case 2:
				if(pckt_rx_s8(&a_pckt_inst, &s8_rx_val) == PCKT_INVALID_LEN) break;
				if(int8_val == s8_rx_val) successful = 1;
				break;

			case 3:
				if(pckt_rx_u16(&a_pckt_inst, &u16_rx_val) == PCKT_INVALID_LEN) break;
				if(uint16_val == u16_rx_val) successful = 1;
				break;

			case 4:
				if(pckt_rx_s16(&a_pckt_inst, &s16_rx_val) == PCKT_INVALID_LEN) break;
				if(int16_val == s16_rx_val) successful = 1;
				break;

			case 5:
				if(pckt_rx_u32(&a_pckt_inst, &u32_rx_val) == PCKT_INVALID_LEN) break;
				if(uint32_val == u32_rx_val) successful = 1;
				break;

			case 6:
				if(pckt_rx_s32(&a_pckt_inst, &s32_rx_val) == PCKT_INVALID_LEN) break;
				if(int32_val == s32_rx_val) successful = 1;
				break;

			case 7:
				if(pckt_rx_u64(&a_pckt_inst, &u64_rx_val) == PCKT_INVALID_LEN) break;
				if(uint64_val == u64_rx_val) successful = 1;
				break;

			case 8:
				if(pckt_rx_s64(&a_pckt_inst, &s64_rx_val) == PCKT_INVALID_LEN) break;
				if(int64_val == s64_rx_val) successful = 1;
				break;

			case 9:
				if(pckt_rx_flt32(&a_pckt_inst, &flt32_rx_val) == PCKT_INVALID_LEN) break;
				if(float_val == flt32_rx_val) successful = 1;
				break;

			case 10:
				if(pckt_rx_dbl64(&a_pckt_inst, &dbl64_rx_val) == PCKT_INVALID_LEN) break;
				if(double_val == dbl64_rx_val) successful = 1;
				break;

			case 11:
				//id 11 should NOT get here
				printf("CHECKSUM FAIL");
				while(1);
				break;

			case 12:
				//id 12 should NOT get here
				printf("TIMEOUT FAIL");
				while(1);
				break;

			case 13:
				//purposly recieving the wrong type
				if(pckt_rx_u16(&a_pckt_inst, &u16_rx_val) == PCKT_INVALID_LEN)
				{
					successful = 1;
					break;
				}

				//id 13 should NOT get here
				printf("RX LEN MISMATCH FAIL");
				while(1);
				break;

			case 0xDEAD:
				if(pckt_rx_u16(&a_pckt_inst, &u16_rx_val) == PCKT_INVALID_LEN) break;
				if(0xBEEF == u16_rx_val && pckt_rx.crc_16_checksum == 0x7419) successful = 1;
				break;

			case PCKT_ERR_ID_CHKSM:
				successful = 2;
				if(last_id == 10)
				{
					//id 11 should return an error
					printf("TEST: 0xB SUCCESS\r\n");
				}
				else
				{
					printf("PCKT_ID_ERR_CHECKSUM UNKNOWN SOURCE\r\n");
					while(1);
				}
				break;

			case PCKT_ERR_ID_TO:
				successful = 2;
				if((last_id == 11) || (last_id == PCKT_ERR_ID_CHKSM))
				{
					//if last id was 11 then 11 failed test, if PCKT_ID_ERR_CHECKSUM was last then id 11 passed, FYI
					//id 12 should return an error
					printf("TEST: 0xC SUCCESS\r\n");
				}
				else
				{
					printf("PCKT_ID_ERR_TIMEOUT UNKNOWN SOURCE\r\n");
					while(1);
				}
				break;

			default:
				printf("BAD ID ERROR ON A\r\n");
				while(1);
				break;
		}

		//if successful not 0 or 1 then message is handled in the case above
		if(successful < 2)
		{
			printf("TEST: 0x%X %s\r\n", pckt_rx.id, (successful == 1 ? "SUCCESS" : "FAIL"));

			//stop on FAIL
			if(successful == 0)
            {
                while(1);
            }
		}

		last_id = pckt_rx.id; //record last id to verify proper test response
	}
}

/******************************************************************************
* Generates random number within a range
******************************************************************************/
static uint64_t rand_range(uint64_t min, uint64_t max)
{
	return rand_uint64_slow() % (max - min + 1) + min;
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

/******************************************************************************
* 32 bit rand
******************************************************************************/
static uint32_t rand_uint32_slow(void)
{
	uint32_t r = 0;
	for(int i = 0; i < 32; i++)
	{
		r = r * 2 + rand() % 2;
	}
	return r;
}

/******************************************************************************
* 16 bit rand
******************************************************************************/
static uint16_t rand_uint16_slow(void)
{
	uint16_t r = 0;
	for(int i = 0; i < 16; i++)
	{
		r = r * 2 + rand() % 2;
	}
	return r;
}

/******************************************************************************
* generate random values
******************************************************************************/
static void gen_rand_vals(void)
{
    bit8_dat_t  tmp_8bit = {0};
    bit16_dat_t tmp_16bit = {0};
    bit32_dat_t tmp_32bit = {0};
    bit64_dat_t tmp_64bit = {0};

     tmp_8bit._uint = rand();
    tmp_16bit._uint = rand_uint16_slow();
    tmp_32bit._uint = rand_uint32_slow();
    tmp_64bit._uint = rand_uint64_slow();

     uint8_val =  tmp_8bit._uint;
	  int8_val =  tmp_8bit._int;
	uint16_val = tmp_16bit._uint;
	 int16_val = tmp_16bit._int;
	uint32_val = tmp_32bit._uint;
	 int32_val = tmp_32bit._int;
	uint64_val = tmp_64bit._uint;
	 int64_val = tmp_64bit._int;

	 //generate new value if not valid e.g. NAN, INF
	while(!isnormal(tmp_32bit._flt))
    {
        tmp_32bit._uint = rand_uint32_slow();
    }
    float_val = tmp_32bit._flt;

    while(!isnormal(tmp_64bit._dbl))
    {
        tmp_64bit._uint = rand_uint64_slow();
    }
    double_val = tmp_64bit._dbl;
}
