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
static int16_t     dflt_rx_byte   (void);
static void        dflt_tx_data   (const uint8_t * const data, const uint32_t length);
static void        err_send       (pckt_inst_t * const packet_inst, const pckt_id_err_t error);
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
void pckt_get_config_defaults(pckt_conf_t * const pckt_conf)
{
	pckt_conf->tick_ptr              = NULL;
	pckt_conf->rx_byte_fptr          = dflt_rx_byte;
	pckt_conf->tx_data_fprt          = dflt_tx_data;
	pckt_conf->crc_16_fptr           = pckt_sw_crc;
	pckt_conf->clear_buffer_timeout  = 0xFFFFFFFF;
	pckt_conf->enable                = PCKT_ENABLED;
}

/******************************************************************************
*  \brief Packet init
*
*  \note
******************************************************************************/
void pckt_init(pckt_inst_t * const pckt_inst, const pckt_conf_t pckt_conf)
{
	/*Conf*/
	pckt_inst->conf = pckt_conf;

	/*Inst*/
	pckt_inst->rx_byte              = 0;
	memset(pckt_inst->rx_buffer, 0, sizeof(pckt_inst->rx_buffer));
	pckt_inst->rx_buffer_ind        = 0;
	pckt_inst->calc_crc_16_checksum = 0;
	memset(&pckt_inst->pckt_rx, 0, sizeof(pckt_inst->pckt_rx));
	pckt_inst->last_tick            = *pckt_inst->conf.tick_ptr;
}

/******************************************************************************
*  \brief Packet task
*
*  \note takes pointer to instance and function pointer to command handler
******************************************************************************/
void pckt_task(pckt_inst_t * const pckt_inst, void(*cmd_handler_fptr)(pckt_inst_t * const, const pckt_rx_t))
{
	/*If packet is disabled do not run*/
	if(pckt_inst->conf.enable == PCKT_DISABLED) return;

	/*Get byte*/
	pckt_inst->rx_byte = pckt_inst->conf.rx_byte_fptr();

	/*Check for received byte*/
	if(pckt_inst->rx_byte != -1)
	{
		/*Record time of last byte*/
		pckt_inst->last_tick = *pckt_inst->conf.tick_ptr;

		/*Is received buffer full?*/
		if(pckt_inst->rx_buffer_ind == RX_BUFFER_LEN_BYTES)
		{
			/*Set to the last byte*/
			pckt_inst->rx_buffer_ind -= 1;

			/*Making it here means the received buffer is full*/
		}
		else
        {
            /*Put received byte in buffer*/
            pckt_inst->rx_buffer[pckt_inst->rx_buffer_ind] = (uint8_t)pckt_inst->rx_byte;
            pckt_inst->rx_buffer_ind++;
        }

		/*Check for valid packet - after ID:0 ID:1 and LEN bytes received*/
		if(pckt_inst->rx_buffer_ind >= 3)
		{
			/*Copy LEN*/
			pckt_inst->pckt_rx.len = pckt_inst->rx_buffer[LEN_POS];

			/*Verify LEN*/
			if(pckt_inst->pckt_rx.len > MAX_PAYLOAD_LEN_BYTES)
			{
				/*If not going to fit force it down to the max*/
				pckt_inst->pckt_rx.len = MAX_PAYLOAD_LEN_BYTES;
			}

			/*Check data received to see if all bytes received - the +5 is [ID:0, ID:1][LEN][CRC16:0, CRC16:1]*/
			if((pckt_inst->pckt_rx.len + 5) == pckt_inst->rx_buffer_ind)
			{
				/*Calculate checksum - performed on [ID:0, ID:1][LEN][PAYLOAD:0 ...,  PAYLOAD:n] if LEN=0 then just [ID:0, ID:1][LEN]*/
				pckt_inst->calc_crc_16_checksum = pckt_inst->conf.crc_16_fptr(pckt_inst->rx_buffer, (pckt_inst->rx_buffer_ind - 2)); //subtract 2 bytes for [CRC16:0, CRC16:1]

				/*Copy received CRC checksum*/
				pckt_inst->pckt_rx.crc_16_checksum = UNSERIALIZE_UINT16(pckt_inst->rx_buffer[CRC1_POS(pckt_inst->pckt_rx.len)], pckt_inst->rx_buffer[CRC0_POS(pckt_inst->pckt_rx.len)]);

				/*Check if calculated checksum matches received*/
				if(pckt_inst->calc_crc_16_checksum == pckt_inst->pckt_rx.crc_16_checksum)
				{
					/*Copy ID*/
					pckt_inst->pckt_rx.id = UNSERIALIZE_UINT16(pckt_inst->rx_buffer[ID_1_POS], pckt_inst->rx_buffer[ID_0_POS]);

					/*Copy data - if data in packet*/
					if(pckt_inst->pckt_rx.len > 0)
					{
						memcpy(pckt_inst->pckt_rx.payload, &pckt_inst->rx_buffer[DATA_N_POS], pckt_inst->pckt_rx.len);
					}
					/*Fill with zeros*/
					else
					{
						memset(pckt_inst->pckt_rx.payload, 0, sizeof(pckt_inst->pckt_rx.payload));
					}

					/*Run command handler*/
					cmd_handler_fptr(pckt_inst, pckt_inst->pckt_rx);
				}
				else
				{
					err_send(pckt_inst, PCKT_ID_ERR_CHECKSUM);
				}

				/*Clear buffer*/
				pckt_inst->rx_buffer_ind = 0;
			}
		}
	}

	/*Clear buffer timeout if timeout has expired and there is data in the buffer*/
	if(((*pckt_inst->conf.tick_ptr - pckt_inst->last_tick) >= pckt_inst->conf.clear_buffer_timeout) && (pckt_inst->rx_buffer_ind > 0))
	{
		/*Clear buffer*/
		pckt_inst->rx_buffer_ind = 0;

		err_send(pckt_inst, PCKT_ID_ERR_TIMEOUT);
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
crc_t pckt_sw_crc(const uint8_t * const message, const uint32_t num_bytes)
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
void pckt_tx_raw(pckt_inst_t * const pckt_inst, const uint16_t id, const uint8_t * const data, uint8_t len)
{
	uint8_t pckt[RX_BUFFER_LEN_BYTES];
	uint16_t checksum;

	/*If packet is disabled do not run*/
	if(pckt_inst->conf.enable == PCKT_DISABLED) return;

	/*Limit len*/
	len = (len > MAX_PAYLOAD_LEN_BYTES ? MAX_PAYLOAD_LEN_BYTES : len);

	/*Copy data to holding array*/
	pckt[ID_1_POS] = (uint8_t)(id >> 8);
	pckt[ID_0_POS] = (uint8_t)id;
	pckt[LEN_POS]  = len;

	memcpy(&pckt[DATA_N_POS], data, len);

	/*Calc checksum*/
	checksum = pckt_inst->conf.crc_16_fptr(pckt, (len + 3));

	/*Copy checksum*/
	pckt[CRC1_POS(len)] = (uint8_t)(checksum >> 8);
	pckt[CRC0_POS(len)] = (uint8_t)checksum;

	/*TX packet*/
	pckt_inst->conf.tx_data_fprt(pckt, len + 5);
}

/******************************************************************************
*  \brief TX unsigned 8BIT
*
*  \note
******************************************************************************/
void pckt_tx_u8(pckt_inst_t * const pckt_inst, const uint16_t id, const uint8_t data)
{
	pckt_tx_raw(pckt_inst, id, &data, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 8BIT
*
*  \note
******************************************************************************/
void pckt_tx_s8(pckt_inst_t * const pckt_inst, const uint16_t id, const int8_t data)
{
    bit8_dat_t bit8_dat;

    bit8_dat._int = data;

	pckt_tx_raw(pckt_inst, id, &bit8_dat._uint, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 16BIT
*
*  \note
******************************************************************************/
void pckt_tx_u16(pckt_inst_t * const pckt_inst, const uint16_t id, const uint16_t data)
{
    uint8_t pckt[sizeof(data)];

    sr_16(pckt, data);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 16BIT
*
*  \note
******************************************************************************/
void pckt_tx_s16(pckt_inst_t * const pckt_inst, const uint16_t id, const int16_t data)
{
    uint8_t pckt[sizeof(data)];
    bit16_dat_t bit16_dat;

    bit16_dat._int = data;

    sr_16(pckt, bit16_dat._uint);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 32BIT
*
*  \note
******************************************************************************/
void pckt_tx_u32(pckt_inst_t * const pckt_inst, const uint16_t id, const uint32_t data)
{
    uint8_t pckt[sizeof(data)];

    sr_32(pckt, data);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 32BIT
*
*  \note
******************************************************************************/
void pckt_tx_s32(pckt_inst_t * const pckt_inst, const uint16_t id, const int32_t data)
{
    uint8_t pckt[sizeof(data)];
    bit32_dat_t bit32_dat;

    bit32_dat._int = data;

    sr_32(pckt, bit32_dat._uint);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX float 32BIT
*
*  \note
******************************************************************************/
void pckt_tx_float_32(pckt_inst_t * const pckt_inst, const uint16_t id, const float data)
{
    uint8_t pckt[sizeof(data)];
    bit32_dat_t bit32_dat;

    bit32_dat._flt = data;

    sr_32(pckt, bit32_dat._uint);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX unsigned 64BIT
*
*  \note
******************************************************************************/
void pckt_tx_u64(pckt_inst_t * const pckt_inst, const uint16_t id, const uint64_t data)
{
    uint8_t pckt[sizeof(data)];

    sr_64(pckt, data);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX signed 64BIT
*
*  \note
******************************************************************************/
void pckt_tx_s64(pckt_inst_t * const pckt_inst, const uint16_t id, const int64_t data)
{
    uint8_t pckt[sizeof(data)];
    bit64_dat_t bit64_dat;

    bit64_dat._int = data;

    sr_64(pckt, bit64_dat._uint);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief TX double 64BIT
*
*  \note
******************************************************************************/
void pckt_tx_double_64(pckt_inst_t * const pckt_inst, const uint16_t id, const double data)
{
    uint8_t pckt[sizeof(data)];
    bit64_dat_t bit64_dat;

    bit64_dat._dbl = data;

    sr_64(pckt, bit64_dat._uint);
	pckt_tx_raw(pckt_inst, id, pckt, sizeof(data));
}

/******************************************************************************
*  \brief Packet enable disable
*
*  \note disables or enables packet task and packet_tx_raw
******************************************************************************/
void pckt_enable(pckt_inst_t * const pckt_inst, const pckt_en_t enable)
{
	pckt_inst->conf.enable = enable;
}

/******************************************************************************
*  \brief Packet payload convert to uint16
*
*  \note
******************************************************************************/
uint8_t pckt_rx_u8(const pckt_rx_t pckt_rx)
{
	return pckt_rx.payload[0];
}

/******************************************************************************
*  \brief Packet payload convert to int16
*
*  \note
******************************************************************************/
int8_t pckt_rx_s8(const pckt_rx_t pckt_rx)
{
    bit8_dat_t bit8_dat;

    bit8_dat._uint = pckt_rx.payload[0];

	return bit8_dat._int;
}

/******************************************************************************
*  \brief Packet payload convert to uint16
*
*  \note
******************************************************************************/
uint16_t pckt_rx_u16(const pckt_rx_t pckt_rx)
{
    bit16_dat_t bit16_dat;

    bit16_dat = unsr_16(pckt_rx.payload);

	return bit16_dat._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int16
*
*  \note
******************************************************************************/
int16_t pckt_rx_s16(const pckt_rx_t pckt_rx)
{
    bit16_dat_t bit16_dat;

    bit16_dat = unsr_16(pckt_rx.payload);

	return bit16_dat._int;
}

/******************************************************************************
*  \brief Packet payload convert to uint32
*
*  \note
******************************************************************************/
uint32_t pckt_rx_u32(const pckt_rx_t pckt_rx)
{
	bit32_dat_t bit32_dat;

    bit32_dat = unsr_32(pckt_rx.payload);

	return bit32_dat._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int32
*
*  \note
******************************************************************************/
int32_t pckt_rx_s32(const pckt_rx_t pckt_rx)
{
	bit32_dat_t bit32_dat;

    bit32_dat = unsr_32(pckt_rx.payload);

	return bit32_dat._int;
}

/******************************************************************************
*  \brief Packet payload convert to float 32-bit
*
*  \note
******************************************************************************/
float pckt_rx_flt32(const pckt_rx_t pckt_rx)
{
	bit32_dat_t bit32_dat;

    bit32_dat = unsr_32(pckt_rx.payload);

	return bit32_dat._flt;
}

/******************************************************************************
*  \brief Packet payload convert to uint64
*
*  \note
******************************************************************************/
uint64_t pckt_rx_u64(const pckt_rx_t pckt_rx)
{
	bit64_dat_t bit64_dat;

    bit64_dat = unsr_64(pckt_rx.payload);

	return bit64_dat._uint;
}

/******************************************************************************
*  \brief Packet payload convert to int64
*
*  \note
******************************************************************************/
int64_t pckt_rx_s64(const pckt_rx_t pckt_rx)
{
	bit64_dat_t bit64_dat;

    bit64_dat = unsr_64(pckt_rx.payload);

	return bit64_dat._int;
}

/******************************************************************************
*  \brief Packet payload convert to double 64-bit
*
*  \note
******************************************************************************/
double pckt_rx_dbl64(const pckt_rx_t pckt_rx)
{
	bit64_dat_t bit64_dat;

    bit64_dat = unsr_64(pckt_rx.payload);

	return bit64_dat._dbl;
}


/**************************************************************************************************
*                                         LOCAL FUNCTIONS
*************************************************^************************************************/
/******************************************************************************
*  \brief Default rx byte function
*
*  \note
******************************************************************************/
static int16_t dflt_rx_byte(void)
{
	return -1;
}

/******************************************************************************
*  \brief Default tx data function
*
*  \note
******************************************************************************/
static void dflt_tx_data(const uint8_t * const data, const uint32_t length)
{
	//empty
}

/******************************************************************************
*  \brief Error handler
*
*  \note This sends a packet which contains no data bytes but the ID indicates the error
******************************************************************************/
static void err_send(pckt_inst_t * const pckt_inst, const pckt_id_err_t error)
{
	pckt_tx_raw(pckt_inst, (uint16_t)error, 0, 0);
}

/******************************************************************************
*  \brief Unserialize 16bit
*
*  \note
******************************************************************************/
static bit16_dat_t unsr_16(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit16_dat_t bit16_dat = {0};

    for(i = 0; i < sizeof(bit16_dat); i++)
    {
        bit16_dat._uint |= ((uint16_t)big_endian_data[i] << (sizeof(bit16_dat) - 1 - i) * 8);
    }

    return bit16_dat;
}

/******************************************************************************
*  \brief Unserialize 32bit
*
*  \note
******************************************************************************/
static bit32_dat_t unsr_32(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit32_dat_t bit32_dat = {0};

    for(i = 0; i < sizeof(bit32_dat); i++)
    {
        bit32_dat._uint |= ((uint32_t)big_endian_data[i] << (sizeof(bit32_dat) - 1 - i) * 8);
    }

    return bit32_dat;
}

/******************************************************************************
*  \brief Unserialize 64bit
*
*  \note
******************************************************************************/
static bit64_dat_t unsr_64(const uint8_t * const big_endian_data)
{
    uint8_t i;
    bit64_dat_t bit64_dat = {0};

    for(i = 0; i < sizeof(bit64_dat); i++)
    {
        bit64_dat._uint |= ((uint64_t)big_endian_data[i] << (sizeof(bit64_dat) - 1 - i) * 8);
    }

    return bit64_dat;
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
