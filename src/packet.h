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
typedef enum pckt_id_err_t
{
	PCKT_ID_ERR_CHECKSUM = 0xFFFF,
	PCKT_ID_ERR_TIMEOUT  = 0xFFFE
} pckt_id_err_t;

/*Packet enable disable enum*/
typedef enum pckt_en_t
{
	PCKT_DISABLED,
	PCKT_ENABLED
} pckt_en_t;

/*Set up for CRC-16 (CRC-CCITT)*/
typedef uint16_t crc_t; //The width of the CRC calculation and result. Modify the typedef for a 16 or 32-bit CRC standard.
#define SW_CRC_POLYNOMIAL 0x1021
#define SW_CRC_WIDTH  (8 * sizeof(crc_t))
#define SW_CRC_TOPBIT (1 << (SW_CRC_WIDTH - 1))

/*Packet received struct*/
typedef struct pckt_rx_t
{
	uint16_t id;
	uint8_t len;
	uint8_t payload[MAX_PAYLOAD_LEN_BYTES];
	uint16_t crc_16_checksum;
} pckt_rx_t;

/*Packet configuration struct*/
typedef	struct pckt_conf_t
{
	const volatile uint32_t *tick_ptr;                        //pointer to sys tick
	int16_t (*rx_byte_fptr)(void);                            //function pointer for received byte return -1 for no data or >=0 for valid data
	void (*tx_data_fprt)(const uint8_t * const, uint32_t);    //function pointer for transmit, ptr to 8 bit data array and length
	uint16_t (*crc_16_fptr)(const uint8_t * const, uint32_t); //function pointer for crc-16, default will be sw_crc
	uint32_t clear_buffer_timeout;                            //timeout for buffer to be cleared when incomplete packet received
	pckt_en_t enable;                                   //enable or disable packet instance
} pckt_conf_t;

/*Packet instance struct*/
typedef struct pckt_inst_t
{
	pckt_conf_t conf;

	int16_t rx_byte;
	uint8_t rx_buffer[RX_BUFFER_LEN_BYTES];
	uint16_t rx_buffer_ind;
	uint16_t calc_crc_16_checksum;
	pckt_rx_t pckt_rx;
	uint32_t last_tick;
} pckt_inst_t;

/*Packet return value of rx functions*/
typedef enum pckt_rx_valid_t
{
	PCKT_INVALID_LEN = 0,
	PCKT_VALID_LEN   = 1
} pckt_rx_valid_t;


/**************************************************************************************************
*                                            PROTOTYPES
*************************************************^************************************************/
void     pckt_get_config_defaults(pckt_conf_t * const pckt_conf);
void     pckt_init               (pckt_inst_t * const pckt_inst, const pckt_conf_t pckt_conf);
void     pckt_task               (pckt_inst_t * const pckt_inst, void(*cmd_handler_fptr)(pckt_inst_t * const, const pckt_rx_t));
crc_t    pckt_sw_crc             (const uint8_t * const message, const uint32_t num_bytes);
void     pckt_tx_raw             (pckt_inst_t * const pckt_inst, const uint16_t id, const uint8_t * const data, const uint8_t len);

void     pckt_tx_u8              (pckt_inst_t * const pckt_inst, const uint16_t id, const uint8_t data);
void     pckt_tx_s8              (pckt_inst_t * const pckt_inst, const uint16_t id, const int8_t data);
void     pckt_tx_u16             (pckt_inst_t * const pckt_inst, const uint16_t id, const uint16_t data);
void     pckt_tx_s16             (pckt_inst_t * const pckt_inst, const uint16_t id, const int16_t data);
void     pckt_tx_u32             (pckt_inst_t * const pckt_inst, const uint16_t id, const uint32_t data);
void     pckt_tx_s32             (pckt_inst_t * const pckt_inst, const uint16_t id, const int32_t data);
void     pckt_tx_flt32           (pckt_inst_t * const pckt_inst, const uint16_t id, const float data);
void     pckt_tx_u64             (pckt_inst_t * const pckt_inst, const uint16_t id, const uint64_t data);
void     pckt_tx_s64             (pckt_inst_t * const pckt_inst, const uint16_t id, const int64_t data);
void     pckt_tx_dbl64           (pckt_inst_t * const pckt_inst, const uint16_t id, const double data);

void     pckt_enable             (pckt_inst_t * const pckt_inst, const pckt_en_t enable);

pckt_rx_valid_t pckt_rx_u8       (uint8_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_s8       (int8_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_u16      (uint16_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_s16      (int16_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_u32      (uint32_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_s32      (int32_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_flt32    (float * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_u64      (uint64_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_s64      (int64_t * const, const pckt_rx_t pckt_rx);
pckt_rx_valid_t pckt_rx_dbl64    (double * const, const pckt_rx_t pckt_rx);


#endif /* PACKET_H_ */
