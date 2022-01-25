/*
 * Rfid4 App header
 */

/**
 * @defgroup RFID4
 * @brief
 * @{*/

#ifndef __RFID4_APP_H__
#define __RFID4_APP_H__

/*
*********************************************************************************************************
*                                               INCLUDES
*********************************************************************************************************
*/

#include "hdlc16.h"
#include "fms/vehicleinfo.h"

/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           GLOBAL DATA TYPES
*********************************************************************************************************
*/
//#define RFID_TASK_TICK_UNIT                     10

#define RFID_PWR_PERIOD_LED_ms          1000      /* 1sec */
#define RFID_PWR_PERIOD_MAX_TIME_ms          10000      /* 10sec */
#if defined(FEATURE_RFID_OVER_DV10)
#define RFID_PWR_PERIOD_DEFAULT_TIME_ms       1500      /* JYP_030905 1.5sec */
#define RFID_PWR_DURATION_DEFAULT_TIME_ms      1000      /* JYP_030905 1sec */
#else
#define RFID_PWR_PERIOD_DEFAULT_TIME_ms       2000      /* 2sec */
//  #if defined(FEATURE_TARGET_CSF)
//#define RFID_PWR_DURATION_DEFAULT_TIME_ms      1900      /* 1900ms */
//  #else
#define RFID_PWR_DURATION_DEFAULT_TIME_ms      500      /* 500ms */
//  #endif
#endif


/* Audio Play timer */
#define RFID4_AUDIO_GUIDANCE_WELCOME_WAIT_TIME  3    //3sec


/* 토탈 30회 RETRY Cnt, Timeout은 100ms */
#define RFID_UPDATE_RETRY_CNT_MAX               30
#define RFID_UPDATE_TIME_OUT_MAX_ms          3500   //2500   //1500   //300   //100    //sjhong 2022_0117 pkt size 128 -> 1024

/* 
   'D' 는 기존 RFID3, 
   'E' 는 RFID3+ISO7816
*/
#define RFID3_BOOT_TYPE                       'D'
#define RFID3_ISO7816_BOOT_TYPE               'E'

typedef enum 
{
  RFID_FW_UPDATE_STATUS_BUSY = 0,  
  RFID_FW_UPDATE_STATUS_OK,
  RFID_FW_UPDATE_STATUS_BINARY_SIZE_ERR,
  RFID_FW_UPDATE_STATUS_CRC16_ERR,
  RFID_FW_UPDATE_STATUS_RETRY_ERR,
  
  RFID_FW_UPDATE_STATUS_MAX
}RFID_FW_UPDATE_STATUS_Etype;


typedef struct
{
  bool rfrd_fw_update_flag;      /* 1이면 rfrd fw update 중 */
  bool rfid_fw_update_rsp_rx;    /* fw update중 rfid로부터 rsp가 왔는지 check, 1: rx ok */
  
  uint16_t v_pwr_period_cnt;          /* 전체 주기 period tick cnt */
  uint16_t v_pwr_on_duration_cnt;    /* power on duration tick cnt */
  uint16_t v_pwr_off_retry_cnt;       /* CSC-ADV에서 Power off retry cnt */

  uint16_t v_update_retry_cnt;        /* fw update시 timeout에 의한 retry 횟수 */
  uint16_t v_update_timeout_cnt;      /* fw update시 timeout을 체크하기 위한 tick cnt  */

  uint16_t c_pwr_period_cnt;          /* 전체 주기 period tick cnt 비교값 */
  uint16_t c_pwr_on_duration_cnt;    /* power on duration tick cnt 비교값 */
  uint16_t c_pwr_off_retry_cnt;       /* CSC-ADV에서 Power off retry cnt 비교값 */

  uint16_t c_update_retry_cnt;        /* fw update시 timeout에 의한 retry 최대 횟수 */
  uint16_t c_update_timeout_cnt;      /* fw update시 timeout을 체크하기 위한 tick cnt  */

  RFID_FW_UPDATE_STATUS_Etype update_status;

  FILE *fd;
  char filename[32];
}RFID_STATE_PARAM_Stype;


typedef enum {
  RFID_ID_TYPE_NONE = 0,
  RFID_ID_TYPE_UID,
  RFID_ID_TYPE_MEMBERSHIP,
  RFID_ID_TYPE_SMARTKEY_ID,
  RFID_ID_TYPE_VIRTUAL_ID,
  RFID_ID_TYPE_TKP_ID,
}RFID_ID_TYPE_et;

typedef enum {
  RFID_ID_OPER_UID_ONLY = 0,
  RFID_ID_OPER_MEMBERSHIP_ONLY,
  RFID_ID_OPER_MEMBERSHIP_THEN_UID,
  RFID_ID_OPER_MEMBERSHIP_AND_UID,
  
  RFID_ID_OPER_MAX,
}RFID_ID_OPER_MODE_et;

typedef enum {
 RFID_EXIST = 0,  
 RFID_UID_Read = 1,
 RFID_MEM_Read,
 RFID_MEM_Write
}RFID_Cmd_et;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/
extern      RFID_FWUPDATE_REQ_Stype RFID_FWUPDATE_REQ;
extern      RFID_STATE_PARAM_Stype  RFID_STATE_PARAM;
extern      RFID_FWUPDATE_RSP_Stype RFID_FWUPDATE_RSP;
extern      RFID_INFO_Stype RFID_INFO;


/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

extern void RFID_Car_Status_Process(void);
extern void RFID_AUDIO_Welcome_Process(void);
extern void RFID_AUDIO_FuelCard_Status_Process(void);

extern      void      RFID_set_oper_mode(RFID_ID_OPER_MODE_et  mode) ;
extern      void      RFID_RX_CmdExe( uint8_t *data, uint16_t rx_size );
extern      void      RFID_RX_CardInfo( uint8_t *data, uint16_t rx_size );
extern      void      RFID_TX_LedCtl( OnOffState on_off );
extern      void      RFID_PWR_Ctl( OnOffState on_off );

extern void rfid_config_FuelCardNum_str_set(const char *argv[]);
extern char *rfid_config_FuelCardNum_str_get(void);
  
extern      bool   RFID_FW_BinaryCheck( RFID_FWUPDATE_REQ_Stype *fw_req );
extern      void      RFID_FW_UpdateInit( RFID_STATE_PARAM_Stype *rf_state );
extern      void      RFID_FW_UpdateBin( RFID_STATE_PARAM_Stype *rf_state, RFID_FWUPDATE_REQ_Stype *fw_req, RFID_FWUPDATE_RSP_Stype *fw_rsp );
extern      void      RFID_FW_UpdatePacketTx( RFID_FWUPDATE_REQ_Stype *fw_req, RFID_FWUPDATE_RSP_Stype *fw_rsp, bool Retry);

extern      bool   RFID_FW_ExUpdateStart( RFID_STATE_PARAM_Stype *rf_state );
extern      void      RFID_FW_ExUpdateStatusWrite( RFID_STATE_PARAM_Stype *rf_state, RFID_FW_UPDATE_STATUS_Etype status  );
extern      RFID_FW_UPDATE_STATUS_Etype RFID_FW_ExUpdateStatusRead( RFID_STATE_PARAM_Stype *rf_state );

//extern RFID_FW_UPDATE_STATUS_Etype RFID_FW_ExUpdateByOAP( void ) ;
extern RFID_FW_UPDATE_STATUS_Etype RFID_BIN_ExUpdateByOAP(char *filename, bool isWaiting);

extern      void      RFID_OPM_Write( RFID_OPERATION_REQ_Stype *opm );
  
extern void RFID_set_led_oper_mode(uint8_t  mode) ;

extern void register_cmd_rfid(void);

#endif	/* __RFID4_APP_H__ */