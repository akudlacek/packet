/*
 * packet.c
 *
 * Created: 5/4/2018 3:52:08 PM
 *  Author: arin
 */ 


#include "packet.h"

/******************************************************************************
* Defines
******************************************************************************/
#define RX_BUFFER_LEN_BYTES (MAX_PAYLOAD_LEN_BYTES + 4)


/******************************************************************************
* Variables
******************************************************************************/
struct
{
	int8_t rx_byte;
	uint8_t rx_buffer[RX_BUFFER_LEN_BYTES];
	uint8_t rx_buffer_ind;
} packet;

/******************************************************************************
* Local Prototypes
******************************************************************************/
static int16_t default_rx_byte(void);
static void default_tx_data(uint8_t *data, uint32_t length);
static void default_command_handler(packet_rx_t rx);


/******************************************************************************
*  \brief Packet get config defaults
*
*  \note
******************************************************************************/
void packet_get_config_defaults(packet_conf_t *packet_conf)
{
	packet_conf->rx_byte_fptr     = default_rx_byte;
	packet_conf->tx_data_fprt     = default_tx_data;
	packet_conf->cmd_handler_fptr = default_command_handler;
}

/******************************************************************************
*  \brief Packet init
*
*  \note
******************************************************************************/
void packet_init(packet_inst_t *packet_inst, packet_conf_t packet_conf)
{
	packet_inst->conf.rx_byte_fptr     = packet_conf.rx_byte_fptr;
	packet_inst->conf.tx_data_fprt     = packet_conf.tx_data_fprt;
	packet_inst->conf.cmd_handler_fptr = packet_conf.cmd_handler_fptr;
}

/******************************************************************************
*  \brief Packet task
*
*  \note
******************************************************************************/
void packet_task(packet_inst_t *packet_inst)
{
	/*Get byte*/
	packet.rx_byte = packet_inst->conf.rx_byte_fptr();
	
	/*If data received*/
	if(packet.rx_byte != -1)
	{
		/*Add to buffer*/
		if(packet.rx_buffer_ind >= RX_BUFFER_LEN_BYTES)
		{
			/*Clear buffer*/
			packet.rx_buffer_ind = 0;
			//todo: buffer full error
		}
		else
		{
			/*Put in buffer*/
			packet.rx_buffer[packet.rx_buffer_ind] = (uint8_t)packet.rx_byte;
			packet.rx_buffer_ind++;
		}
		
		/*Check for valid packet - after ID and data length bytes received*/
		if(packet.rx_buffer_ind >= 2)
		{
			/*Check to see if data length is valid*/
			if(packet.rx_buffer[1] > MAX_PAYLOAD_LEN_BYTES)
			{
				//todo: data length too large error
			}
			
			/*Check data length to see if all bytes received - the +4 is ID, SIZE, and two CHECKSUM bytes*/
			else if((packet.rx_buffer[1] + 4) == packet.rx_buffer_ind)
			{
				
				/*Calculate checksum*/
				
			}
		}
	}
}

/******************************************************************************
*  \brief Software CRC
*
*  \note https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
******************************************************************************/
crc_t sw_crc(uint8_t const message[], int nBytes)
{
    crc_t remainder = 0;	

    /*Perform modulo-2 division, a byte at a time.*/
    for(int byte = 0; byte < nBytes; ++byte)
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

/**************************************************************************************************
*                                       LOCAL FUNCTIONS
**************************************************************************************************/
/******************************************************************************
*  \brief default rx byte function
*
*  \note
******************************************************************************/
static int16_t default_rx_byte(void)
{
	return -1;
}

/******************************************************************************
*  \brief default tx data function
*
*  \note
******************************************************************************/
static void default_tx_data(uint8_t *data, uint32_t length)
{
	//empty
}

/******************************************************************************
*  \brief default command handler
*
*  \note
******************************************************************************/
static void default_command_handler(packet_rx_t rx)
{
	//empty
}