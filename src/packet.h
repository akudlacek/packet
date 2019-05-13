/*
 * packet.h
 *
 * Created: 5/4/2018 3:52:27 PM
 *  Author: arin
 */ 


#ifndef PACKET_H_
#define PACKET_H_


#include <stdint.h>


/**************************************************************************************************
*                                             DEFINES
*************************************************^************************************************/
#define MAX_PAYLOAD_LEN_BYTES 8
#define RX_BUFFER_LEN_BYTES (MAX_PAYLOAD_LEN_BYTES + 4)

/*Packet enable disable enum*/
typedef enum
{
	PACKET_DISABLED,
	PACKET_ENABLED
} packet_enable_t;

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
	uint16_t crc_16_checksum;
} packet_rx_t;

/*Packet configuration struct*/
typedef	struct
{
	const volatile uint32_t *tick_ms_ptr;                     //pointer to sys tick in mS
	int16_t (*rx_byte_fptr)(void);                            //function pointer for received byte return -1 for no data or >=0 for valid data
	void (*tx_data_fprt)(const uint8_t * const, uint32_t);    //function pointer for transmit, ptr to 8 bit data array and length
	
	/****!!!! Void ptr in field 1 is actually (packet_inst_t *),  !!!!****
	 ****!!!! the reason for this is this type is within          !!!!****
	 ****!!!! packet_inst_t and is not yet defined, I don't know  !!!!****
	 ****!!!! of a better way to do this                          !!!!****/
	void (*cmd_handler_fptr)(void *, packet_rx_t);            //function pointer for command handler
	
	uint16_t (*crc_16_fptr)(const uint8_t *, uint32_t);       //function pointer for crc-16, default will be sw_crc
	uint32_t clear_buffer_timeout;                            //timeout for buffer to be cleared when incomplete packet received
	packet_enable_t enable;                                   //enable or disable packet instance
} packet_conf_t;

/*Packet instance struct*/
typedef struct 
{
	packet_conf_t conf;
	
	int16_t rx_byte;
	uint8_t rx_buffer[RX_BUFFER_LEN_BYTES];
	uint8_t rx_buffer_ind;
	uint16_t calc_crc_16_checksum;
	packet_rx_t packet_rx;
	uint32_t last_tick;
} packet_inst_t;


/**************************************************************************************************
*                                            PROTOTYPES
*************************************************^************************************************/
void packet_get_config_defaults(packet_conf_t * const packet_conf);
void packet_init(packet_inst_t * const packet_inst, packet_conf_t packet_conf);
void packet_task(packet_inst_t * const packet_inst);
crc_t sw_crc(const uint8_t * const message, uint32_t num_bytes);
void packet_tx_raw(packet_inst_t * const packet_inst, uint8_t id, const uint8_t * const data, uint8_t len);
void packet_tx_float_32(packet_inst_t * const packet_inst, uint8_t id, float data);
void packet_tx_double_64(packet_inst_t * const packet_inst, uint8_t id, double data);
void packet_tx_8(packet_inst_t * const packet_inst, uint8_t id, uint8_t data);
void packet_tx_16(packet_inst_t * const packet_inst, uint8_t id, uint16_t data);
void packet_tx_32(packet_inst_t * const packet_inst, uint8_t id, uint32_t data);
void packet_tx_64(packet_inst_t * const packet_inst, uint8_t id, uint64_t data);
void packet_enable(packet_inst_t * const packet_inst, packet_enable_t enable);


#endif /* PACKET_H_ */
