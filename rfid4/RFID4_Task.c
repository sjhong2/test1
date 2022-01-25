/* ============================================================================

  [ Module Name ]
  RFID4_Task.c

  [ Description ]


  Copyright (c) 2005 Digiparts, Inc. All Rights Reserved.

============================================================================ */


/* ============================================================================

  [ History ]

  when       who          what, where, why 
  ---------- ------------ ---------------------------------------------------
  2012/07/02 firepooh     Initial release


  2016/09/04 firepooh     라임아이 접촉시 주유카드 번호 읽기 기능 추가

  아래 TAG로 검색
  " RFID3+ISO7816 라임아이 기능 추가 "
  
  - CS는 RFID REVISION RSP의 BOOT TYPE을 보고 기존 RFID3인지  RFID3+ISO7816 인지 구분함.
  - 기존 RFID2,3 는 POWER ON/OFF을 반복적으로 하지만 RFID3+ISO7816은 항상 ON함.(ISO7816 CARD NUMBER을 빨리 가져오기 위해서)
  - RFID는 CS로부터 기존 RFID POWER OFF대신 POWER OFF COMMAND 받고, SLEEP 들어감.
 
      RFID_POWER_CTL_REQ.id = RFID_POWER_CTL_REQ_ID; 
      RFID_POWER_CTL_REQ.pwr_ctl = 0; 
      sHDLC16_TxRingBuff ( &RFID_RING_TX_BUFF, (INT8U *)(&RFID_POWER_CTL_REQ), sizeof( RFID_POWER_CTL_REQ_Stype ) );

  - RFID는 CS로부터 기존 RFID POWER ON 대신 아래 SYNC 명령어 받고 SLEEP WAKEUP 후 RESET 하여 신규 동작함.
 
    sRINGBUFF_ByteWrite( &RFID_RING_TX_BUFF, sHDLC16_FLAG_ASYNC);   RFID3+ISO7816 라임아이 기능 추가 , sleep wakeup 
    sRINGBUFF_ByteWrite( &RFID_RING_TX_BUFF, sHDLC16_FLAG_ASYNC);   RFID3+ISO7816 라임아이 기능 추가 , sleep wakeup 
 


============================================================================ */

/*
*********************************************************************************************************
*                                              INCLUDE FILES
*********************************************************************************************************
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_types.h"
#include "esp_event_base.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "gpio_drv.h"
#include "uart2_drv.h"

#include "common/fsmgr.h"
#include "common/dbmgr.h"
#include "common/ctlmgr.h"
#include "network/netmgr.h"
#include "network/pktmgr.h"

#include "sleep_drv.h"

#ifdef CONFIG_DIGIPARTS_RFID4_APPLICATION
#include "rfid4/rfid_data_type.h"
#include "rfid4/rfid4_app.h"
#include "rfid4/RFID4_Task.h"

/*---------------------------------------------------------------------------
    DEFINITIONS AND DECLARATION
---------------------------------------------------------------------------*/
#define RFID4_TASK_NAME			"rfid"
#define RFID4_TASK_STACK_SIZE	(4096)	// 1024*4
#define RFID4_TASK_DELAY		(portMAX_DELAY)
#define RFID4_TASK_PRIO			5
#define RFID4_CMD_QUEUE_SIZE	(10)

/*---------------------------------------------------------------------------
    VARIABLES
---------------------------------------------------------------------------*/
static const char *TAG = RFID4_TASK_NAME;

/* task management */
rfid_mgmt_t rfid_mgmt;

static hdlc16_type RFID_RX_HDLC_BUFF;


/*
*********************************************************************************************************
*                                               임시 생성 
*********************************************************************************************************
*/
#define MAX_CUSTOM_SUBTYPE      3

#if defined(FEATURE_TARGET_CSD)
const uint8_t default_type_admin_rfid[] = {0, 0, 0 }  ; /* 현 단말기가 20개의 Admin RFID 번호를 관리하는지 */
const uint16_t default_rfid_oper_time = ( (20 << 8 ) | ( 5)) ;
const rfid_oper_cfg_type default_rfid_cfg[MAX_CUSTOM_SUBTYPE] = {
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_TMONEY), 0, 250, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_TMONEY), 0, 250, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_TMONEY), 0, 250, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
};
#elif defined(FEATURE_TARGET_CSF)
const uint8_t default_type_admin_rfid[] = {0, 0, 1 }  ; /* 현 단말기가 20개의 Admin RFID 번호를 관리하는지 */
const uint16_t default_rfid_oper_time = ( (20 << 8 ) | ( 5)) ;
const rfid_oper_cfg_type default_rfid_cfg[MAX_CUSTOM_SUBTYPE] = {
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_MIFARE), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), 0, 250, 0x000034A1C006449B, 1, 2, 1, {0, 0, 1, 0, 0} },
  #if defined(FEATURE_CORE_F2) && defined(FEATURE_CUSTOMER_EVERON)
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_MIFARE), 0, 0, 250, 0x0000414F4350454B, 60, 2, 1, {0, 0, 1, 0, 0} },
  #else /* FEATURE_CORE_F2 */
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_MIFARE), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), 0, 250, 0x000034A1C006449B, 1, 2, 1, {0, 0, 1, 0, 0} },
  #endif /* FEATURE_CORE_F2 */
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_MIFARE), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), 0, 250, 0x000034A1C006449B, 1, 2, 1, {0, 0, 1, 0, 0} },
};
#elif defined(FEATURE_TARGET_CSC_ADV)
const uint8_t default_type_admin_rfid[] = {0, 0, 0 }  ; /* 현 단말기가 20개의 Admin RFID 번호를 관리하는지 */
const uint16_t default_rfid_oper_time = ( (20 << 8 ) | ( 5)) ;
const rfid_oper_cfg_type default_rfid_cfg[MAX_CUSTOM_SUBTYPE] = {
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
};
#elif defined(FEATURE_TARGET_K3)     //롯데렡날
const uint8_t default_type_admin_rfid[] = {0, 0, 0 }  ; /* 현 단말기가 20개의 Admin RFID 번호를 관리하는지 */
const uint16_t default_rfid_oper_time = ( (20 << 8 ) | ( 5)) ;
const rfid_oper_cfg_type default_rfid_cfg[MAX_CUSTOM_SUBTYPE] = {
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY|RFID_TYPE2_XCASH|RFID_TYPE2_MIFARE), 0, 0, 250, 0x000034A1C006449Bull, 1, 2, 1, {0, 0, 1, 0, 0} },
};
#elif defined(FEATURE_TARGET_CSA)   //소
const uint8_t default_type_admin_rfid[] = {1, 0, 0 }  ; /* 현 단말기가 20개의 Admin RFID 번호를 관리하는지 */
const uint16_t default_rfid_oper_time = ( (20 << 8 ) | ( 5)) ;
const rfid_oper_cfg_type default_rfid_cfg[MAX_CUSTOM_SUBTYPE] = {
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), RFID_TYPE2_UID, 0, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), RFID_TYPE2_UID, 0, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
	{1, RFID_ID_OPER_MEMBERSHIP_THEN_UID, (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), (RFID_TYPE2_UID|RFID_TYPE2_TMONEY), RFID_TYPE2_UID, 0, 0, 0, 2, 1, {0, 0, 1, 0, 0} },
};
#endif

#if defined(FEATURE_TARGET_K3) || defined(FEATURE_TARGET_CSD)
#define MAX_MULTI_RSV_RFID 2
//#define MAX_ADMIN_RFID 5 // 8, memory 확보후 8로 늘리도록 한다.
//#define MAX_NEW_ADMIN_RFID 10
#else
#define MAX_MULTI_RSV_RFID 1
//#define MAX_ADMIN_RFID 5
//#define MAX_NEW_ADMIN_RFID 10
#endif

#if defined(FEATURE_TARGET_K3)
#define MAX_RFID_UID_LEN        (RFID_DATA_LENTH_MAX*2+1)
#else
#define MAX_RFID_UID_LEN   ((RFID_DATA_LENTH_MAX*2)/MAX_MULTI_RSV_RFID)
#endif

#ifdef RELEASE
/*
 * Administrator RF ID UID or Card No. 
 * 모델별로 여러개의 설정이 가능하도록 변경함.
 +----------+----------------+----------------+----------------+
 |          |   0            |   1            |   2            |
 +----------+----------------+----------------+----------------+
 | CSD      | GreenCar       | None           | None           |
 +----------+----------------+----------------+----------------+
 | CSF(3.0) | KN             | EverOn/LH      | HyundaiCapital |
 | CSF      | KN             | LH             | HyundaiCapital |
 +----------+----------------+----------------+----------------+
 | CSC-ADV  | KT-Rental      | None           | None           |
 | K3       | KT-Rental      | None           | None           |
 +----------+----------------+----------------+----------------+
 | CSA      | SoCar          | None           | None           |
 +----------+----------------+----------------+----------------+
 */ 
const char  *str_admin_rfid[MAX_ADMIN_RFID+1][MAX_CUSTOM_SUBTYPE] = {
  #if defined(FEATURE_TARGET_CSD)
  { "E00401005841CCDA",                 "",                                 ""                              } ,
  { "D1872579",                         "",                                 ""                              } ,
  { "51C659D5",                         "",                                 ""                              } ,
  { "2F56162A",                         "",                                 ""                              } ,
  { "8B8260FE",                         "",                                 ""                              } ,
  #elif defined(FEATURE_TARGET_CSF)
    #if defined(FEATURE_CORE_F2) && defined(FEATURE_CUSTOMER_EVERON)
  { "2099140000000153",                 "1010000899046494",                 "8B8260FE"                      } ,
  { "2099140000000167",                 "",                                 ""                              } ,
  { "2099140000000184",                 "",                                 ""                              } ,
  { "2099140000000198",                 "",                                 ""                              } ,
  { "2099140000000200",                 "",                                 ""                              } ,
    #else /* FEATURE_CORE_F2 */
  { "2099140000000153",                 "1010001055551390",                 "8B8260FE"                      } ,
  { "2099140000000167",                 "",                                 ""                              } ,
  { "2099140000000184",                 "",                                 ""                              } ,
  { "2099140000000198",                 "",                                 ""                              } ,
  { "2099140000000200",                 "",                                 ""                              } ,
    #endif /* FEATURE_CORE_F2 && FEATURE_CUSTOMER_EVERON */
  #elif defined(FEATURE_TARGET_CSC_ADV)
  { "00ABAB00",                         "",                                 ""                              } ,
  { "00ABAB01",                         "",                                 ""                              } ,
  { "00ABAB02",                         "",                                 ""                              } ,
  { "00ABAB03",                         "",                                 ""                              } ,
  { "D1872579",                         "",                                 ""                              } ,
  #elif defined(FEATURE_TARGET_K3)
  { "00ABAB00",                         "",                                 ""                              } ,
  { "00ABAB01",                         "",                                 ""                              } ,
  { "00ABAB02",                         "",                                 ""                              } ,
  { "00ABAB03",                         "",                                 ""                              } ,
  { "D1872579",                         "",                                 ""                              } ,
  #else
  { "E00401005841CCDA",                 "",                                 ""                              } ,
  { "D1872579",                         "",                                 ""                              } ,
  { "51C659D5",                         "",                                 ""                              } ,
  { "2F56162A",                         "",                                 ""                              } ,
  { "2F56175A",                         "",                                 ""                              } ,
  #endif
  { (char*)0 }
} ;
#else
// Debug Mode
/*
 * Administrator RF ID UID or Card No. 
 */ 
const char  *str_admin_rfid[MAX_ADMIN_RFID+1][MAX_CUSTOM_SUBTYPE] = {
  { "E00401005841CCDA",                 "E00401005841CCDA",                 "E00401005841CCDA"} ,
  { "D1872579",                         "D1872579",                         "D1872579"} ,
  { "51C659D5",                         "51C659D5",                         "51C659D5"} ,
  { "2F56162A",                         "2F56162A",                         "2F56162A"} ,
  { "2F56175A",                         "2F56175A",                         "2F56175A"} ,
//  (char*)0
  { (char*)0 }
} ;
#endif

#define DEFAULT_RFID_TAG        2     /* 초단위 */

typedef enum
{
  RF_CTRL_DIRECT  = 0,    /* 장치에 의한 제어 */
  RF_CTRL_SMKEY   = 1,    /* 스마트키에 의한 제어 */
  RF_CTRL_ALL     = 2,    /* 스마트키+장치 제어 */
  RF_CTRL_MAX     = 0xFFFF
} rfid_control_type_e_type;

/*
*********************************************************************************************************
*                                              LOCAL DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
/* RFRD가 스마트폰과 통신중에 있으면 아래 FLAG을 보고 MAIN에서는 RFRD POWER OFF을 SKIP 할 수 있다.  */
bool RFID_ING_FLAG = false;           

static void rfid_direct_uart_transmit(uint8_t *data, uint16_t length);

/*
*********************************************************************************************************
*                                       LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

void RFID_SetPeriodcount_ForValidCardTag( void )
{
  // Valid한 Card를 Tagging하였다.
  // 1. LED를 1초간 동작시키기 위하여 Power duration을 1초만 남도록 조정한다.
  // 2. 다음 Power on까지의 주기를 최대 주기 * 2 로 설정한다.

  RFID_STATE_PARAM.v_pwr_period_cnt = (rfid_mgmt.file_cfg.rfid_tag_speed*1000) /RFID_TASK_TICK_UNIT * (-1) ;
  RFID_STATE_PARAM.v_pwr_on_duration_cnt = (RFID_STATE_PARAM.c_pwr_on_duration_cnt ) - (RFID_PWR_PERIOD_LED_ms/RFID_TASK_TICK_UNIT ) ;  
}

void RFID_StateInit( RFID_STATE_PARAM_Stype *rf_state )
{
  rf_state->rfrd_fw_update_flag = false;
  
  rf_state->v_pwr_period_cnt = 0;
  rf_state->v_pwr_on_duration_cnt = 0;  
  rf_state->v_pwr_off_retry_cnt   = 0; 

  rf_state->c_pwr_period_cnt = ((rfid_mgmt.file_cfg.rfid_oper_time >> 8) * 100) /RFID_TASK_TICK_UNIT;    /* 2000ms 임시 */
  rf_state->c_pwr_on_duration_cnt = ((rfid_mgmt.file_cfg.rfid_oper_time & 0x00FF ) * 100 ) /RFID_TASK_TICK_UNIT;    /* 500ms on time */
  rf_state->c_pwr_off_retry_cnt = 10;    /* 최소 토탈 Poweron 시간이 10sec 정도 되도록 설정한다 */

  /* error 상황이면 강제 설정한다 */
  if( rf_state->c_pwr_period_cnt > RFID_PWR_PERIOD_MAX_TIME_ms/RFID_TASK_TICK_UNIT )
  {
    rf_state->c_pwr_period_cnt = RFID_PWR_PERIOD_MAX_TIME_ms/RFID_TASK_TICK_UNIT;
    rf_state->c_pwr_on_duration_cnt = RFID_PWR_DURATION_DEFAULT_TIME_ms/RFID_TASK_TICK_UNIT;    
  }

  /* error 상황이면 강제 설정한다 */
  if( rf_state->c_pwr_on_duration_cnt > rf_state->c_pwr_period_cnt )
  {
    rf_state->c_pwr_period_cnt = RFID_PWR_PERIOD_DEFAULT_TIME_ms/RFID_TASK_TICK_UNIT;
    rf_state->c_pwr_on_duration_cnt = RFID_PWR_DURATION_DEFAULT_TIME_ms/RFID_TASK_TICK_UNIT;    
  }

  ESP_LOGI(TAG, "\n period_cnt : %d, duration_cnt : %d, nv_rf_tag_spd : %d", rf_state->c_pwr_period_cnt, rf_state->c_pwr_on_duration_cnt, rfid_mgmt.file_cfg.rfid_tag_speed );
}

#if 0
void RFID_StateUpdate( RFID_STATE_PARAM_Stype *rf_state )
{
    RFID_POWER_CTL_REQ_Stype RFID_POWER_CTL_REQ={0,};
    uint8_t buff[1];

    if( rf_state->rfrd_fw_update_flag == true )
        return;
  
    rf_state->v_pwr_period_cnt++;
    rf_state->v_pwr_on_duration_cnt++;  

//  if ( rf_state->v_pwr_period_cnt >= 2 ) BSP_RunLED_Off () ; // 50 ms 간 LED On
  
    if( rf_state->v_pwr_on_duration_cnt == rf_state->c_pwr_on_duration_cnt )
    {
        #if !defined(FEATURE_RFID_OVER_DV10)
    // POWER OFF Event 
//    RFID_PWR_Ctl( OFF );
        #else
        if( RFID_ING_FLAG == true )
        {
            rf_state->v_pwr_off_retry_cnt++;
            if( rf_state->v_pwr_off_retry_cnt > rf_state->c_pwr_off_retry_cnt )
            {
       /* 알수 없는 error로 power off delay 가 길어지면 그냥 off 해버린다 */
//       RFID_PWR_Ctl( OFF );
       //DiagPrintf("\n RFID_POWER_OFF DELAY Long -> ( POWER OFF )");
            }
            else
            {
                /* rfid가 동작중이므로 현재 power on duration cnt을 다시 0으로 만들어서 rfid power on 상태를 유지 한다 */
                rf_state->v_pwr_period_cnt = 0;
                rf_state->v_pwr_on_duration_cnt = 0;
                //DiagPrintf("\n RFID_POWER_OFF DELAY ");
            }
        }
        else
        {
//          if( RFID_INFO.RFID_BOOT_Type != RFID3_ISO7816_BOOT_TYPE )  /* RFID3+ISO7816 라임아이 기능 추가 */
//        RFID_PWR_Ctl( OFF );

            rf_state->v_pwr_off_retry_cnt = 0;
            //DiagPrintf("\n FFFFFFFFFFFFFFFF ");
            /* RFID3+ISO7816 라임아이 기능 추가(sleep 명령어를 tx 한다) -> */
            if( RFID_INFO.RFID_BOOT_Type == RFID3_ISO7816_BOOT_TYPE )
            {
                RFID_POWER_CTL_REQ.id = RFID_POWER_CTL_REQ_ID; 
                RFID_POWER_CTL_REQ.pwr_ctl = 0; 
                rfid_hdlc16_uart_transmit((uint8_t *)(&RFID_POWER_CTL_REQ), sizeof( RFID_POWER_CTL_REQ_Stype));
            }
            /* RFID3+ISO7816 라임아이 기능 추가(sleep 명령어를 tx 한다) <- */
        }
    #endif
    }

    if( rf_state->v_pwr_period_cnt == rf_state->c_pwr_period_cnt )
    {
    // POWER ON Event 
//    RFID_PWR_Ctl( ON );
    #if defined(FEATURE_RFID_OVER_DV10)
        RFID_ING_FLAG = false;
        rf_state->v_pwr_off_retry_cnt = 0;
        //DiagPrintf("\n OOOOOOOOOOOOOOOO ");
        /* RFID3+ISO7816 라임아이 기능 추가 , sleep wakeup -> */
//        if( RFID_INFO.RFID_BOOT_Type == RFID3_ISO7816_BOOT_TYPE )
        {
            buff[0] = FLAG_ASYNC;
            rfid_direct_uart_transmit(buff, 1);
            rfid_direct_uart_transmit(buff, 1);
        }
#if 0
        {
RFID_POWER_CTL_REQ.id = RFID_POWER_CTL_REQ_ID; 
                RFID_POWER_CTL_REQ.pwr_ctl = 0; 
                rfid_hdlc16_uart_transmit((uint8_t *)(&RFID_POWER_CTL_REQ), sizeof( RFID_POWER_CTL_REQ_Stype));}
#endif                
            
        /* RFID3+ISO7816 라임아이 기능 추가 , sleep wakeup <- */
    #endif
//    BSP_RunLED_On () ;

	/* 2020.05.26 Clear rfid string buffer periodically */
    //if( digi_certi_opt_mode ) 
//        Mem_Clr ( (void *) rfid_buf, RFID_DATA_LENTH_MAX*2 );
    
        rf_state->v_pwr_period_cnt = 0;
        rf_state->v_pwr_on_duration_cnt = 0;
  }

}
#endif

/**
 * @brief rfid protocol parser
 */
static void rfid_hdlc16_parser(void)
{
    int i;
    //uint8_t data;
    uint8_t hdlc_state;
/*-------------------------------------------------------------------------*/
#ifdef FEATURE_RFID_UART2_DEBUG
//    ESP_LOGD(TAG, "..last %x", rfid_mgmt.buffer[rfid_mgmt.buffer_length - 1]);
    printf("R (%d) ", rfid_mgmt.buffer_length);
    for(uint16_t i=0; i<rfid_mgmt.buffer_length; i++)    printf("%02X ", rfid_mgmt.buffer[i]);
    printf("\r\n");
#endif

    for (i = 0; i < rfid_mgmt.buffer_length; i++)
    {
        RFID_RX_HDLC_BUFF.data = rfid_mgmt.buffer[i];
        
        /* Process HDLC */
        hdlc_state = HDLC16_async_rx(&RFID_RX_HDLC_BUFF);

        /* Check the result */
        if (hdlc_state == HDLC_RX_STILL_CONSTRUCT) {
            /* Still constructing HDLC packet ... */
            continue;
        }
        else if (hdlc_state == HDLC_RX_RECEIVED_VALID_PACKET) {
            RFID_RX_CmdExe((uint8_t *)&RFID_RX_HDLC_BUFF.read_buf, (RFID_RX_HDLC_BUFF.read_index-2)) ;
        }
       else if ( RFID_RX_HDLC_BUFF.read_index > 3)
      {
#ifdef FEATURE_OAP_TEST
      ESP_LOGD(TAG, "Invalid HDLC state[%d]\r", hdlc_state );
#endif
        }
       
        /* Reset HDLC */
        HDLC16_reset_async_rx_buf(&RFID_RX_HDLC_BUFF);
    }
/*-------------------------------------------------------------------------*/
} /* otos1_prot_parser() */

/**
 * @brief rfid uart data received callback
 */
static void rfid_uart_receive_callback(void)
{
    size_t length = 0;
/*-------------------------------------------------------------------------*/

    uart_get_buffered_data_len(UART_NUM_2, &length);
    length = MIN(rfid_mgmt.buffer_size, length);
    length = uart_read_bytes(UART_NUM_2, &rfid_mgmt.buffer[rfid_mgmt.buffer_length], length, portMAX_DELAY);
    rfid_mgmt.buffer_length += length;

    // RESERVED
    if (rfid_mgmt.buffer_length) {
        ESP_LOGI(TAG, "rcvd %d %d", length, rfid_mgmt.buffer_length);
        rfid_hdlc16_parser();
        rfid_mgmt.buffer_length = 0;
    }

/*-------------------------------------------------------------------------*/
} /* rfid_uart_receive_callback() */

/**
 * @brief rfid direct uart data transmit
 */
static void rfid_direct_uart_transmit(uint8_t *data, uint16_t length)
{
/*-------------------------------------------------------------------------*/
#ifdef FEATURE_RFID_UART2_DEBUG
//    ESP_LOGD(TAG, "xmit %d %x ..", length, data[0]);
    printf("T (%d) ", length);
    for(uint16_t i=0; i<length; i++)    printf("%02X ", data[i]);
    printf("\r\n");
#endif

    if(rfid_mgmt.uart.WriteBytes != NULL )
    {
        rfid_mgmt.uart.WriteBytes(data, length);
    }

/*-------------------------------------------------------------------------*/
} /* rfid_direct_uart_transmit() */

/**
 * @brief rfid hdlc16 uart data transmit <--> HDLC16_Direct_to_RingBuff
 */
void rfid_hdlc16_uart_transmit(uint8_t *src_byte, uint16_t src_len)
{
    uint8_t *frame;
    uint16_t i, length;
    uint8_t result;
    uint16_t crc;               /* The crc value */
    uint8_t crc_l, crc_h;   /* The crc bytes */
    uint8_t *tx_buffer;     /* The transmit buffer pointer */

    /*-------------------------------------------------------------------------*/

    length = 0;
    frame = rfid_mgmt.xmit;
    
    tx_buffer = src_byte;

    for(i=0; i<src_len; i++)
    {
        if(HDLC16_chk_byte( &result, *tx_buffer++)) {
            frame[length++] = ESC_ASYNC;
        }
        frame[length++] = result;
    }

    /* CRC seed */
    crc = HDLC_CRC_SEED;

    /***********************************************************************
    Calculate the tx crc.
    ***********************************************************************/
    tx_buffer = src_byte;

    for ( i = 0; i < src_len; i++ ) {
        crc = CRC_COMPUTE( crc, *tx_buffer++);
    }
    
    /***********************************************************************
    Split the crc value into two bytes ...
    **********************************************************************/
    crc  ^= 0xffff;
    crc_l =  (uint8_t)crc;
    crc_h =  (uint8_t)(crc >> 8);

    /******* Send the crc out **********/
    if( HDLC16_chk_byte( &result, crc_l)) {
        frame[length++] = ESC_ASYNC;
    }
    frame[length++] = result;

    if(HDLC16_chk_byte( &result, crc_h)) {
        frame[length++] = ESC_ASYNC;
    }
    frame[length++] = result;

    frame[length++] = FLAG_ASYNC;

    rfid_direct_uart_transmit(frame, length);

/*-------------------------------------------------------------------------*/
} /* rfid_hdlc16_uart_transmit() */

/**
 * @brief uart initialize
 */
static void rfid_uart_initialize(void)
{
/*-------------------------------------------------------------------------*/

    /* intialize serial interface */
    if (!rfid_mgmt.uart.bInited) {
        rfid_mgmt.uart.Baudrate = 115200;
        rfid_mgmt.uart.TxRingSize = 1024*2;
        rfid_mgmt.uart.RxRingSize = 1024*2;
        rfid_mgmt.uart.bPattenDetectEnable = false;
        rfid_mgmt.uart.uxSleepTimeoutMS = (60*1000);
        rfid_mgmt.uart.DATA_Callback = rfid_uart_receive_callback;
        if (!UART2_Init(&rfid_mgmt.uart)) {
            ESP_LOGE(TAG, "uart2 initialization failed!");
        }
    }

    if (rfid_mgmt.uart.bInited) {
        /* initialize buffer */
        rfid_mgmt.buffer_size = 4098;
        rfid_mgmt.buffer = calloc(1, rfid_mgmt.buffer_size);
        if (!rfid_mgmt.buffer) {
            ESP_LOGE(TAG, "calloc buffer failed!");
        }
    }

    ESP_LOGI(TAG, "uart init %d %x", 
        rfid_mgmt.uart.bInited, 
        (rfid_mgmt.buffer ? 1 : 0));

/*-------------------------------------------------------------------------*/
} /* rfid_uart_initialize() */


/**
 * @brief rfid write configuration
 */
void rfid_config_write(void)
{
    fsmgr_command_t fscmd;
/*-------------------------------------------------------------------------*/

    fscmd.hdr.id = FSMGR_CMD_WRITE;
    fscmd.hdr.hdl = NULL;
    fscmd.hdr.evt = TASK_EVENT_DONE;
    snprintf(fscmd.body.data.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_CFG_NAME);
    fscmd.body.data.length = FILE_RFID_CFG_SIZE;
    fscmd.body.data.data = (uint8_t *) &rfid_mgmt.file_cfg;
    fscmd.body.data.result = NULL;
    (void) fsmgr_request(&fscmd);

/*-------------------------------------------------------------------------*/
} /* rfid_config_write() */

/**
 * @brief rfid read configuration
 */
static void rfid_config_read(void)
{
    file_rfid_cfg_t *cfg = NULL;
    fsmgr_command_t fscmd;
    esp_err_t result = ESP_FAIL;
    uint8_t index=0;
/*-------------------------------------------------------------------------*/

    /* read configuration */
    fscmd.hdr.id = FSMGR_CMD_READ;
    fscmd.hdr.hdl = rfid_mgmt.evt_hdl;
    fscmd.hdr.evt = TASK_EVENT_DONE;
    snprintf(fscmd.body.data.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_CFG_NAME);
    fscmd.body.data.length = FILE_RFID_CFG_SIZE;
    fscmd.body.data.data = (uint8_t *) &rfid_mgmt.file_cfg;
    fscmd.body.data.result = &result;
    (void) fsmgr_request(&fscmd);

    xEventGroupWaitBits(rfid_mgmt.evt_hdl, TASK_EVENT_DONE, pdTRUE, pdTRUE, TASK_COMMON_DELAY);

    cfg = &rfid_mgmt.file_cfg;
    if (result == ESP_OK && cfg->magic_num == FILE_RFID_CFG_MAGIC_NUMBER)
    {
	ESP_LOGI(TAG, "%s [%d]\r\n"
            "-active:%d\r\n"
            "-rfid_id_type:%d\r\n"
            "-iso14443a_opm:%d\r\n"
            "-iso14443b_opm:%d\r\n"
            "-iso15693_opm:%d\r\n"
            "-felica_opm:%d\r\n"
            "-card_mem_auth_id:%llu\r\n"
            "-card_mem_block_add:%d\r\n"
            "-period_sec:%d\r\n"
            "-on_100ms:%d",
            FILE_RFID_CFG_NAME, result,
            cfg->rfid_cfg.active,
            cfg->rfid_cfg.rfid_id_type,
            cfg->rfid_cfg.iso14443a_opm,
            cfg->rfid_cfg.iso14443b_opm,
            cfg->rfid_cfg.iso15693_opm,
            cfg->rfid_cfg.felica_opm,
            cfg->rfid_cfg.card_mem_auth_id,
            cfg->rfid_cfg.card_mem_block_add,
            cfg->rfid_cfg.period_sec,
            cfg->rfid_cfg.on_100ms);
    
    }
    else
    {
        ESP_LOGW(TAG, "read cfg err=%x", result);

        /* default */
        (void) memset((void *) cfg, 0, FILE_RFID_CFG_SIZE);
        
        cfg->magic_num = FILE_RFID_CFG_MAGIC_NUMBER;
        //cfg->feature_mask = RFID_FEATURE_NONE;  //RFID_FEATURE_ENABLE_LOG;

	/* 관리자용 RFID NUM init*/
	cfg->type_admin_rfid = default_type_admin_rfid [index] ;
	for(uint8_t i=0; i<MAX_ADMIN_RFID; i++) {
		memset((void *)&cfg->admin_rfid.admin.admin_rfid[i][0], 0, MAX_RFID_UID_LEN);
	}
	for(uint8_t i=0; i<MAX_ADMIN_RFID; i++) { // default는 5만..
		if ( str_admin_rfid[i][index]) {
      			strcpy((void *)(cfg->type_admin_rfid?(&cfg->admin_rfid.admin.new_admin_rfid[i][0]):(&cfg->admin_rfid.admin.admin_rfid[i][0])), str_admin_rfid[i][index]);
   		 }
   		else	break ;
  	}
	cfg->rfid_tag_speed   = DEFAULT_RFID_TAG; //sec 단위.
	cfg->rfid_tag_control = RF_CTRL_SMKEY;  //RFID가 태깅될때  기본으로  스마트키 동작.

	/* RFID Reader의 동작 주기를 설정 */
	cfg->rfid_oper_time = default_rfid_oper_time;  
		
        /* 2014.04.09. by andrew for RFID configuration */
        memcpy( (void *)&cfg->rfid_cfg, (const void *)&default_rfid_cfg[index], sizeof(rfid_oper_cfg_type) ) ;

        cfg->rfid_cfg.iso14443a_opm = 0;
        cfg->rfid_cfg.iso14443b_opm = 0;
        cfg->rfid_cfg.iso15693_opm = 0;
        cfg->rfid_cfg.felica_opm = 0;

        /* write default */
        rfid_config_write();
    }

/*-------------------------------------------------------------------------*/
} /* ctlmgr_config_read() */

/**
 * @brief setup log
 */
static void rfid_log_setup(void)
{
/*-------------------------------------------------------------------------*/

    if (rfid_mgmt.file_cfg.feature_mask & RFID_FEATURE_ENABLE_LOG)
    {
        esp_log_level_set(TAG, ESP_LOG_INFO);
    }
    else
    {
        esp_log_level_set(TAG, ESP_LOG_NONE);
    }
    
#ifdef FEATURE_RFID_DEBUG
    esp_log_level_set(TAG, ESP_LOG_INFO);
#endif

/*-------------------------------------------------------------------------*/
} /* rfid_log_setup() */

/**
 * @brief task status timer callback
 */
static void rfid_tmrcb_sts(void *arg)
{
/*-------------------------------------------------------------------------*/

    xEventGroupSetBits(rfid_mgmt.evt_hdl, RFID_EVENT_TIMER_STATUS);

/*-------------------------------------------------------------------------*/
} /* rfid_tmrcb_sts() */

static void rfid_tmrcb_welcome(void *arg)
{
/*-------------------------------------------------------------------------*/

    xEventGroupSetBits(rfid_mgmt.evt_hdl, RFID_EVENT_AUDIO_WELCOME);

/*-------------------------------------------------------------------------*/
} /* rfid_tmrcb_welcome() */

void rfid4_fwupdate_byBoot(void)
{
    xEventGroupSetBits(rfid_mgmt.evt_hdl, RFID_EVENT_BOOT_UPDATE);
}
void rfid_event_audio_fuelcard_status(void)
{
    xEventGroupSetBits(rfid_mgmt.evt_hdl, RFID_EVENT_AUDIO_FUELCARD);
}

/**
 * @brief rfid app initialize
 */
static void rfid_app_initialize(void)
{
    /* 주유카드가 없는 경우를 위해서 Default OFF를 만든다. */
    RFID_INFO.OilCard_Status = 1;
    RFID_INFO.card_slot_info = 1 ;

#if defined(FEATURE_USES_FORCED_RFID_POWER)
    RFID_INFO.FORCED_PWR = RFID_FORCED_PWR_NONE;
#endif  /* FEATURE_USES_FORCED_RFID_POWER */

    // Oper mode 초기화.
    RFID_set_oper_mode( (RFID_ID_OPER_MODE_et) rfid_mgmt.file_cfg.rfid_cfg.rfid_id_type);

    // RFID STATE Parameter 초기화
    RFID_StateInit(&RFID_STATE_PARAM);

    // RX HDLC Parameter 초기화.
    HDLC16_reset_async_rx_buf( &RFID_RX_HDLC_BUFF );  /* 시작전 꼭 초기화 !! */

}

void rfid4_looptimer_start(void)
{
    (void) esp_timer_start_periodic(rfid_mgmt.tmr_hdl_sts, CONV_MS2US(RFID_TASK_TICK_UNIT));    //10ms
}

void rfid4_looptimer_stop(void)
{
    (void) esp_timer_stop(rfid_mgmt.tmr_hdl_sts);
}

esp_err_t rfid_request(
    rfid_command_t *cmd
)
{
    esp_err_t err = ESP_FAIL;
    BaseType_t result;
/*-------------------------------------------------------------------------*/

    result = xQueueSend(rfid_mgmt.que_hdl_cmd, (void *) cmd, (TickType_t) 10);
    if (result == pdPASS)
    {
        ESP_LOGD(TAG, "queue cmd success");
        xEventGroupSetBits(rfid_mgmt.evt_hdl, RFID_EVENT_CAR_STATUS);
        err = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "queue cmd error=%d", result);
    }

/*-------------------------------------------------------------------------*/
    return err;
} /* fsmgr_request() */

/**
 * @brief initialize rfid4 task
 * @param arg argument pointer passed through task
 */
static void rfid4_initialize(void *arg)
{
    /*-------------------------------------------------------------------------*/

    (void) memset((void *)&rfid_mgmt, 0, sizeof(rfid_mgmt_t));
    
    /*---------------------------------------------------------------------*/

    /* Create the event group */
    rfid_mgmt.evt_hdl = xEventGroupCreate();

    /* Create command queue */
    rfid_mgmt.que_hdl_cmd = xQueueCreate(RFID4_CMD_QUEUE_SIZE, sizeof(rfid_command_t));
    if (!rfid_mgmt.que_hdl_cmd) {
        ESP_LOGE(TAG, "fail to intialize command queue !");
    }

    /* Setup initial wait bits */
    rfid_mgmt.wait_bits = TASK_COMMON_EVENTS
        | RFID_EVENT_COMMAND
        | RFID_EVENT_BOOT_UPDATE
        | RFID_EVENT_TIMER_STATUS
        | RFID_EVENT_CAR_STATUS
        | RFID_EVENT_AUDIO_FUELCARD
        | RFID_EVENT_AUDIO_WELCOME;

    /*---------------------------------------------------------------------*/

    /* create timers */
    const esp_timer_create_args_t status_timer_args = {
        .callback = &rfid_tmrcb_sts,
        .name = "rfid-sts"
    };
    ESP_ERROR_CHECK(esp_timer_create(&status_timer_args, &rfid_mgmt.tmr_hdl_sts));

    const esp_timer_create_args_t welcome_timer_args = {
        .callback = &rfid_tmrcb_welcome,
        .name = "rfid-welcome"
    };
    ESP_ERROR_CHECK(esp_timer_create(&welcome_timer_args, &rfid_mgmt.tmr_hdl_welcome));
    /*---------------------------------------------------------------------*/

    rfid_config_read();

    /*---------------------------------------------------------------------*/

    rfid_log_setup();

    /*---------------------------------------------------------------------*/

    rfid_uart_initialize();

    /*---------------------------------------------------------------------*/

    rfid_app_initialize();

    /*---------------------------------------------------------------------*/    

    ESP_LOGD(TAG, "initialize done");

    /*---------------------------------------------------------------------*/
    
}

/**
 * @brief rfid4 task loop
 * @param arg argument pointer
 */
static void RFID4_Task(void *arg)
{
    EventBits_t evts;
    /*-------------------------------------------------------------------------*/

    /* initialize task management and misc */
    rfid4_initialize(arg);
    
    /*---------------------------------------------------------------------*/
    
    /* infinite task loop */
    while (true) 
    {
        evts = xEventGroupWaitBits(
        rfid_mgmt.evt_hdl,
        rfid_mgmt.wait_bits,
        pdTRUE,     // clear on exit
        pdFALSE,    // wait for all bits
        RFID4_TASK_DELAY);

    /*---------------------------------------------------------------------*/
         if (evts & RFID_EVENT_COMMAND)
        {
            evts &= ~RFID_EVENT_COMMAND;
        }

         if(evts & RFID_EVENT_BOOT_UPDATE)
         {
            RFID_BIN_ExUpdateByOAP(FILE_RFID_FW_NAME, false);
            evts &= ~RFID_EVENT_BOOT_UPDATE;
         }

        if(evts & RFID_EVENT_TIMER_STATUS)
        {
            //RFID_StateUpdate(&RFID_STATE_PARAM);
            RFID_FW_UpdateBin(&RFID_STATE_PARAM, &RFID_FWUPDATE_REQ, &RFID_FWUPDATE_RSP);
            evts &= ~RFID_EVENT_TIMER_STATUS;
        }

        if(evts & RFID_EVENT_CAR_STATUS)
        {
            RFID_Car_Status_Process();
            evts &= ~RFID_EVENT_CAR_STATUS;
        }
        
        if(evts & RFID_EVENT_AUDIO_FUELCARD)
        {
            RFID_AUDIO_FuelCard_Status_Process();
            evts &= ~RFID_EVENT_AUDIO_FUELCARD;
        }

        if(evts & RFID_EVENT_AUDIO_WELCOME)
        {
            RFID_AUDIO_Welcome_Process();
            evts &= ~RFID_EVENT_AUDIO_WELCOME;
        }
    }
}

/**
 * @brief  create RFID4 task
 */
void create_RFID4_task(void)
{
	/*-------------------------------------------------------------------------*/
	xTaskCreatePinnedToCore(
		RFID4_Task, 
		RFID4_TASK_NAME, 
		RFID4_TASK_STACK_SIZE, 
		NULL, 
		RFID4_TASK_PRIO, 
		NULL, 
		TASK_COREID_DEFAULT);
	/*-------------------------------------------------------------------------*/
} /* create_RFID4_task() */

#endif

