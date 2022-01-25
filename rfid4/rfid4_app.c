/* 
 * RFID Service
 */


/*---------------------------------------------------------------------------
    EDIT HISTORY FOR MODULE

when     who       what, where, why
-------- --------  ----------------------------------------------------------

---------------------------------------------------------------------------*/

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
#include "esp_console.h"

#include "driver/uart.h"
#include "gpio_drv.h"
#include "uart2_drv.h"

#include "common/fsmgr.h"
#include "common/dbmgr.h"
#include "common/ctlmgr.h"
#include "network/netmgr.h"
#include "network/pktmgr.h"
#include "cmdline.h"

#ifdef CONFIG_DIGIPARTS_RFID4_APPLICATION
#include "rfid4/rfid_data_type.h"
#include "rfid4/rfid4_app.h"
#include "rfid4/RFID4_Task.h"

/*---------------------------------------------------------------------------
    DEFINITIONS AND DECLARATION
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    VARIABLES
---------------------------------------------------------------------------*/
static const char *TAG = "rfid";

static RFID_ID_OPER_MODE_et RFID_oper_mode = RFID_ID_OPER_MEMBERSHIP_THEN_UID ;
static RFID_UID_Stype RFID_latest_data ;
RFID_FWUPDATE_REQ_Stype RFID_FWUPDATE_REQ;
RFID_STATE_PARAM_Stype  RFID_STATE_PARAM;
RFID_FWUPDATE_RSP_Stype RFID_FWUPDATE_RSP;
RFID_FW_UPDATE_STATUS_Etype RFID_FW_UPDATE_STATUS;
//RFID_REVISION_REQ_Stype     RFID_REVISION_REQ={0,};
RFID_OPERATION_REQ_Stype    RFID_OPERATION_REQ={0,};
RFID_CARD_INFO_RSP_Stype    RFID_CARD_INFO_RSP;
RFID_INFO_Stype RFID_INFO={0,};

static void RFID_DEG_FuelCardNum_Clear(void);
static void RFID_DEG_TX_LedOpm(int argc, char **argv);
static void RFID_DEG_TX_SoundOpm(int argc, char **argv);
static void RFID_TX_CardInfoACK(uint16_t ackId);
static void RFID_TX_ReqSoundOpm(uint8_t mode, uint32_t index);
static void RFID_TX_ReqLedOpm(uint8_t arun, uint16_t lock, uint16_t unlock, uint16_t run);
static void RFID_DEG_View(void);
static void RFID_TX_ReqRevision(void);
static void RFID_DEG_FW_Update(void);
static void RFID_DEG_BIN_Update(int argc, char **argv);

Cmd_stype RFID_CmdList[] =
{
    { 1,    "view",         NULL,         RFID_DEG_View,                 NULL },        
    { 2,    "cno",         "clr",         RFID_DEG_FuelCardNum_Clear,                 NULL },       
    { 2,    "tx",             "rev",          RFID_TX_ReqRevision,        NULL },
    { 2,    "up",            "fw",           RFID_DEG_FW_Update,      NULL },
    { 3,    "up",            "bin",          NULL,          RFID_DEG_BIN_Update },
    { 4,    "tx",            "snd",          NULL,          RFID_DEG_TX_SoundOpm },
    { 6,    "tx",            "led",          NULL,          RFID_DEG_TX_LedOpm },        

    /* End Flag */
    { 0, NULL,      NULL,     NULL,           NULL }
};

/*---------------------------------------------------------------------------
    MACRO
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    PROTOTYPE
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    FUNCTIONS
---------------------------------------------------------------------------*/

/*
*********************************************************************************************************
* Function       :  RFID_set_led_oper_mode
* Description	:	RFID의 LED 동작 모드를 설정
* Argument		:	none
* Returns			:	none
* Notes 			:	
*********************************************************************************************************
*/
void RFID_set_led_oper_mode(uint8_t  mode)
{
    RFID_INFO.led_ctrl_mode = mode ;
}

/*
*********************************************************************************************************
* Function       :  RFID_set_oper_mode
* Description	:	RFID의 ID Type를 설정
* Argument		:	none
* Returns			:	none
* Notes 			:	
*********************************************************************************************************
*/
void RFID_set_oper_mode(RFID_ID_OPER_MODE_et  mode)
{
    if ( mode < RFID_ID_OPER_MAX )
        RFID_oper_mode = mode ;
    else
        ESP_LOGE(TAG, "RFID ID configuration error(%d)\n", RFID_oper_mode) ;

    RFID_INFO.OilCard_Status = 0xFF ;
    RFID_INFO.card_slot_info = 0xFF ;
    RFID_INFO.led_ctrl_mode = 0 ;
}

/*
*********************************************************************************************************
* Function       :  RFID_get_operation_cfg
* Description	:	RFID 리더기의 operation 설정 데이터를 전달하기 위한 함수
* Argument		:	none
* Returns			:	리더기로 전달할 opearion config data의 pointer
* Notes 			:	
*********************************************************************************************************
*/
static rfid_oper_cfg_type*  RFID_get_operation_cfg ( void ) 
{
    return (&rfid_mgmt.file_cfg.rfid_cfg);
}

void rfid_config_FuelCardNum_str_set(const char *argv[])
{
    uint8_t i=0, j=0;
    int k=0;
    
    j = strnlen(argv[0], (RFID_CARD_NUM_BYTE_MAX*2));
    j = (j+1) /2 ;
    memset(rfid_mgmt.file_cfg.fuelcard.fuel_cardno, 0, RFID_CARD_NUM_BYTE_MAX);
    if(1)   //FEATURE_MASK_RFID_BIG_ENDIAN_FORMAT
    {
        for(i=0; i<j; i++)      // Big Endian
            rfid_mgmt.file_cfg.fuelcard.fuel_cardno[i] = (argv[0][i*2]-0x30)*16 + (argv[0][i*2+1]-0x30) ;
    }
    else
    {
        for(i=0, k = j-1; i<j; i++, k--)    //뒤집어야한다.(Little Endian)
            rfid_mgmt.file_cfg.fuelcard.fuel_cardno[k] = (argv[0][i*2]-0x30)*16 + (argv[0][i*2+1]-0x30) ;
    }

    rfid_config_write();
}

char *rfid_config_FuelCardNum_str_get(void)
{
    uint8_t i=0, j=RFID_CARD_NUM_BYTE_MAX;
    char buff[32]={0,};
    
    if(1)       //FEATURE_MASK_RFID_BIG_ENDIAN_FORMAT
    {
        for(i=0; i<j; i++) // Big Endian
              sprintf((void *)&buff[i*2],"%02X", rfid_mgmt.file_cfg.fuelcard.fuel_cardno[i]);
    }
    else
    {
        for(i=0; i<j; i++) //뒤집어야한다.(Little Endian)
              sprintf((void *)&buff[i*2],"%02X", rfid_mgmt.file_cfg.fuelcard.fuel_cardno[j-i-1]);
    }

    buff[i*2] = 0;  //Null add

    return buff;
}

static bool RFID_FuelCardInfo_Update(uint16_t rx_size, uint8_t slot_info, uint8_t *card_num)
{
    bool flag_store = false;

    if(RFID_INFO.card_slot_info == slot_info)       return flag_store;

    RFID_INFO.card_slot_info = slot_info ;
    
    switch(slot_info)
    {
        case eINSERT_OK: // Normal
            if(rx_size > 8) // 주유 카드 번호가 포함된 경우, Version 으로 확인하는 것이 더 좋다.
            {
                if(memcmp(RFID_INFO.fuel_card_no, card_num, RFID_CARD_NUM_BYTE_MAX))
                {
                    memset(RFID_INFO.fuel_card_no, 0, RFID_CARD_NUM_BYTE_MAX);
                    // nv에 저장된 값이 없다면 채우고...
                    if(!memcmp(&rfid_mgmt.file_cfg.fuelcard.fuel_cardno, RFID_INFO.fuel_card_no, RFID_CARD_NUM_BYTE_MAX) )
                    {
                        memcpy(&rfid_mgmt.file_cfg.fuelcard.fuel_cardno, card_num, RFID_CARD_NUM_BYTE_MAX ) ;
                        flag_store = true;
                    }
                    memcpy(RFID_INFO.fuel_card_no, card_num, RFID_CARD_NUM_BYTE_MAX ) ;
                    if(!memcmp(&rfid_mgmt.file_cfg.fuelcard.fuel_cardno, RFID_INFO.fuel_card_no, RFID_CARD_NUM_BYTE_MAX))
                    {
                        // 정상 
                        RFID_INFO.OilCard_Status = slot_info;
                    }
                    else
                    {   // mis matched                                    
                        if ( RFID_INFO.OilCard_Status != 4 )
                        {
                            RFID_INFO.OilCard_Status = 4;
                            // Report
                        }
                    }
                }
                else
                {
                    // 이전 번호랑 같다...
                }
            }
            else
            {   // 정상                             
                RFID_INFO.OilCard_Status = slot_info;
            }
        break ;

        case eINSERT_ERR: // un normal
            memset(RFID_INFO.fuel_card_no, 0, RFID_CARD_NUM_BYTE_MAX ) ;
            RFID_INFO.OilCard_Status = slot_info;
            //Report
        break ;

        case eINSERT_EMPTY: // Removed
            memset(RFID_INFO.fuel_card_no, 0, RFID_CARD_NUM_BYTE_MAX ) ;
            RFID_INFO.OilCard_Status = slot_info;
        break ;

        case eINSERT_CHECKING: // unknown card no
            memset(RFID_INFO.fuel_card_no, 0, RFID_CARD_NUM_BYTE_MAX ) ;
            RFID_INFO.OilCard_Status = slot_info;
            //Report
        break ;

        case 4: // mis-match card no- internal
        case 5: // reading card no..
        default:
        break ;
    }


    return flag_store;
}

void RFID_AUDIO_FuelCard_Status_Process(void)
{
    uint32_t index=0;
    
    if(RFID_INFO.OilCard_Status == eINSERT_OK)				index = RFID4_AUDIO_FUELCARD_OK;
    else if(RFID_INFO.OilCard_Status == eINSERT_ERR)                index = RFID4_AUDIO_FUELCARD_ERROR;
    else if(RFID_INFO.OilCard_Status == eINSERT_EMPTY)            index = RFID4_AUDIO_FUELCARD_REMOVED;
    else if(RFID_INFO.OilCard_Status == eINSERT_CHECKING)	index = RFID4_AUDIO_FUELCARD_ERROR;
    else if(RFID_INFO.OilCard_Status == 4)					index = RFID4_AUDIO_FUELCARD_ERROR;

    if(index) {
        RFID_TX_ReqSoundOpm(eSOUND_CMD_PLAY, index);
    }
}
void RFID_AUDIO_Welcome_Process(void)
{
    RFID_INFO.Audio.flag_welcome = true;
    RFID_TX_ReqSoundOpm(eSOUND_CMD_PLAY, RFID4_AUDIO_WELCOME);
}

void RFID_Car_Status_Process(void)
{
    rfid_command_t cmd;

    while (xQueueReceive(rfid_mgmt.que_hdl_cmd, &cmd, ( TickType_t ) 0) != pdFAIL)
    {
        switch (cmd.hdr.id)
        {
            case RFID_CMD_CAR_STATUS:
                RFID_INFO.CarStatus.mask = cmd.body.carsts.mask;
                RFID_INFO.CarStatus.door_open_st = cmd.body.carsts.door_open_st;
                RFID_INFO.CarStatus.door_unlock_st = cmd.body.carsts.door_unlock_st;
                RFID_INFO.CarStatus.key_position_st = cmd.body.carsts.key_position_st;
                RFID_INFO.CarStatus.fuel_lvl = cmd.body.carsts.fuel_lvl;

                // 주유 알림
                // 조건 : 예약자 사용중, 시동 상태, 연료 잔량 < 30%
                if((RFID_INFO.Audio.flag_refill == false)
                    && (RFID_INFO.CarStatus.fuel_lvl < 30) 
                    && (RFID_INFO.CarStatus.key_position_st > KEY_ACC_POS))
                {
                    RFID_INFO.Audio.flag_refill = true;
                    RFID_TX_ReqSoundOpm(eSOUND_CMD_PLAY, RFID4_AUDIO_REFILL);
                }
                else if(RFID_INFO.CarStatus.fuel_lvl > 45)
                {
                    RFID_INFO.Audio.flag_refill = false;
                }
                else if((RFID_INFO.Audio.flag_refill == true) && (RFID_INFO.CarStatus.key_position_st < KEY_ON_POS))    // 다음 시동시 다시 알려주기 위하여 
                {
                    RFID_INFO.Audio.flag_refill = false;
                }

                // 월컴 안내.
                // 조건 : 예약자 사용중, 도어 open후 5초
                if((RFID_INFO.Audio.flag_welcome_unlock == false) && (RFID_INFO.CarStatus.door_unlock_st)) {
                    RFID_INFO.Audio.flag_welcome_unlock = true;
                }
                if((RFID_INFO.Audio.flag_welcome_open == false) 
                    && (RFID_INFO.Audio.flag_welcome_unlock) 
                    && (RFID_INFO.CarStatus.door_open_st))
                {
                    RFID_INFO.Audio.flag_welcome_open = true;
                }
                if ((RFID_INFO.Audio.flag_welcome == false) 
                    && (RFID_INFO.Audio.flag_welcome_timer == false)
                    && (RFID_INFO.Audio.flag_welcome_unlock == true) 
                    && (RFID_INFO.Audio.flag_welcome_open == true))
                {
                    RFID_INFO.Audio.flag_welcome_timer = true;
                    (void) esp_timer_start_once(rfid_mgmt.tmr_hdl_welcome, CONV_SEC2US(RFID4_AUDIO_GUIDANCE_WELCOME_WAIT_TIME));
                }
            break;

            default:
            break;
        }

        if (cmd.hdr.hdl)
        {
            xEventGroupSetBits(cmd.hdr.hdl, cmd.hdr.evt);
        }
    }
}

/*
*********************************************************************************************************
*                                           RFID_RX_CmdExe()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_RX_CmdExe( uint8_t *data, uint16_t rx_size )
{
    uint16_t  id;
#ifdef FEATURE_RFID_DEBUG  
    uint8_t   i;
#endif

    RFID_REVISION_RSP_Stype *revision_rsp;
    RFID_FWUPDATE_RSP_Stype *fwupdate_rsp;   
    RFID_POWER_CTL_RSP_Stype *pwr_rsp;
    bool flag_store = false;
    
    id  = data[1];
    id <<= 8;
    id |= data[0];
  
//  scu_set_alarm(SRFID_ALM, FALSE);  
//  cbt_rfidok_timer_set(200);        /* 없애도 될거 같은데..*/  /* rfid ok response check every 20 seconds */
#if 0
    printf("E (%d) ", rx_size);
    for(uint16_t i=0; i<rx_size; i++)    printf("%02X ", data[i]);
    printf("\r\n");
#endif
    ESP_LOGI(TAG, "[RFID_RX_CmdEx] ID : 0x%04X", id);

    switch(id)
    {
        case RFID_REVISION_RSP_ID :
            revision_rsp = (RFID_REVISION_RSP_Stype*)(data);
            RFID_INFO.RFID_BOOT_Type = revision_rsp->boot_type; /* RFID3+ISO7816 라임아이 기능 추가 */
            // led_ctrl_mode 에 의해 LED 제어
            if(RFID_INFO.led_ctrl_mode)    {
                RFID_TX_LedCtl(ON);        
            }
            RFID_INFO.usSoundVer = revision_rsp->usSoundVer;
            RFID_INFO.uxSoundCrc32 = revision_rsp->uxSoundCrc32;
    #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n [REVISION_RSP] (%d) : fw:%d, boot:%c, boot:%d, slot:%d, FW : %d", rx_size, revision_rsp->fw_ver, revision_rsp->boot_type, revision_rsp->boot_ver, revision_rsp->card_slot_info, revision_rsp->fw_ver_20 );
            ESP_LOGI(TAG, "Snd Ver:%d / Snd Crc32:%d", revision_rsp->usSoundVer, revision_rsp->uxSoundCrc32);
            /* RFID3+ISO7816 라임아이 기능 추가 -> */
            if( RFID_INFO.RFID_BOOT_Type == RFID3_ISO7816_BOOT_TYPE )   
            {
                ESP_LOGI(TAG, "\n RFID_ContactCard_Number :");
                for( i = 0 ; i < 8; i++ )
                    ESP_LOGI(TAG, "%d%d",revision_rsp->fuel_card_no[i]>>4,revision_rsp->fuel_card_no[i]&0x0f); 
            }
            /* RFID3+ISO7816 라임아이 기능 추가 <- */
    #endif
            RFID_INFO.RFID_BD_FW_Ver = revision_rsp->fw_ver;
            flag_store = RFID_FuelCardInfo_Update(rx_size, revision_rsp->card_slot_info, revision_rsp->fuel_card_no);

            if ( rx_size > 6 ) // extended version 정보 포함한 경우.
                RFID_INFO.RFID_BD_FW_Min_Ver = revision_rsp->fw_ver_20;
            else
                RFID_INFO.RFID_BD_FW_Min_Ver = 0;
            memset((void *)&RFID_latest_data, 0, sizeof(RFID_UID_Stype) ) ;

        #if defined(FEATURE_RFID_OVER_DV10)
            RFID_OPM_Write(&RFID_OPERATION_REQ);
            rfid_hdlc16_uart_transmit((uint8_t *)(&RFID_OPERATION_REQ), sizeof( RFID_OPERATION_REQ_Stype));
        #endif
        break;

        case RFID_FUELCARD_INFO_RSP_ID:
        {
            bool flag_snd=false;
            RFID_FUELCARD_RSP_Stype *fuelcard_rsp = (RFID_FUELCARD_RSP_Stype*)(data);
             if(RFID_INFO.card_slot_info != fuelcard_rsp->card_slot_info)   flag_snd = true;
            flag_store = RFID_FuelCardInfo_Update(rx_size, fuelcard_rsp->card_slot_info, fuelcard_rsp->fuel_card_no);
            RFID_TX_CardInfoACK(id);
            if(flag_snd == true)    rfid_event_audio_fuelcard_status();
        }
        break;

        case RFID_CARD_INFO_RSP_ID :
            RFID_RX_CardInfo(data, rx_size);
        break;
      
        case RFID_FWUPDATE_RSP_ID :
            fwupdate_rsp = (RFID_FWUPDATE_RSP_Stype*)(data);
            RFID_FWUPDATE_RSP = *fwupdate_rsp;
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n RFID_RX_FWUPDATE : %d, %d", fwupdate_rsp->status, fwupdate_rsp->frame_idx );
        #endif
        break;

        case RFID_FWUPDATE_EMERGENCY_RSP_ID:
       {
            RFID_FWUPDATE_EMERGENCY_RSP_Stype*fwupdate_emergency_rsp= (RFID_FWUPDATE_EMERGENCY_RSP_Stype*)(data);
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n  [%04X] RFID_FWUPDATE_EMERGENCY_RSP_ID /rfrd_fw_update_flag(%d)", fwupdate_emergency_rsp->id, RFID_STATE_PARAM.rfrd_fw_update_flag);
        #endif
            if(RFID_STATE_PARAM.rfrd_fw_update_flag == false) {
                rfid4_fwupdate_byBoot();
            }
        }
        break;
      
        case RFID_POWER_CTL_RSP_ID :
            pwr_rsp = (RFID_POWER_CTL_RSP_Stype*)(data);      
            RFID_ING_FLAG = pwr_rsp->pwr_ctl;
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n [%04X] RFID_POWER_CTL_RSP_ID(%d)", pwr_rsp->id, pwr_rsp->pwr_ctl);
        #endif
        break;

        case RFID_OPERATION_EXT_RSP_ID:
        {
            RFID_OPERATION_EXT_RSP_Stype *opm_ext_rsp = (RFID_OPERATION_EXT_RSP_Stype*)(data);
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n [%04X] RFID_SOUND_OPM_RSP_ID", opm_ext_rsp->id);
        #endif
        }
        break;

        case RFID_SOUND_OPM_RSP_ID:
        {
            RFID_SOUND_OPM_RSP_Stype *snd_rsp = (RFID_SOUND_OPM_RSP_Stype*)(data);
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n [%04X] RFID_SOUND_OPM_RSP_ID", snd_rsp->id);
        #endif
        }
        break;
            
        case RFID_LED_OPM_RSP_ID:
        {
            RFID_LED_OPM_RSP_Stype *led_rsp = (RFID_LED_OPM_RSP_Stype*)(data);
        #ifdef FEATURE_RFID_DEBUG
            ESP_LOGI(TAG, "\n [%04X] RFID_LED_OPM_RSP_Stype", led_rsp->id);
        #endif
        }
        break;
      
        default :
        #ifdef FEATURE_RFID_DEBUG
        ESP_LOGI(TAG, "\n Warning!! RFID_RX_CMD Invalid [%04X]", id );
        #endif
        break;
    }

    if(flag_store) {
        rfid_config_write();
    }
}

/*
*********************************************************************************************************
*                                           RFID_RX_CardInfo()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_RX_CardInfo( uint8_t *data, uint16_t rx_size )
{
    uint32_t i = 0;
  
    RFID_CARD_INFO_RSP_Stype *card_rsp;
#if 0   //sjhong 2022_0112
    ds_cmd_type * cmd ;

    // JYP_130827
    if(rfid_busy == TRUE) 
    {
        ESP_LOGI(TAG, "\n rfid_busy.........");
        return;
    }
#endif

    card_rsp = (RFID_CARD_INFO_RSP_Stype*)(data);

#ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "\n RFID_RX_CardInfo : ok:%d, t1:0x%x, t2:0x%x, atqa:0x%x%x%x%x, sak:0x%x%x",  card_rsp->read_ok,card_rsp->card_type1, card_rsp->card_type2,
                                  card_rsp->atqa[0]>>4, card_rsp->atqa[0]&0x0f, card_rsp->atqa[1]>>4, card_rsp->atqa[1]&0x0f, 
                                  card_rsp->sak>>4, card_rsp->sak&0x0f);
    ESP_LOGI(TAG, ", UID(%d) = ", card_rsp->uid_sz ); for(i=0;i<card_rsp->uid_sz ;i++) ESP_LOGI(TAG, "%X%X", card_rsp->uid_data[i]>>4, card_rsp->uid_data[i]&0x0F);
    ESP_LOGI(TAG, ", NUM(%d) = ", card_rsp->card_num_sz); for(i=0;i<card_rsp->card_num_sz;i++) ESP_LOGI(TAG, "%X%X", card_rsp->card_num_data[i]>>4, card_rsp->card_num_data[i]&0x0F );        
    ESP_LOGI(TAG, "\rTRANS(%d) = ", card_rsp->trans_sz); for(i=0;i<card_rsp->trans_sz;i++) ESP_LOGI(TAG, "%02X", card_rsp->trans_data[i]);        
#endif

    if( card_rsp->read_ok == false )
    {
        ESP_LOGW(TAG, "Does not finished previous operation\n" ) ;
        return;
    }

    // Random UID를 고려하여 Card ID부터 비교.
    // Mifare Card의 경우 Card ID가 Null인 경우가 있어 이때에는 UID로 확인.
    if ( card_rsp->card_num_sz )
    {
        if ((RFID_latest_data.card_num_sz == card_rsp->card_num_sz) && memcmp((const void*)RFID_latest_data.card_num_data, (const void*)card_rsp->card_num_data, card_rsp->card_num_sz))
        {
            //ESP_LOGI(TAG, "\nSame Card\n" ) ;
            RFID_SetPeriodcount_ForValidCardTag() ;
            return ;
        }
    }
    else
    {
        if ((RFID_latest_data.uid_sz == card_rsp->uid_sz) && memcmp((const void*)RFID_latest_data.uid_data,(const void*)card_rsp->uid_data, card_rsp->uid_sz))
        {
            //ESP_LOGI(TAG, "\nSame Card\n" ) ;
            RFID_SetPeriodcount_ForValidCardTag() ;
            return ;
        }
    }

    if (RFID_INFO.led_ctrl_mode)
    {
        RFID_TX_LedCtl(OFF);        
    }
  
    i = 0 ;
    /*
    * 2013.08.26. by andrew for Add NFC Feature
    */
    if ( ( card_rsp->card_type1 & RFID_TYPE1_NFC ) )
    {
        // 카드 번호가 존재한다면 Card 번호를 넣어준다.
        if ( card_rsp->card_num_sz )
        {
#if 0   //sjhong 2022_0112
            cmd = get_ds_cmd_buf ( ) ;
            if( cmd )
            {
                cmd->hdr.cmd = DS_RFID_DETECT_CMD ;
                cmd->data.rfid.cmd_type = 0 ;
                cmd->data.rfid.sig = 0 ;
                cmd->data.rfid.id[i].type = RFID_ID_TYPE_SMARTKEY_ID ;
                cmd->data.rfid.id[i].len = card_rsp->card_num_sz;
                Mem_Copy ( (void *) cmd->data.rfid.id[i].rfid, (void *) card_rsp->card_num_data, card_rsp->card_num_sz );
            }
            else
            {
                ESP_LOGW(TAG, "DS Command queue full\n") ;
                return ;
            }
#endif
        }
        else
        {
            ESP_LOGI(TAG, "Invalid NFC Data\n") ;
            return ;
        }
    }
    else
    {
        /*
        * 2013.07.30. by Andrew
        * DSControlTask에서는 Card 번호나 UID만 필요하다.
        */ 
        switch ( RFID_oper_mode )
        {
            case RFID_ID_OPER_MEMBERSHIP_ONLY :
            case RFID_ID_OPER_MEMBERSHIP_AND_UID :
            // 위의 두 Type은 Card No가 없다면 굳이 보낼 필요가 없다.
            // ( ( card_rsp->card_type2 & RFID_TYPE2_TMONEY ) || ( card_rsp->card_type2 & RFID_TYPE2_CASHBEE ) || ( card_rsp->card_type2 & RFID_TYPE2_XCASH ) ) 역시 확인 필요 없다.
            // 만일 필요하다면 Structure Type2을 넣어줘서 DSControlTask에서 처리하자.
            if ( !card_rsp->card_num_sz )
            {
                return ;
            }
            case RFID_ID_OPER_MEMBERSHIP_THEN_UID :
            // 카드 번호가 존재한다면 Card 번호를 넣어준다.
            if ( card_rsp->card_num_sz )
            {
#if 0   //sjhong 2022_0112
                cmd = get_ds_cmd_buf ( ) ;
                if( cmd )
                {
                    cmd->hdr.cmd = DS_RFID_DETECT_CMD ;
                    cmd->data.rfid.cmd_type = 0 ;
                    cmd->data.rfid.sig = 0 ;
                    cmd->data.rfid.id[i].type = RFID_ID_TYPE_MEMBERSHIP ;
                    cmd->data.rfid.id[i].len = card_rsp->card_num_sz;
                    Mem_Copy ( (void *) cmd->data.rfid.id[i].rfid, (void *) card_rsp->card_num_data, card_rsp->card_num_sz );
                    cmd->data.rfid.trans.len = card_rsp->trans_sz;
                    if (card_rsp->trans_sz)
                        Mem_Copy ( (void *) cmd->data.rfid.trans.info, (void *) card_rsp->trans_data, card_rsp->trans_sz );
                    // RFID_ID_OPER_MEMBERSHIP_AND_UID 의 경우 는 UID도 넣어주어야 한다.
                    if ( RFID_oper_mode != RFID_ID_OPER_MEMBERSHIP_AND_UID )
                    {
                        // 2014.103.25. by andrew for 수집된 data는 packet으로 전달할 수 있도록 모든 data를 CS로 보내기 위해 break하지 않음.
                        //break ;
                    }
                    i ++ ;
                }
                else
                {
                    ESP_LOGW(TAG, "DS Command queue full") ;
                    return ;
                }
#endif
            }
            case RFID_ID_OPER_UID_ONLY :
            {
//            DiagPrintf("\rRFID_ID_OPER_UID_ONLY\n") ;
#if 0   //sjhong 2022_0112
                /* UID */
                // 위에서 command buffer를 받아오지 않은 경우 이다.
                if ( i == 0 )  cmd = get_ds_cmd_buf ( ) ;
                if( cmd )
                {
                    cmd->hdr.cmd = DS_RFID_DETECT_CMD ;
                    cmd->data.rfid.cmd_type = 0 ;
                    cmd->data.rfid.sig = 0 ;
                    cmd->data.rfid.id[i].type = RFID_ID_TYPE_UID ;
                    cmd->data.rfid.id[i].len = card_rsp->uid_sz ; // 18.11.07 by Andrew for rollback MIN(card_rsp->uid_sz, 4); // UID Length is Max 4.
                    Mem_Copy ( (void *) cmd->data.rfid.id[i].rfid, (void *) card_rsp->uid_data, card_rsp->uid_sz );
                }
                else
                {
                    ESP_LOGW(TAG, "DS Command queue full") ;
                    return ;
                }
#endif
            }
            break ;

            default :
                ESP_LOGE(TAG, "RFID Card Operate mode configure error(%d)\n", RFID_oper_mode) ;
            return ;
        }
    }

    // Random UID를 고려하여 Card ID 와 UID 별도로 저장
    memset((void *)&RFID_latest_data, 0, sizeof(RFID_UID_Stype) ) ;
    if ( card_rsp->card_num_sz )
    {
        RFID_latest_data.card_num_sz = card_rsp->card_num_sz ;
        memcpy((void *)RFID_latest_data.card_num_data, card_rsp->card_num_data, card_rsp->card_num_sz ) ;
    }
    if ( card_rsp->uid_sz )
    {
        RFID_latest_data.uid_sz = card_rsp->uid_sz ;
        memcpy((void *)RFID_latest_data.uid_data, card_rsp->uid_data, card_rsp->uid_sz ) ;
    }

#if 0   //sjhong 2022_0112
    ds_cmd (cmd ) ;
#endif
    RFID_TX_LedCtl(ON);        
    RFID_SetPeriodcount_ForValidCardTag() ;
}

/*
*********************************************************************************************************
*                                           RFID_TX_LedCtl()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_TX_LedCtl( OnOffState on_off )
{
    RFID_LED_CTL_REQ_Stype led_ctl;

    led_ctl.id = RFID_LED_CTL_REQ_ID;

    if(on_off == ON)    led_ctl.led_ctl = 1;
    else                        led_ctl.led_ctl = 0;

    rfid_hdlc16_uart_transmit((uint8_t *)(&led_ctl), sizeof(RFID_LED_CTL_REQ_Stype));

#ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "\n led ctl (%d)", on_off);
#endif
}

/*
*********************************************************************************************************
*                                           RFID_TX_ReqRevision()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void RFID_TX_ReqRevision(void)
{
    RFID_REVISION_REQ_Stype reqRevision={0,};

    reqRevision.id = RFID_REVISION_REQ_ID;

    rfid_hdlc16_uart_transmit((uint8_t *)(&reqRevision), sizeof(RFID_REVISION_REQ_Stype));

#ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "RFID_TX_ReqRevision");
#endif
}

/*
*********************************************************************************************************
*                                           RFID_TX_CardInfoACK()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void RFID_TX_CardInfoACK(uint16_t ackId)
{
    RFID_CARD_INFO_ACK_Stype reqAck={0,};

    reqAck.id = RFID_CARD_INFO_ACK_ID;
    reqAck.usAckId = ackId;

    rfid_hdlc16_uart_transmit((uint8_t *)(&reqAck), sizeof(RFID_CARD_INFO_ACK_Stype));

#ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "RFID_TX_CardInfoACK");
#endif
}

/*
*********************************************************************************************************
*                                           RFID_TX_ReqSoundOpm()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void RFID_TX_ReqSoundOpm(uint8_t mode, uint32_t index)
{
    RFID_SOUND_OPM_REQ_Stype sndopm={0,};

    sndopm.id = RFID_SOUND_OPM_REQ_ID;
    sndopm.ucMode = mode;
    sndopm.usParam = index;

    if(RFID_STATE_PARAM.rfrd_fw_update_flag == false) {
        rfid_hdlc16_uart_transmit((uint8_t *)(&sndopm), sizeof(RFID_SOUND_OPM_REQ_Stype));

#ifdef FEATURE_RFID_DEBUG
        ESP_LOGI(TAG, "RFID_TX_ReqSoundOpm(%d)(%d)", mode, index);
#endif
    }
}

/*
*********************************************************************************************************
*                                           RFID_TX_ReqSoundOpm()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void RFID_TX_ReqLedOpm(uint8_t arun, uint16_t lock, uint16_t unlock, uint16_t run)
{
    RFID_LED_OPM_REQ_Stype ledopm={0,};

    ledopm.id = RFID_LED_OPM_REQ_ID;
    ledopm.ucAutoRunDisable = arun;
    ledopm.usLockLedTime = lock;
    ledopm.usUnLockLedTime = unlock;
    ledopm.usRunLedTime = run;

    if(RFID_STATE_PARAM.rfrd_fw_update_flag == false) {
        rfid_hdlc16_uart_transmit((uint8_t *)(&ledopm), sizeof(RFID_LED_OPM_REQ_Stype));

#ifdef FEATURE_RFID_DEBUG
        ESP_LOGI(TAG, "RFID_TX_ReqLedOpm(%d)(%d)(%d)(%d)", arun, lock, unlock, run);
#endif
    }
}
    
/*
*********************************************************************************************************
*                                           RFID_PWR_Ctl()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_PWR_Ctl( OnOffState on_off )
{
#if 0   //sjhong 2022_0122
  GPIO_InitTypeDef GPIO_RFID_TXD;
  
  if( on_off == ON )
  {
#if defined(FEATURE_USES_FORCED_RFID_POWER)
    if ( RFID_INFO.FORCED_PWR & RFID_FORCED_PWR_ENABLE_MASK )
    {
      BSP_PwrCtrl ( PWR_RFID, (OnOffState) (RFID_INFO.FORCED_PWR & RFID_FORCED_PWR_ALWAYS_ON) );
    }
    else
#endif /* FEATURE_USES_FORCED_RFID_POWER */
    /* 파워 on 하고, GPIO을 원래 UART TX로 설정 */
    BSP_PwrCtrl(PWR_RFID, ON);

    #ifdef FEATURE_CORE_F2
    GPIO_RFID_TXD.GPIO_Pin   = GPIO_Pin_10;
    GPIO_RFID_TXD.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_RFID_TXD.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_RFID_TXD.GPIO_OType = GPIO_OType_PP;
    GPIO_RFID_TXD.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    #else
    GPIO_RFID_TXD.GPIO_Pin   = GPIO_Pin_10;
    GPIO_RFID_TXD.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_RFID_TXD.GPIO_Mode  = GPIO_Mode_AF_PP;
    #endif
    GPIO_ResetBits( GPIOC, GPIO_Pin_10 );
    GPIO_Init(GPIOC, &GPIO_RFID_TXD);
    //DiagPrintf("\n RFID_POWER ON ");
  }
  else
    {
      /* GPIO을 GPIO로 설정해서 LOW을 설정하고 파워 off */
      #ifdef FEATURE_CORE_F2
      GPIO_RFID_TXD.GPIO_Pin   = GPIO_Pin_10;
      GPIO_RFID_TXD.GPIO_Mode  = GPIO_Mode_OUT;
      GPIO_RFID_TXD.GPIO_Speed = GPIO_Speed_50MHz;
      GPIO_RFID_TXD.GPIO_OType = GPIO_OType_PP;
      GPIO_RFID_TXD.GPIO_PuPd  = GPIO_PuPd_NOPULL;
      #else
      GPIO_RFID_TXD.GPIO_Pin   = GPIO_Pin_10;
      GPIO_RFID_TXD.GPIO_Speed = GPIO_Speed_10MHz;
      GPIO_RFID_TXD.GPIO_Mode  = GPIO_Mode_Out_PP;
      #endif
      GPIO_Init(GPIOC, &GPIO_RFID_TXD);
      GPIO_ResetBits( GPIOC, GPIO_Pin_10 );
  
#if defined(FEATURE_USES_FORCED_RFID_POWER)
    if ( RFID_INFO.FORCED_PWR & RFID_FORCED_PWR_ENABLE_MASK )
    {
      BSP_PwrCtrl ( PWR_RFID, (OnOffState) (RFID_INFO.FORCED_PWR & RFID_FORCED_PWR_ALWAYS_ON) );
    }
    else
#endif /* FEATURE_USES_FORCED_RFID_POWER */
      BSP_PwrCtrl(PWR_RFID, OFF);
      //DiagPrintf("\n RFID_POWER OFF ");
    }
#endif
}

/*
*********************************************************************************************************
*                                           RFID_FW_BinaryCheck()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : SFLASH에 RFID FW가 들어 있는지 검사 한다.
*********************************************************************************************************
*/
bool RFID_FW_BinaryCheck( RFID_FWUPDATE_REQ_Stype *fw_req )
{
    fsmgr_command_t fscmd={0,};
    esp_err_t result = ESP_FAIL;
    uint32_t fsize=0;
    bool ret=false;
/*-------------------------------------------------------------------------*/

    /* read bin header */
    fscmd.hdr.id = FSMGR_CMD_UPDATE;
    fscmd.hdr.hdl = rfid_mgmt.evt_hdl;
    fscmd.hdr.evt = TASK_EVENT_DONE;
    fscmd.body.update.code = FSMGR_CMDUPDATE_CHECK;
//    snprintf(fscmd.body.update.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_FW_NAME);
    sprintf(fscmd.body.update.filename, RFID_STATE_PARAM.filename);    
    fscmd.body.update.length = FILE_BIN_HEADER_SIZE;
    fscmd.body.update.size = &fsize;
    fscmd.body.update.data = (uint8_t *) &rfid_mgmt.file_bin_header;
    fscmd.body.update.result = &result;
    (void) fsmgr_request(&fscmd);

    xEventGroupWaitBits(rfid_mgmt.evt_hdl, TASK_EVENT_DONE, pdTRUE, pdTRUE, TASK_COMMON_DELAY);

    if((result == ESP_OK) //&& (rfid_mgmt.file_bin_header.magic_data == FILE_RFID_FW_MAGIC_NUMBER)
        //&& (rfid_mgmt.file_bin_header.bin_length < FILE_RFID_FW_BIN_MAX_SIZE)
        && (rfid_mgmt.file_bin_header.bin_length == fsize))
    {
        ESP_LOGI(TAG, "%s[%d]\r\n"
                                    "- file size : %dByte\r\n"
                                    "- bin_crc32 : 0x%08X\r\n"
                                    "- bin_length : %d\r\n"
                                    "- magic_data : 0x%08X\r\n"
                                    "- bin_info : %d", 
                                    //FILE_RFID_FW_NAME, result,
                                    RFID_STATE_PARAM.filename, result,
                                    fsize,
                                    rfid_mgmt.file_bin_header.bin_crc32,
                                    rfid_mgmt.file_bin_header.bin_length,
                                    rfid_mgmt.file_bin_header.magic_data,
                                    rfid_mgmt.file_bin_header.bin_info);
        
        fw_req->id = RFID_FWUPDATE_REQ_ID;
        fw_req->bin_sz = rfid_mgmt.file_bin_header.bin_length;
        fw_req->crc32 = rfid_mgmt.file_bin_header.bin_crc32;
        fw_req->frame_segment = RFID_FW_UPDATE_SEGMENT;
        fw_req->frame_idx = 0;
        fw_req->frame_num = (fw_req->bin_sz / fw_req->frame_segment);
        if( fw_req->bin_sz % fw_req->frame_segment )
            fw_req->frame_num += 1;
    
        ret = true;
    }
    else
    {
    ret = false;
    }
    
    return ret;               
}

/*
*********************************************************************************************************
*                                           RFID_FW_UpdateInit()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_FW_UpdateInit( RFID_STATE_PARAM_Stype *rf_state )
{
    rf_state->v_update_retry_cnt = 0;
    rf_state->v_update_timeout_cnt = 0;

    rf_state->c_update_retry_cnt = RFID_UPDATE_RETRY_CNT_MAX;
    rf_state->c_update_timeout_cnt = RFID_UPDATE_TIME_OUT_MAX_ms / RFID_TASK_TICK_UNIT;

    rf_state->update_status = RFID_FW_UPDATE_STATUS_BUSY;
    
    rf_state->fd = NULL;
}

/*
*********************************************************************************************************
*                                           RFID_FW_UpdateBin()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_FW_UpdateBin( RFID_STATE_PARAM_Stype *rf_state, RFID_FWUPDATE_REQ_Stype *fw_req, RFID_FWUPDATE_RSP_Stype *fw_rsp )
{
  /* update중이 아니면 return */
  if( rf_state->rfrd_fw_update_flag == false )
    return;


  /* 맨 첫 frame check ******************************************************/
  if( fw_req->frame_idx == 0 )
  {
    if( fw_req->frame_idx == fw_rsp->frame_idx )      
    {
      if( fw_rsp->status == 1 )
      {
        /* binary size ok */
        rf_state->v_update_timeout_cnt = 0;
        fw_req->frame_idx++;
        RFID_FW_UpdatePacketTx(fw_req, fw_rsp, false);
      }else
        {
          /* binary size fail */
          RFID_FW_ExUpdateStatusWrite( rf_state, RFID_FW_UPDATE_STATUS_BINARY_SIZE_ERR );
          #ifdef FEATURE_RFID_DEBUG
          ESP_LOGI(TAG, "\n RFID_FW_UpdateBin : binary size err ");
          #endif
        }
    }
  }
  /**************************************************************************/

  /* 중간 frame check **************************************************/
  if( (fw_req->frame_idx > 0) && (fw_req->frame_idx < (fw_req->frame_num - 1 )) )
  {
    if( fw_req->frame_idx == fw_rsp->frame_idx )
    {
      /* 정상 ack 수신시 그 다음 packet 전송 */
      rf_state->v_update_timeout_cnt = 0;
      fw_req->frame_idx++;
      RFID_FW_UpdatePacketTx(fw_req, fw_rsp, false);
    }
  }
  /**************************************************************************/
  
  /* 맨 마지막 frame check **************************************************/
  if( fw_req->frame_idx == fw_req->frame_num -1 )
  {
    if( fw_req->frame_idx == fw_rsp->frame_idx )
    {
      if( fw_rsp->status == 1 )
      {
        /* verify ok */
        RFID_FW_ExUpdateStatusWrite( rf_state, RFID_FW_UPDATE_STATUS_OK );
        #ifdef FEATURE_RFID_DEBUG
        ESP_LOGI(TAG, "\n RFID_FW_UpdateBin : download ok ");
        #endif
        
      }else
        {
          /* verify fail */
          RFID_FW_ExUpdateStatusWrite( rf_state, RFID_FW_UPDATE_STATUS_CRC16_ERR );
          #ifdef FEATURE_RFID_DEBUG
          ESP_LOGI(TAG, "\n RFID_FW_UpdateBin : binary crc16 err ");
          #endif
        }
    }
  }
  /**************************************************************************/

  
  /* Time out 처리 **********************************************************/
  rf_state->v_update_timeout_cnt++;
  if( rf_state->v_update_timeout_cnt >= rf_state->c_update_timeout_cnt )  /* timeout  */
  {
    rf_state->v_update_timeout_cnt = 0;
    /* 재전송 */
    RFID_FW_UpdatePacketTx(fw_req, fw_rsp, true);

    rf_state->v_update_retry_cnt++;
  }
  /**************************************************************************/

  
  /* Retry 처리 *************************************************************/
  if( rf_state->v_update_retry_cnt >= rf_state->c_update_retry_cnt )
  {
    /* fail */
    RFID_FW_ExUpdateStatusWrite( rf_state, RFID_FW_UPDATE_STATUS_RETRY_ERR );
    #ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "\n RFID_FW_UpdateBin : retry err ");
    #endif

  }
  /**************************************************************************/


#if 0
  rf_state->v_update_timeout_cnt++;
  if( rf_state->v_update_timeout_cnt >= 10 )  /* test : 300ms tx */
  {
    rf_state->v_update_timeout_cnt = 0;
    RFID_FW_UpdatePacketTx( fw_req, fw_rsp );

    fw_req->frame_idx++;
    if( fw_req->frame_idx == fw_req->frame_num )
    {
      rf_state->rfrd_fw_update_flag = FALSE;
    }
  }
#endif

 
}

/*
*********************************************************************************************************
*                                           RFID_FW_UpdatePacketTx()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_FW_UpdatePacketTx( RFID_FWUPDATE_REQ_Stype *fw_req, RFID_FWUPDATE_RSP_Stype *fw_rsp, bool Retry)
{
    fsmgr_command_t fscmd={0,};
    esp_err_t result = ESP_FAIL;
    bool ret=false;
/*-------------------------------------------------------------------------*/
    if(Retry == false)
    {
        /* read bin header */
        fscmd.hdr.id = FSMGR_CMD_UPDATE;
        fscmd.hdr.hdl = rfid_mgmt.evt_hdl;
        fscmd.hdr.evt = TASK_EVENT_DONE;
        fscmd.body.update.code = FSMGR_CMDUPDATE_READ;
    //    snprintf(fscmd.body.update.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_FW_NAME);
        sprintf(fscmd.body.update.filename, RFID_STATE_PARAM.filename);    
#if 0
        if(fw_req->frame_idx == (fw_req->frame_num-1))
            fscmd.body.update.length = (int)(fw_req->bin_sz % RFID_FW_UPDATE_SEGMENT);
        else
            fscmd.body.update.length = RFID_FW_UPDATE_SEGMENT;
#else
        fscmd.body.update.length = fw_req->frame_segment;
#endif
        fscmd.body.update.data = (uint8_t *) &fw_req->frame_data;
        fscmd.body.update.result = &result;
        fscmd.body.update.fd = &RFID_STATE_PARAM.fd;
        fscmd.body.update.frame_idx = fw_req->frame_idx;
        fscmd.body.update.frame_num = fw_req->frame_num;
    //    ESP_LOGI(TAG, "[333] %p / %p", RFID_STATE_PARAM.fd, *fscmd.body.update.fd);
        (void) fsmgr_request(&fscmd);

        xEventGroupWaitBits(rfid_mgmt.evt_hdl, TASK_EVENT_DONE, pdTRUE, pdTRUE, TASK_COMMON_DELAY);

        if(result != ESP_OK) {
            ESP_LOGE(TAG, "Update Read Error!!![%d/%d]", fw_req->frame_idx, fw_req->frame_num);
        }
        else {
            ESP_LOGI(TAG, "RFID_FW_UpdateTx [%d/%d]", fw_req->frame_idx, fw_req->frame_num);
        }
    }
    
    /* 보내는 packet에 대한 응답을 미리 clear 해놓는다 */
    fw_rsp->frame_idx = 0xffff;
    fw_rsp->status = 0;

#if 0
    if(fw_req->frame_idx == (fw_req->frame_num-1))
        rfid_hdlc16_uart_transmit((uint8_t *)(fw_req), (sizeof(RFID_FWUPDATE_REQ_Stype)-(RFID_FW_UPDATE_SEGMENT-fscmd.body.update.length)));
    else
        rfid_hdlc16_uart_transmit((uint8_t *)(fw_req), sizeof(RFID_FWUPDATE_REQ_Stype));
#else
    rfid_hdlc16_uart_transmit((uint8_t *)(fw_req), sizeof(RFID_FWUPDATE_REQ_Stype));
#endif
}

/*
*********************************************************************************************************
*                                           RFID_FW_UpdateEnd()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_FW_UpdateEnd(RFID_STATE_PARAM_Stype *rf_state )
{
    /* rfid state init */
    /* rfid fw_update_flag clear */
    //RFID_StateInit( rf_state );

    /* rfid power on */
    RFID_PWR_Ctl( ON );
}

/*
*********************************************************************************************************
*                                           RFID_FW_ExUpdateStart()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
bool RFID_FW_ExUpdateStart( RFID_STATE_PARAM_Stype *rf_state )
{
    fsmgr_command_t fscmd={0,};
    esp_err_t result = ESP_FAIL;
//    FILE fd;
    
    /* SFLASH에 RFID용 FW가 제대로 들어가 있는지 검사 한다. */
    if(RFID_FW_BinaryCheck(&RFID_FWUPDATE_REQ) == false)
        return(false);  /* SFLASH에 RFID용 FW가 없다 */


    RFID_FW_UpdateInit(rf_state);

    /* open bin */
    fscmd.hdr.id = FSMGR_CMD_UPDATE;
    fscmd.hdr.hdl = rfid_mgmt.evt_hdl;
    fscmd.hdr.evt = TASK_EVENT_DONE;
    fscmd.body.update.code = FSMGR_CMDUPDATE_OPEN;
//    snprintf(fscmd.body.update.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_FW_NAME);
    sprintf(fscmd.body.update.filename, RFID_STATE_PARAM.filename);    
    fscmd.body.update.result = &result;
    fscmd.body.update.fd = &RFID_STATE_PARAM.fd;
    (void) fsmgr_request(&fscmd);

    xEventGroupWaitBits(rfid_mgmt.evt_hdl, TASK_EVENT_DONE, pdTRUE, pdTRUE, TASK_COMMON_DELAY);

    if((result == ESP_OK) && (fscmd.body.update.fd != NULL)) {
        ESP_LOGI(TAG, "Success to bin open [%s](%p)", fscmd.body.update.filename, rf_state->fd);
    }
    else {
        ESP_LOGI(TAG, "Failed to bin open [%s](%p)", fscmd.body.update.filename, rf_state->fd);
    }

    /* 첫 Packet 개시  */
    /* rfid에서 app에서 boot로 넘어가게 하기 위한 꼼수(rfid app은 응답 없음) */
    RFID_FW_UpdatePacketTx(&RFID_FWUPDATE_REQ, &RFID_FWUPDATE_RSP, false);  

    /* rfid power on/off seq stop!! */
    /* rfid fw_update_flag set */
    rf_state->rfrd_fw_update_flag = true;


    return(true);
}

/*
*********************************************************************************************************
*                                           RFID_FW_ExUpdateStatusWrite()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_FW_ExUpdateStatusWrite( RFID_STATE_PARAM_Stype *rf_state, RFID_FW_UPDATE_STATUS_Etype status  )
{
    fsmgr_command_t fscmd={0,};
    esp_err_t result = ESP_FAIL;
    
    rf_state->update_status = status;

    if( status != RFID_FW_UPDATE_STATUS_BUSY )
    {
        /* fw update stop */
        rf_state->rfrd_fw_update_flag = false;

        rfid4_looptimer_stop();

         /* close bin */
        fscmd.hdr.id = FSMGR_CMD_UPDATE;
        fscmd.hdr.hdl = rfid_mgmt.evt_hdl;
        fscmd.hdr.evt = TASK_EVENT_DONE;
        fscmd.body.update.code = FSMGR_CMDUPDATE_CLOSE;
//        snprintf(fscmd.body.update.filename, CONFIG_SPIFFS_OBJ_NAME_LEN, FILE_RFID_FW_NAME);
        sprintf(fscmd.body.update.filename, RFID_STATE_PARAM.filename);    
        fscmd.body.update.result = &result;
        fscmd.body.update.fd = &RFID_STATE_PARAM.fd;
        (void) fsmgr_request(&fscmd);

        xEventGroupWaitBits(rfid_mgmt.evt_hdl, TASK_EVENT_DONE, pdTRUE, pdTRUE, TASK_COMMON_DELAY);
    }
}

/*
*********************************************************************************************************
*                                           RFID_FW_ExUpdateStatusRead()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
RFID_FW_UPDATE_STATUS_Etype RFID_FW_ExUpdateStatusRead( RFID_STATE_PARAM_Stype *rf_state )
{
    return (rf_state->update_status );
}

/*
*********************************************************************************************************
* Function :RFID_FW_ExUpdateByOAP()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if 0
RFID_FW_UPDATE_STATUS_Etype RFID_FW_ExUpdateByOAP( void )
{
    RFID_FW_UPDATE_STATUS_Etype err = RFID_FW_UPDATE_STATUS_OK ;
    uint8_t busy_cnt = 0 ;
    if ( !RFID_FW_ExUpdateStart( &RFID_STATE_PARAM ) )
    {
        return ( RFID_FW_UPDATE_STATUS_CRC16_ERR ) ;
    }

    while(1)
    {
        busy_cnt ++ ;
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "\r\n RFID Status(%d)", RFID_FW_ExUpdateStatusRead( &RFID_STATE_PARAM ) );
        if( ( err = RFID_FW_ExUpdateStatusRead( &RFID_STATE_PARAM ) )  != RFID_FW_UPDATE_STATUS_BUSY ) 
        {
            return err ;
        }
        // busy count가 기준 이상이라면 return ;
        if ( busy_cnt > 15 ) return ( RFID_FW_UPDATE_STATUS_BUSY ) ;
    }
}
#endif
/*
*********************************************************************************************************
*                                           RFID_OPM_NfcArrWrite()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_OPM_NfcArrWrite( RFID_OPERATION_REQ_Stype *opm, uint8_t *buff, uint8_t sz )
{
#if 0
  uint32_t i;

  if( opm == NULL )
    return;

  if( sz > RFID_NFC_ARR_BYTE_MAX )
    return;
  
  for( i = 0 ; i < sz; i++ )
  {
    opm->nfc_arr_sz = sz;
    opm->nfc_arr_data[i] = buff[i];
  }
#endif  
}

/*
*********************************************************************************************************
*                                           RFID_OPM_MifareMemoryAuthWrite()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_OPM_MifareMemoryAuthWrite( RFID_OPERATION_REQ_Stype *opm, uint64_t auth, uint8_t block )
{
  opm->card_mem_auth_id = auth;
  opm->card_mem_block_add = block;
}

/*
*********************************************************************************************************
*                                           RFID_OPM_CardOpmWrite()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void RFID_OPM_CardOpmWrite( RFID_OPERATION_REQ_Stype *opm, uint16_t iso14443a, uint16_t iso14443b, uint16_t iso15693, uint16_t felica )
{
    opm->id = RFID_OPERATION_EXT_REQ_ID;

    opm->iso14443a_opm = iso14443a;
    opm->iso14443b_opm = iso14443b;
    opm->iso15693_opm  = iso15693;  
    opm->felica_opm       = felica;
}

/*
*********************************************************************************************************
*                                           RFID_OPM_Write()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
void RFID_OPM_Write( RFID_OPERATION_REQ_Stype *opm )
{
    rfid_oper_cfg_type *rfid_cfg ;

    rfid_cfg= RFID_get_operation_cfg() ;
    RFID_OPM_CardOpmWrite( opm, rfid_cfg->iso14443a_opm,  rfid_cfg->iso14443b_opm, rfid_cfg->iso15693_opm, rfid_cfg->felica_opm);
    RFID_OPM_MifareMemoryAuthWrite( opm, rfid_cfg->card_mem_auth_id, rfid_cfg->card_mem_block_add );
#ifdef FEATURE_RFID_DEBUG
    ESP_LOGI(TAG, "\n RFID_OPM_Write" );
#endif
}


/*
*********************************************************************************************************
* Function :register_cmd_rfid()
*
* Description : RFID Reader기와 관련된 Test Command set
*
* Argument(s) : 
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/
RFID_FW_UPDATE_STATUS_Etype RFID_BIN_ExUpdateByOAP(char *filename, bool isWaiting)
{
    RFID_FW_UPDATE_STATUS_Etype status = RFID_FW_UPDATE_STATUS_OK;
    uint8_t busy_cnt=0;

    if(RFID_STATE_PARAM.rfrd_fw_update_flag == true)    return (RFID_FW_UPDATE_STATUS_BUSY);

    memset(RFID_STATE_PARAM.filename, 0, sizeof(RFID_STATE_PARAM.filename));
    sprintf(RFID_STATE_PARAM.filename, filename);
    
    if(RFID_FW_ExUpdateStart(&RFID_STATE_PARAM) == false) {
        ESP_LOGI(TAG, "Failed to Update RFID FW!!!");
        return (RFID_FW_UPDATE_STATUS_BINARY_SIZE_ERR );
    }

    rfid4_looptimer_start();

    while(isWaiting)
    {   //update가 end 기다림 
        busy_cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
        status = RFID_FW_ExUpdateStatusRead(&RFID_STATE_PARAM);
        ESP_LOGI(TAG, "cnt(%d) update_status(%d)", busy_cnt, status);
        if(status != RFID_FW_UPDATE_STATUS_BUSY)      return status;

        // busy count가 기준 이상이라면 return ;
        //31282ms debug 존재 하고 약 76104byte일 경우
        //12818ms debug 없을 경우, 약 76104byte일 경우
        //121318ms debug 없을 경우, 약 418432byte일 경우 
        if(busy_cnt > 150)       return (RFID_FW_UPDATE_STATUS_BUSY) ;
    }

    return status;
}

static void RFID_DEG_FW_Update(void)
{
    RFID_BIN_ExUpdateByOAP(FILE_RFID_FW_NAME, true);
}

static void RFID_DEG_BIN_Update(int argc, char **argv)
{
    RFID_BIN_ExUpdateByOAP(argv[3], true);
}


static void RFID_DEG_TX_SoundOpm(int argc, char **argv)
{
    RFID_TX_ReqSoundOpm((uint8_t)atoi(argv[3]), (uint32_t)atoi(argv[4]));
}

static void RFID_DEG_TX_LedOpm(int argc, char **argv)
{
    RFID_TX_ReqLedOpm((uint8_t)atoi(argv[3]), (uint16_t)atoi(argv[4]), (uint16_t)atoi(argv[5]), (uint16_t)atoi(argv[6]));
}
static void RFID_DEG_FuelCardNum_Clear(void)
{
    memset(rfid_mgmt.file_cfg.fuelcard.fuel_cardno, 0, RFID_CARD_NUM_BYTE_MAX);
    rfid_config_write();
    
}
static void RFID_DEG_View(void)
{
    ESP_LOGI(TAG, "rfid_id_type = %d ", rfid_mgmt.file_cfg.rfid_cfg.rfid_id_type);
    ESP_LOGI(TAG, "iso14443a_opm = 0x%x ", rfid_mgmt.file_cfg.rfid_cfg.iso14443a_opm);
    ESP_LOGI(TAG, "iso14443b_opm = 0x%x ", rfid_mgmt.file_cfg.rfid_cfg.iso14443b_opm);
    ESP_LOGI(TAG, "iso15693_opm = 0x%x ", rfid_mgmt.file_cfg.rfid_cfg.iso15693_opm);
    ESP_LOGI(TAG, "felica_opm = %d ", rfid_mgmt.file_cfg.rfid_cfg.felica_opm);
    ESP_LOGI(TAG, "card_mem_auth_id = 0x%llx ", rfid_mgmt.file_cfg.rfid_cfg.card_mem_auth_id);
    ESP_LOGI(TAG, "card_mem_block_add = %d ", rfid_mgmt.file_cfg.rfid_cfg.card_mem_block_add);
    ESP_LOGI(TAG, "\n Fuel Card_Number : %02X%02X%02X%02X%02X%02X%02X%02X\r\n"
                , rfid_mgmt.file_cfg.fuelcard.fuel_cardno[0], rfid_mgmt.file_cfg.fuelcard.fuel_cardno[1]
                , rfid_mgmt.file_cfg.fuelcard.fuel_cardno[2], rfid_mgmt.file_cfg.fuelcard.fuel_cardno[3]
                , rfid_mgmt.file_cfg.fuelcard.fuel_cardno[4], rfid_mgmt.file_cfg.fuelcard.fuel_cardno[5]
                , rfid_mgmt.file_cfg.fuelcard.fuel_cardno[6], rfid_mgmt.file_cfg.fuelcard.fuel_cardno[7]);
}

static int rfid_cmd( int argc, char **argv )
{
  CmdRun( RFID_CmdList, argc, argv );

  return 0;
}

void register_cmd_rfid(void)
{
    const esp_console_cmd_t cmd = {
        .command = "rfid",
        .help = "RFID Command",
        .hint = NULL,
        .func = &rfid_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
#endif
