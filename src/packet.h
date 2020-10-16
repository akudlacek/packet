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
/* If you want to override these symbols without modifying source code you will have to add a
 * symbol definition in the preprocessor settings.
 * In VS 2019        put DEFINE_OF_INTEREST=0 in [Configuration Properties->C/C++->Preprocessor->Preprocessor Definitions]
 * In Atmel Studio 7 put DEFINE_OF_INTEREST=0 in [Properties->ARM/GNU C Compiler->Symbols->Defined symbols (-D)]
 * In Code Blocks    put DEFINE_OF_INTEREST=0 in [Properties->Project's build options...->Compiler settings->#defines]
 */

#ifndef MAX_PAYLOAD_LEN_BYTES
#define MAX_PAYLOAD_LEN_BYTES 8 //max of 255
#endif

#define RX_BUFFER_LEN_BYTES (MAX_PAYLOAD_LEN_BYTES + 5) /*the +5 is [ID:0, ID:1][LEN][CRC16:0, CRC16:1]*/

//Packet error IDs, these are reserved IDs
typedef enum packet_id_err_t
{
	PCKT_ID_ERR_CHECKSUM = 0xFFFF,
	PCKT_ID_ERR_TIMEOUT  = 0xFFFE
} packet_id_err_t;

/*Packet enable disable enum*/
typedef enum packet_enable_t
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
typedef struct packet_rx_t
{
	uint16_t id;
	uint8_t len;
	uint8_t payload[MAX_PAYLOAD_LEN_BYTES];
	uint16_t crc_16_checksum;
} packet_rx_t;

/*Packet configuration struct*/
typedef	struct packet_conf_t
{
	const volatile uint32_t *tick_ptr;                        //pointer to sys tick
	int16_t (*rx_byte_fptr)(void);                            //function pointer for received byte return -1 for no data or >=0 for valid data
	void (*tx_data_fprt)(const uint8_t * const, uint32_t);    //function pointer for transmit, ptr to 8 bit data array and length
	uint16_t (*crc_16_fptr)(const uint8_t * const, uint32_t); //function pointer for crc-16, default will be sw_crc
	uint32_t clear_buffer_timeout;                            //timeout for buffer to be cleared when incomplete packet received
	packet_enable_t enable;                                   //enable or disable packet instance
} packet_conf_t;

/*Packet instance struct*/
typedef struct packet_inst_t
{
	packet_conf_t conf;

	int16_t rx_byte;
	uint8_t rx_buffer[RX_BUFFER_LEN_BYTES];
	uint16_t rx_buffer_ind;
	uint16_t calc_crc_16_checksum;
	packet_rx_t packet_rx;
	uint32_t last_tick;
} packet_inst_t;


/**************************************************************************************************
*                                            PROTOTYPES
*************************************************^************************************************/
void     packet_get_config_defaults(packet_conf_t * const packet_conf);
void     packet_init               (packet_inst_t * const packet_inst, const packet_conf_t packet_conf);
void     packet_task               (packet_inst_t * const packet_inst, void(*cmd_handler_fptr)(packet_inst_t * const, const packet_rx_t));
crc_t    sw_crc                    (const uint8_t * const message, const uint32_t num_bytes);
void     packet_tx_raw             (packet_inst_t * const packet_inst, const uint16_t id, const uint8_t * const data, const uint8_t len);
void     packet_tx_8               (packet_inst_t * const packet_inst, const uint16_t id, const uint8_t data);
void     packet_tx_16              (packet_inst_t * const packet_inst, const uint16_t id, const uint16_t data);
void     packet_tx_32              (packet_inst_t * const packet_inst, const uint16_t id, const uint32_t data);
void     packet_tx_64              (packet_inst_t * const packet_inst, const uint16_t id, const uint64_t data);
void     packet_tx_float_32        (packet_inst_t * const packet_inst, const uint16_t id, const float data);
void     packet_tx_double_64       (packet_inst_t * const packet_inst, const uint16_t id, const double data);
void     packet_enable             (packet_inst_t * const packet_inst, const packet_enable_t enable);
uint16_t packet_payload_uint16     (const packet_rx_t packet_rx);
int16_t  packet_payload_int16      (const packet_rx_t packet_rx);
uint32_t packet_payload_uint32     (const packet_rx_t packet_rx);
int32_t  packet_payload_int32      (const packet_rx_t packet_rx);
uint64_t packet_payload_uint64     (const packet_rx_t packet_rx);
int64_t  packet_payload_int64      (const packet_rx_t packet_rx);
float    packet_payload_float_32   (const packet_rx_t packet_rx);
double   packet_payload_double_64  (const packet_rx_t packet_rx);


#endif /* PACKET_H_ */
