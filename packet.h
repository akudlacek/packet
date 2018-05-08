/*
 * packet.h
 *
 * Created: 5/4/2018 3:52:27 PM
 *  Author: arin
 */ 


#ifndef PACKET_H_
#define PACKET_H_


#include <stdint.h>


/******************************************************************************
* Defines
******************************************************************************/
#define MAX_PAYLOAD_LEN_BYTES 8

/*Set up for CRC-16 (CRC-CCITT)*/
typedef uint16_t crc_t; //The width of the CRC calculation and result. Modify the typedef for a 16 or 32-bit CRC standard.
#define SW_CRC_POLYNOMIAL 0x1021
#define SW_CRC_WIDTH  (8 * sizeof(crc_t))
#define SW_CRC_TOPBIT (1 << (SW_CRC_WIDTH - 1))

/*Packet received struct*/
typedef struct
{
	uint8_t id;
	uint8_t len;
	uint8_t payload[MAX_PAYLOAD_LEN_BYTES];
} packet_rx_t;

/*Packet configuration struct*/
typedef	struct
{
	int16_t (*rx_byte_fptr)(void);                  //function pointer for received byte return -1 for no data or >=0 for valid data
	void (*tx_data_fprt)(uint8_t *, uint32_t);      //function pointer for transmit, ptr to 8 bit data array and length
	void (*cmd_handler_fptr)(packet_rx_t);          //function pointer for command handler
} packet_conf_t;

/*Packet instance struct*/
typedef struct 
{
	packet_conf_t conf;
} packet_inst_t;


/******************************************************************************
* Prototypes
******************************************************************************/
void packet_get_config_defaults(packet_conf_t *packet_conf);
void packet_init(packet_inst_t *packet_inst, packet_conf_t packet_conf);
void packet_task(packet_inst_t *packet_inst);
crc_t sw_crc(uint8_t const message[], int nBytes);


#endif /* PACKET_H_ */