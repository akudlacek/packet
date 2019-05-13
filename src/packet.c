/*
* packet.c
*
* Created: 5/4/2018 3:52:08 PM
*  Author: arin
*/

/*
* Packet format:
* Little endian meaning LSB sent first e.g 0x40CC is sent as 0xCC 0x40 same is expected for receive
* 
* 0x12345678
*   ^^    ^^
*  MSB   LSB
*  LAST  1ST
*
* NO DATA LEN=0: [ID][LEN][CRC16 on ID and LEN BYTE:0,BYTE:1]
*    DATA LEN>0: [ID][LEN][BYTE:0 ... BYTE:n][CRC16 on ID, LEN, and DATA0..DATA:n BYTE:0,BYTE:1]
*/


#include "packet.h"

#include <string.h>

/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
typedef enum
{
	CHECKSUM_ERROR         = 0x02
} packet_error_t;


/**************************************************************************************************
*                                         LOCAL PROTOTYPES
*************************************************^************************************************/
static int16_t default_rx_byte(void);
static void default_tx_data(const uint8_t * const data, uint32_t length);
static void default_command_handler(packet_inst_t * const packet_inst, packet_rx_t packet_rx);
static void error_handler(packet_inst_t * const packet_inst, packet_error_t error);


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
	packet_conf->tick_ms_ptr           = NULL;
	packet_conf->rx_byte_fptr          = default_rx_byte;
	packet_conf->tx_data_fprt          = default_tx_data;
	packet_conf->cmd_handler_fptr      = (void *)default_command_handler;
	packet_conf->crc_16_fptr           = sw_crc;
	packet_conf->clear_buffer_timeout  = 0xFFFFFFFF;
	packet_conf->enable                = PACKET_ENABLED;
}

/******************************************************************************
*  \brief Packet init
*
*  \note
******************************************************************************/
void packet_init(packet_inst_t * const packet_inst, packet_conf_t packet_conf)
{
	packet_inst->conf.tick_ms_ptr           = packet_conf.tick_ms_ptr;
	packet_inst->conf.rx_byte_fptr          = packet_conf.rx_byte_fptr;
	packet_inst->conf.tx_data_fprt          = packet_conf.tx_data_fprt;
	packet_inst->conf.cmd_handler_fptr      = packet_conf.cmd_handler_fptr;
	packet_inst->conf.crc_16_fptr           = packet_conf.crc_16_fptr;
	packet_inst->conf.clear_buffer_timeout  = packet_conf.clear_buffer_timeout;
	packet_inst->conf.enable                = packet_conf.enable;
}

/******************************************************************************
*  \brief Packet task
*
*  \note
******************************************************************************/
void packet_task(packet_inst_t * const packet_inst)
{
	/*If packet is disabled do not run*/
	if(packet_inst->conf.enable == PACKET_DISABLED) return;
	
	/*Get byte*/
	packet_inst->rx_byte = packet_inst->conf.rx_byte_fptr();
	
	/*Check for received byte*/
	if(packet_inst->rx_byte != -1)
	{
		/*Record time of last byte*/
		packet_inst->last_tick = *packet_inst->conf.tick_ms_ptr;
		
		/*Check buffer limit*/
		if(packet_inst->rx_buffer_ind >= RX_BUFFER_LEN_BYTES)
		{
			/*Set to the last byte*/
			packet_inst->rx_buffer_ind = RX_BUFFER_LEN_BYTES - 1;
		}
		
		/*Put received byte in buffer*/
		packet_inst->rx_buffer[packet_inst->rx_buffer_ind] = (uint8_t)packet_inst->rx_byte;
		packet_inst->rx_buffer_ind++;
		
		/*Check for valid packet - after ID and data length bytes received*/
		if(packet_inst->rx_buffer_ind >= 2)
		{
			/*Copy LEN*/
			packet_inst->packet_rx.len = packet_inst->rx_buffer[1];
			
			/*Check to see if data length limit*/
			if(packet_inst->packet_rx.len > MAX_PAYLOAD_LEN_BYTES)
			{
				/*Set to the max bytes*/
				packet_inst->packet_rx.len = MAX_PAYLOAD_LEN_BYTES;
			}
			
			/*Check data received to see if all bytes received - the +4 is ID, SIZE, and two CHECKSUM bytes*/
			if((packet_inst->packet_rx.len + 4) == packet_inst->rx_buffer_ind)
			{
				/*Calculate checksum - performed on [ID][LEN][DATA] if LEN=0 then just [ID][LEN]*/
				packet_inst->calc_crc_16_checksum = packet_inst->conf.crc_16_fptr(packet_inst->rx_buffer, (packet_inst->rx_buffer_ind - 2));
				
				/*Copy received CRC checksum*/
				packet_inst->packet_rx.crc_16_checksum = ((uint16_t)(packet_inst->rx_buffer[packet_inst->rx_buffer_ind - 1] << 8) | (uint16_t)packet_inst->rx_buffer[packet_inst->rx_buffer_ind - 2]);
				
				/*Check if calculated checksum matches received*/
				if(packet_inst->calc_crc_16_checksum == packet_inst->packet_rx.crc_16_checksum)
				{
					/*Copy ID*/
					packet_inst->packet_rx.id = packet_inst->rx_buffer[0];
					
					/*Copy data - if data in packet*/
					if(packet_inst->packet_rx.len > 0)
					{
						memcpy(packet_inst->packet_rx.payload, &packet_inst->rx_buffer[2], packet_inst->packet_rx.len);
					}
					/*Fill with zeros*/
					else
					{
						memset(packet_inst->packet_rx.payload, 0, MAX_PAYLOAD_LEN_BYTES);
					}
					
					/*Run command handler*/
					packet_inst->conf.cmd_handler_fptr(packet_inst, packet_inst->packet_rx);
				}
				else
				{
					error_handler(packet_inst, CHECKSUM_ERROR);
				}
				
				/*Clear buffer*/
				packet_inst->rx_buffer_ind = 0;
			}
		}
	}
	
	/*Clear buffer timeout*/
	if((*packet_inst->conf.tick_ms_ptr - packet_inst->last_tick) >= packet_inst->conf.clear_buffer_timeout)
	{
		/*Clear buffer*/
		packet_inst->rx_buffer_ind = 0;
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
crc_t sw_crc(const uint8_t * const message, uint32_t num_bytes)
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
void packet_tx_raw(packet_inst_t * const packet_inst, uint8_t id, const uint8_t * const data, uint8_t len)
{
	/*If packet is disabled do not run*/
	if(packet_inst->conf.enable == PACKET_DISABLED) return;
	
	uint8_t packet[RX_BUFFER_LEN_BYTES];
	uint16_t checksum;
	
	/*Limit len*/
	len = (len > MAX_PAYLOAD_LEN_BYTES ? MAX_PAYLOAD_LEN_BYTES : len);
	
	/*Copy data to holding array*/
	packet[0] = id;
	packet[1] = len;
	memcpy(&packet[2], data, len);
	
	/*Calc checksum*/
	checksum = packet_inst->conf.crc_16_fptr(packet, (len + 2));
	
	/*Copy checksum*/
	packet[2+len] = (uint8_t)checksum;
	packet[3+len] = (uint8_t)(checksum >> 8);
	
	/*TX packet*/
	packet_inst->conf.tx_data_fprt(packet, len + 4);
}

/******************************************************************************
*  \brief TX float 32BIT
*
*  \note
******************************************************************************/
void packet_tx_float_32(packet_inst_t * const packet_inst, uint8_t id, float data)
{
	packet_tx_raw(packet_inst, id, (uint8_t*)&data, 4);
}

/******************************************************************************
*  \brief TX double 64BIT
*
*  \note
******************************************************************************/
void packet_tx_double_64(packet_inst_t * const packet_inst, uint8_t id, double data)
{
	packet_tx_raw(packet_inst, id, (uint8_t*)&data, 8);
}

/******************************************************************************
*  \brief TX 8BIT
*
*  \note
******************************************************************************/
void packet_tx_8(packet_inst_t * const packet_inst, uint8_t id, uint8_t data)
{
	packet_tx_raw(packet_inst, id, &data, 1);
}

/******************************************************************************
*  \brief TX 16BIT
*
*  \note
******************************************************************************/
void packet_tx_16(packet_inst_t * const packet_inst, uint8_t id, uint16_t data)
{
	packet_tx_raw(packet_inst, id, (uint8_t*)&data, 2);
}

/******************************************************************************
*  \brief TX 32BIT
*
*  \note
******************************************************************************/
void packet_tx_32(packet_inst_t * const packet_inst, uint8_t id, uint32_t data)
{
	packet_tx_raw(packet_inst, id, (uint8_t*)&data, 4);
}

/******************************************************************************
*  \brief TX 64BIT
*
*  \note
******************************************************************************/
void packet_tx_64(packet_inst_t * const packet_inst, uint8_t id, uint64_t data)
{
	packet_tx_raw(packet_inst, id, (uint8_t*)&data, 8);
}

/******************************************************************************
*  \brief Packet enable disable
*
*  \note disables or enables packet task and packet_tx_raw
******************************************************************************/
void packet_enable(packet_inst_t * const packet_inst, packet_enable_t enable)
{
	packet_inst->conf.enable = enable;
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
static void default_tx_data(const uint8_t * const data, uint32_t length)
{
	//empty
}

/******************************************************************************
*  \brief Default command handler
*
*  \note
******************************************************************************/
static void default_command_handler(packet_inst_t * const packet_inst, packet_rx_t packet_rx)
{
	//empty
}

/******************************************************************************
*  \brief Error handler
*
*  \note
******************************************************************************/
static void error_handler(packet_inst_t * const packet_inst, packet_error_t error)
{
	uint8_t data[1];
	
	data[0] = error;
	
	packet_tx_raw(packet_inst, 0xFF, data, 1);
}
