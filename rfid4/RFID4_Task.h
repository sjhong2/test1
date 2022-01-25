/*
 * Rfid4 task header
 */

/**
 * @defgroup RFID4
 * @brief
 * @{*/


#ifndef __RFID4_TASK_H__
#define __RFID4_TASK_H__

//#define FEATURE_RFID_DEBUG

#include "comdef.h"
#include "uart2_drv.h"
#include "common/fsmgr.h"


/*-------------------------------------------------------------------------*/
#define FEATURE_RFID_DEBUG        1
#define FEATURE_RFID_UART2_DEBUG    1
/*-------------------------------------------------------------------------*/

#define FEATURE_TARGET_CSF
#define FEATURE_CORE_F2
#define FEATURE_RFID_OVER_DV10

/**
 * @brief Packet manager events
 */
typedef enum {
    RFID_EVENT_NONE = 0,

    // BIT0 ~ BIT3 reserved

    // BIT4 ~ BIT7
    RFID_EVENT_COMMAND = BIT4,
    RFID_EVENT_BOOT_UPDATE = BIT5,
    
    // BIT8 ~ BIT11
    RFID_EVENT_TIMER_STATUS = BIT8,
    RFID_EVENT_CAR_STATUS = BIT9,
    
    // BIT12 ~ BIT15
    RFID_EVENT_AUDIO_FUELCARD = BIT12,
    RFID_EVENT_AUDIO_WELCOME = BIT13,

    RFID_EVENT_ENUM_PAD = 0xffffffff
} rfid4_event_bit_t;

/*-------------------------------------------------------------------------*/

typedef enum {
    RFID_FEATURE_NONE              = 0x0,

//    RFID_FEATURE_ENABLE_SERVICE    = BIT0,

    RFID_FEATURE_ENABLE_LOG        = BIT24,

    RFID_FEATURE_ENUM_PAD          = 0xffffffff
} rfid_feature_bit_t;

/*-------------------------------------------------------------------------*/
typedef enum {
    RFID_CMD_CAR_STATUS = 0,

    RFID_CMD_MAX
} rfidcmd_name_t;

typedef struct {
    uint8_t id;
    EventGroupHandle_t hdl;
    EventBits_t evt;
} rfidcmd_header_t;

typedef struct {
    uint8_t code;
    uint8_t param;
    uint32_t param2;
} rfidcmd_cfg_t;

typedef struct {
    uint16_t mask;
    uint8_t door_open_st;
    uint8_t door_unlock_st;
    uint8_t key_position_st;
    uint8_t fuel_lvl;
} rfidcmd_carsts_t;
/**
 * @brief rfid task command
 */
typedef struct {
    rfidcmd_header_t hdr;
    union {
        uint8_t param1;
        uint32_t param2;
        rfidcmd_cfg_t cfg;
        rfidcmd_carsts_t carsts;
    } body;
} rfid_command_t;

/*-------------------------------------------------------------------------*/

/**
 * @brief RFID4 task internal objects
 */

#define RFID_HDLC_FRAME_MAX                 (2048)  //(384)

typedef struct {
    EventGroupHandle_t evt_hdl;
    QueueHandle_t que_hdl_cmd;
    EventBits_t wait_bits;

    esp_timer_handle_t tmr_hdl_sts;
    esp_timer_handle_t tmr_hdl_welcome;
    
    
    file_rfid_cfg_t file_cfg;

    xUART2_Driver_stype uart;
    int buffer_size;
    uint8_t *buffer;
    uint16_t buffer_length;

    uint8_t xmit[RFID_HDLC_FRAME_MAX];

    file_bin_header_t file_bin_header;

} rfid_mgmt_t;


/*
*********************************************************************************************************
*                                               INCLUDES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/
#define RFID_TASK_TICK_UNIT     10

/*
*********************************************************************************************************
*                                           GLOBAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern rfid_mgmt_t rfid_mgmt;
extern bool RFID_ING_FLAG;

/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/
esp_err_t rfid_request(rfid_command_t *cmd);
void rfid_config_write(void);
void rfid4_fwupdate_byBoot(void);
void rfid_event_audio_fuelcard_status(void);
void rfid4_looptimer_start(void);
void rfid4_looptimer_stop(void);
void RFID_SetPeriodcount_ForValidCardTag( void ) ;
void create_RFID4_task(void);
void rfid_hdlc16_uart_transmit(uint8_t *src_byte, uint16_t src_len);



#endif	/* __RFID4_TASK_H__ */
