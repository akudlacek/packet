/*
* packet.c
*
* Created: 5/4/2018 3:52:08 PM
*  Author: arin
*/

/*
* Packet format:
* Big endian meaning most significant byte sent first e.g 0x40CC is sent as 0x40 0xCC
*
* 0x12345678
*   ^^    ^^
* MSByte LSByte
*  1st    Last
*
* ID is 2 bytes
* LEN is 1 byte
* PAYLOAD is LEN bytes
*
* NO DATA LEN=0: [ID:1, ID:0][LEN][CRC16:1, CRC16:0]
*    DATA LEN>0: [ID:1, ID:0][LEN][PAYLOAD:n, ...,  PAYLOAD:0][CRC16:1, CRC16:0]
*
* Example:
* ID = 0xDEAD
* LEN = 2
* PAYLOAD = 0xBEEF
* CRC = 0x7419
*
* Packet sent
*  0   1   2   3   4   5   6
* [DE][AD][02][BE][EF][74][19]
*/


#include "packet.h"

#include <string.h>

/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#define ID_1_POS   0
#define ID_0_POS   1
#define LEN_POS    2
#define DATA_N_POS 3
#define DATA_0_POS(payload_data_len) (payload_data_len + LEN_POS)
#define CRC1_POS(payload_data_len)   (DATA_0_POS(payload_data_len) + 1)
#define CRC0_POS(payload_data_len)   (CRC1_POS(payload_data_len) + 1)

#define UNSERIALIZE_UINT16(msbyt, lsbyt) ( (((uint16_t)msbyt) << 8) | (uint16_t)lsbyt )

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
static int16_t     default_rx_byte(void);
static void        default_tx_data(const uint8_t * const data, const uint32_t length);
static void        error_handler  (packet_inst_t * const packet_inst, const packet_id_err_t error);
static void *      revmemcpy      (void* dest, const void* src, size_t len);
static bit16_dat_t unsr_16        (const uint8_t * const big_endian_data);
static bit32_dat_t unsr_32        (const uint8_t * const big_endian_data);
static bit64_dat_t unsr_64        (const uint8_t * const big_endian_data);
static void        sr_16          (uint8_t * const dest, const uint16_t src);
static void        sr_32          (uint8_t * const dest, const uint32_t src);
static void        sr_64          (uint8_t * const dest, const uint64_t src);


/**************************************************************************************************
*                                            FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief Packet get config defaults
*
*  \note
******************************************************************************/
void packet_get_config_defaults(packet_conf_t * const packet_conf)
{
	packet_conf->tick_ptr              = NULL;
	packet_conf->rx_byte_fptr          = default_rx_byte;
	packet_conf->tx_data_fprt          = default_tx_data;
	packet_conf->crc_16_fptr           = sw_crc;
	packet_conf->clear_buffer_timeout  = 0xFFFFFFFF;
	packet_conf->enable                = PACKET_ENABLED;
}

/******************************************************************************
*  \brief Packet init
*
*  \note
******************************************************************************/
void packet_init(packet_inst_t * const packet_inst, const packet_conf_t packet_conf)
{
	/*Conf*/
	packet_inst->conf = packet_conf;

	/*Inst*/
	packet_inst->rx_byte              = 0;
	memset(packet_inst->rx_buffer, 0, sizeof(packet_inst->rx_buffer));
	packet_inst->rx_buffer_ind        = 0;
	packet_inst->calc_crc_16_checksum = 0;
	memset(&packet_inst->packet_rx, 0, sizeof(packet_inst->packet_rx));
	packet_inst->last_tick            = *packet_inst->conf.tick_ptr;
}

/******************************************************************************
*  \brief Packet task
*
*  \note takes pointer to instance and function pointer to command handler
******************************************************************************/
void packet_task(packet_inst_t * const packet_inst, void(*cmd_handler_fptr)(packet_inst_t * const, const packet_rx_t))
{
	/*If packet is disabled do not run*/
	if(packet_inst->conf.enable == PACKET_DISABLED) return;

	/*Get byte*/
	packet_inst->rx_byte = packet_inst->conf.rx_byte_fptr();

	/*Check for received byte*/
	if(packet_inst->rx_byte != -1)
	{
		/*Record time of last byte*/
		packet_inst->last_tick = *packet_inst->conf.tick_ptr;

		/*Is received buffer full?*/
		if(packet_inst->rx_buffer_ind == RX_BUFFER_LEN_BYTES)
		{
			/*Set to the last byte*/
			packet_inst->rx_buffer_ind -= 1;

			/*Making it here means the received buffer is full*/
		}
		else
        {
            /*Put received byte in buffer*/
            packet_inst->rx_buffer[packet_inst->rx_buffer_ind] = (uint8_t)packet_inst->rx_byte;
            packet_inst->rx_buffer_ind++;
        }

		/*Check for valid packet - after ID:0 ID:1 and LEN bytes received*/
		if(packet_inst->rx_buffer_ind >= 3)
		{
			/*Copy LEN*/
			packet_inst->packet_rx.len = packet_inst->rx_buffer[LEN_POS];

			/*Verify LEN*/
			if(packet_inst->packet_rx.len > MAX_PAYLOAD_LEN_BYTES)
			{
				/*If not going to fit force it down to the max*/
				packet_inst->packet_rx.len = MAX_PAYLOAD_LEN_BYTES;
			}

			/*Check data received to see if all bytes received - the +5 is [ID:0, ID:1][LEN][CRC16:0, CRC16:1]*/
			if((packet_inst->packet_rx.len + 5) == packet_inst->rx_buffer_ind)
			{
				/*Calculate checksum - performed on [ID:0, ID:1][LEN][PAYLOAD:0 ...,  PAYLOAD:n] if LEN=0 then just [ID:0, ID:1][LEN]*/
				packet_inst->calc_crc_16_checksum = packet_inst->conf.crc_16_fptr(packet_inst->rx_buffer, (packet_inst->rx_buffer_ind - 2)); //subtract 2 bytes for [CRC16:0, CRC16:1]

				/*Copy received CRC checksum*/
				packet_inst->packet_rx.crc_16_checksum = UNSERIALIZE_UINT16(packet_inst->rx_buffer[CRC1_POS(packet_inst->packet_rx.len)], packet_inst->rx_buffer[CRC0_POS(packet_inst->packet_rx.len)]);

				/*Check if calculated checksum matches received*/
				if(packet_inst->calc_crc_16_checksum == packet_inst->packet_rx.crc_16_checksum)
				{
					/*Copy ID*/
					packet_inst->packet_rx.id = UNSERIALIZE_UINT16(packet_inst->rx_buffer[ID_1_POS], packet_inst->rx_buffer[ID_0_POS]);

					/*Copy data - if data in packet*/
					if(packet_inst->packet_rx.len > 0)
					{
						memcpy(packet_inst->packet_rx.payload, &packet_inst->rx_buffer[DATA_N_POS], packet_inst->packet_rx.len);
					}
					/*Fill with zeros*/
					else
					{
						memset(packet_inst->packet_rx.payload, 0, sizeof(packet_inst->packet_rx.payload));
					}

					/*Run command handler*/
					cmd_handler_fptr(packet_inst, packet_inst->packet_rx);
				}
				else
				{
					error_handler(packet_inst, PCKT_ID_ERR_CHECKSUM);
				}

				/*Clear buffer*/
				packet_inst->rx_buffer_ind = 0;
			}
		}
	}

	/*Clear buffer timeout if timeout has expired and there is data in the buffer*/
	if(((*packet_inst->conf.tick_ptr - packet_inst->last_tick) >= packet_inst->conf.clear_buffer_timeout) && (packet_inst->rx_buffer_ind > 0))
	{
		/*Clear buffer*/
		packet_inst->rx_buffer_ind = 0;

		error_handler(packet_inst, PCKT_ID_ERR_TIMEOUT);
	}
}

/******************************************************************************
*  \brief Software CRC (SLOW)
*
*  \note https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
*        CRC16_CCIT_ZERO
*        CRC-16 (CRC-CCITT)
*        Calculator: http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
******************************************************************************/
crc_t sw_crc(const uint8_t * const message, const uint32_t num_bytes)
{
	crc_t remainder = 0;

	/*Perform modulo-2 division, a byte at a time.*/
	for(uint32_t byte = 0; byte < num_bytes; ++byte)
	{
		/*Bring the next byte into the remainder.*/
		remainder ^= (message[byte] << (SW_CRC_WIDTH - 8));

		/*Perform modulo-2 division, a bit at a time.*/
		for(uint8_t bit = 8; bit > 0; --bit)
		{
			/*Try to divide the current data bit.*/
			if(remainder & SW_CRC_TOPBIT)
			{
				remainder = (remainder << 1) ^ SW_CRC_POLYNOMIAL;
			}
			else
			{
				remainder = (remainder << 1);
			}
		}
	}

	/*The final remainder is the CRC result.*/
	return remainder;
}

/******************************************************************************
*  \brief TX raw data
*
*  \note
******************************************************************************/
void packet_tx_raw(packet_inst_t * const packet_inst, const uint16_t id, const uint8_t * const data, uint8_t len)
{
	uint8_t packet[RX_BUFFER_LEN_BYTES];
	uint16_t checksum;

	/*If packet is disabled do not run*/
	if(packet_inst->conf.enable == PACKET_DISABLED) return;

	/*Limit len*/
	len = (len > MAX_PAYLOAD_LEN_BYTES ? MAX_PAYLOAD_LEN_BYTES : len);

	/*Copy data to holding array*/
	packet[ID_1_POS] = (uint8_t)(id >> 8);
	packet[ID_0_POS] = (uint8_t)id;
	packet[LEN_POS]  = len;

	memcpy(&packet[DATA_N_POS], data, len);

	/*Calc checksum*/
	checksum = packet_inst->conf.crc_16_fptr(packet, (len + 3));

	/*Copy checksum*/
	packet[CRC1_POS(len)] = (uint8_t)(checksum >> 8);
	packet[CRC0_POS(len)] = (uint8_t)checksum;

	/*TX packet*/
	packet_inst->conf.tx_data_fprt(packet, len + 5);
}

/******************************************************************************
*  \brief TX unsigned 8BIT
*
*  \note
******************************************************************************/
void packet_tx_u8(packet_inst_t * const packet_inst, const uint16_t id, const uint8_t data)
{
	packet_tx_raw(packet_inst, id, &data, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 8BIT
*
*  \note
******************************************************************************/
void packet_tx_s8(packet_inst_t * const packet_inst, const uint16_t id, const int8_t data)
{
    bit8_dat_t data_tmp;

    data_tmp._int = data;

	packet_tx_raw(packet_inst, id, &data_tmp._uint, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 16BIT
*
*  \note
******************************************************************************/
void packet_tx_u16(packet_inst_t * const packet_inst, const uint16_t id, const uint16_t data)
{
    uint8_t tmp[sizeof(data)];

    sr_16(tmp, data);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 16BIT
*
*  \note
******************************************************************************/
void packet_tx_s16(packet_inst_t * const packet_inst, const uint16_t id, const int16_t data)
{
    uint8_t tmp[sizeof(data)];
    bit16_dat_t data_tmp;

    data_tmp._int = data;

    sr_16(tmp, data_tmp._uint);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 32BIT
*
*  \note
******************************************************************************/
void packet_tx_u32(packet_inst_t * const packet_inst, const uint16_t id, const uint32_t data)
{
    uint8_t tmp[sizeof(data)];

    sr_32(tmp, data);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 32BIT
*
*  \note
******************************************************************************/
void packet_tx_s32(packet_inst_t * const packet_inst, const uint16_t id, const int32_t data)
{
    uint8_t tmp[sizeof(data)];
    bit32_dat_t data_tmp;

    data_tmp._int = data;

    sr_32(tmp, data_tmp._uint);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX float 32BIT
*
*  \note
******************************************************************************/
void packet_tx_float_32(packet_inst_t * const packet_inst, const uint16_t id, const float data)
{
    uint8_t tmp[sizeof(data)];
    bit32_dat_t data_tmp;

    data_tmp._flt = data;

    sr_32(tmp, data_tmp._uint);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 64BIT
*
*  \note
******************************************************************************/
void packet_tx_u64(packet_inst_t * const packet_inst, const uint16_t id, const uint64_t data)
{
    uint8_t tmp[sizeof(data)];

    sr_64(tmp, data);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 64BIT
*
*  \note
******************************************************************************/
void packet_tx_s64(packet_inst_t * const packet_inst, const uint16_t id, const int64_t data)
{
    uint8_t tmp[sizeof(data)];
    bit64_dat_t data_tmp;

    data_tmp._int = data;

    sr_64(tmp, data_tmp._uint);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief TX double 64BIT
*
*  \note
******************************************************************************/
void packet_tx_double_64(packet_inst_t * const packet_inst, const uint16_t id, const double data)
{
    uint8_t tmp[sizeof(data)];
    bit64_dat_t data_tmp;

    data_tmp._dbl = data;

    sr_64(tmp, data_tmp._uint);
	packet_tx_raw(packet_inst, id, tmp, sizeof(data));
}

/******************************************************************************
*  \brief Packet enable disable
*
*  \note disables or enables packet task and packet_tx_raw
******************************************************************************/
void packet_enable(packet_inst_t * const packet_inst, const packet_enable_t enable)
{
	packet_inst->conf.enable = enable;
}

/******************************************************************************
*  \brief Packet payload convert to uint16
*
*  \note
******************************************************************************/
uint8_t packet_payload_uint8(const packet_rx_t packet_rx)
{
	return packet_rx.payload[0];
}

/******************************************************************************
*  \brief Packet payload convert to int16
*
*  \note
******************************************************************************/
int8_t packet_payload_int8(const packet_rx_t packet_rx)
{
    bit8_dat_t tmp;

    tmp._uint = packet_rx.payload[0];

	return tmp._int;
}

/******************************************************************************
*  \brief Packet payload convert to uint16
*
*  \note
******************************************************************************/
uint16_t packet_payload_uint16(const packet_rx_t packet_rx)
{
    bit16_dat_t tmp;

    tmp = unsr_16(packet_rx.payload);

	return tmp._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int16
*
*  \note
******************************************************************************/
int16_t packet_payload_int16(const packet_rx_t packet_rx)
{
    bit16_dat_t tmp;

    tmp = unsr_16(packet_rx.payload);

	return tmp._int;
}

/******************************************************************************
*  \brief Packet payload convert to uint32
*
*  \note
******************************************************************************/
uint32_t packet_payload_uint32(const packet_rx_t packet_rx)
{
	bit32_dat_t tmp;

    tmp = unsr_32(packet_rx.payload);

	return tmp._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int32
*
*  \note
******************************************************************************/
int32_t packet_payload_int32(const packet_rx_t packet_rx)
{
	bit32_dat_t tmp;

    tmp = unsr_32(packet_rx.payload);

	return tmp._int;
}

/******************************************************************************
*  \brief Packet payload convert to float 32-bit
*
*  \note
******************************************************************************/
float packet_payload_float_32(const packet_rx_t packet_rx)
{
	bit32_dat_t tmp;

    tmp = unsr_32(packet_rx.payload);

	return tmp._flt;
}

/******************************************************************************
*  \brief Packet payload convert to uint64
*
*  \note
******************************************************************************/
uint64_t packet_payload_uint64(const packet_rx_t packet_rx)
{
	bit64_dat_t tmp;

    tmp = unsr_64(packet_rx.payload);

	return tmp._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int64
*
*  \note
******************************************************************************/
int64_t packet_payload_int64(const packet_rx_t packet_rx)
{
	bit64_dat_t tmp;

    tmp = unsr_64(packet_rx.payload);

	return tmp._int;
}

/******************************************************************************
*  \brief Packet payload convert to double 64-bit
*
*  \note
******************************************************************************/
double packet_payload_double_64(const packet_rx_t packet_rx)
{
	bit64_dat_t tmp;

    tmp = unsr_64(packet_rx.payload);

	return tmp._dbl;
}


/**************************************************************************************************
*                                         LOCAL FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief Default rx byte function
*
*  \note
******************************************************************************/
static int16_t default_rx_byte(void)
{
	return -1;
}

/******************************************************************************
*  \brief Default tx data function
*
*  \note
******************************************************************************/
static void default_tx_data(const uint8_t * const data, const uint32_t length)
{
	//empty
}

/******************************************************************************
*  \brief Error handler
*
*  \note This sends a packet which contains no data bytes but the ID indicates the error
******************************************************************************/
static void error_handler(packet_inst_t * const packet_inst, const packet_id_err_t error)
{
	packet_tx_raw(packet_inst, (uint16_t)error, 0, 0);
}

/******************************************************************************
*  \brief Reverse memory copy
*
*  \note used to invert endianness
******************************************************************************/
static void * revmemcpy(void *dest, const void *src, size_t len)
{
	uint8_t *d = (uint8_t *)dest + len - 1;
	const uint8_t *s = src;

	while(len--)
		*d-- = *s++;

	return dest;
}

/******************************************************************************
*  \brief Unserialize 16bit
*
*  \note
******************************************************************************/
static bit16_dat_t unsr_16(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit16_dat_t tmp = {0};

    for(i = 0; i < sizeof(tmp); i++)
    {
        tmp._uint |= ((uint16_t)big_endian_data[i] << (sizeof(tmp) - 1 - i) * 8);
    }

    return tmp;
}

/******************************************************************************
*  \brief Unserialize 32bit
*
*  \note
******************************************************************************/
static bit32_dat_t unsr_32(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit32_dat_t tmp = {0};

    for(i = 0; i < sizeof(tmp); i++)
    {
        tmp._uint |= ((uint32_t)big_endian_data[i] << (sizeof(tmp) - 1 - i) * 8);
    }

    return tmp;
}

/******************************************************************************
*  \brief Unserialize 64bit
*
*  \note
******************************************************************************/
static bit64_dat_t unsr_64(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit64_dat_t tmp = {0};

    for(i = 0; i < sizeof(tmp); i++)
    {
        tmp._uint |= ((uint64_t)big_endian_data[i] << (sizeof(tmp) - 1 - i) * 8);
    }

    return tmp;
}

/******************************************************************************
*  \brief Serialize 16bit, big endian
*
*  \note
******************************************************************************/
static void sr_16(uint8_t * const dest, const uint16_t src)
{
    uint8_t i;

    for(i = 0; i < sizeof(src); i++)
    {
        dest[i] = (uint8_t)(src >> (sizeof(src) - 1 - i) * 8);
    }
}

/******************************************************************************
*  \brief Serialize 32bit, big endian
*
*  \note
******************************************************************************/
static void sr_32(uint8_t * const dest, const uint32_t src)
{
    uint8_t i;

    for(i = 0; i < sizeof(src); i++)
    {
        dest[i] = (uint8_t)(src >> (sizeof(src) - 1 - i) * 8);
    }
}

/******************************************************************************
*  \brief Serialize 64bit, big endian
*
*  \note
******************************************************************************/
static void sr_64(uint8_t * const dest, const uint64_t src)
{
    uint8_t i;

    for(i = 0; i < sizeof(src); i++)
    {
        dest[i] = (uint8_t)(src >> (sizeof(src) - 1 - i) * 8);
    }
}
