/*
 * Rfid data type header
 */

/**
 * @defgroup RFID4
 * @brief
 * @{*/


#ifndef __RFID_DATA_TYPE_H__
#define __RFID_DATA_TYPE_H__

/*
*********************************************************************************************************
*                                               INCLUDES
*********************************************************************************************************
*/
#include "comdef.h"

/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/
/* MAIN <-- RFRD */
#define RFID_REVISION_RSP_ID		0x3000
#define RFID_CARD_INFO_RSP_ID		0x3001
#define RFID_FWUPDATE_RSP_ID		0x3002
#define RFID_POWER_CTL_RSP_ID		0x3003
#define RFID_CARD_INFO_EXT_RSP_ID	0x3004
#define RFID_OPERATION_EXT_RSP_ID	0x3006
#define RFID_FUELCARD_INFO_RSP_ID	0x3007
#define RFID_SOUND_OPM_RSP_ID		0x3009
#define RFID_LED_OPM_RSP_ID			0x300A
#define RFID_FWUPDATE_EMERGENCY_RSP_ID  0x300B

/* MAIN -> RFID */
#define RFID_LED_CTL_REQ_ID			0x4000
#define RFID_FWUPDATE_REQ_ID		0x4001
#define RFID_CRYPTO_KEY_REQ_ID		0x4002
#define RFID_POWER_CTL_REQ_ID		0x4003
#define RFID_REVISION_REQ_ID		0x4004
#define RFID_OPERATION_REQ_ID		0x4005
#define RFID_OPERATION_EXT_REQ_ID	0x4006  
#define RFRD_WAKEUP_REQ_ID			0x4007
#define RFID_CARD_INFO_ACK_ID		0x4008
#define RFID_SOUND_OPM_REQ_ID		0x4009
#define RFID_LED_OPM_REQ_ID 		0x400A

#define RFID_UID_BYTE_MAX			8
#define RFID_CARD_NUM_BYTE_MAX		8
#define RFID_TRANS_BYTE_MAX			52

#define RFID_RANDOM_BYTE_MAX		8  // JYP_130827
#define RFID_AUTH_BYTE_MAX			16  // JYP_130827

#define MIME_DATA_BYTE_MAX			32  // JYP_130827
#define RFID_AUTH_ID_BYTE_MAX		8  // JYP_130827
#define RFID_NFC_ARR_BYTE_MAX		64  // JYP_130827

typedef enum 
{
  RFID2_TYPE1_14443A = 0,
  RFID2_TYPE1_14443B,
  RFID2_TYPE1_15593,
  RFID2_TYPE1_NFC,
  RFID2_TYPE1_FELICA,
  
  RFID2_TYPE1_MAX_ENUM
}RFID2_CARD_TYPE1_Etype;
#define RFID_TYPE1_14443A       		(1<<RFID2_TYPE1_14443A)
#define RFID_TYPE1_14443B       		(1<<RFID2_TYPE1_14443B)
#define RFID_TYPE1_15693        		(1<<RFID2_TYPE1_15593)
#define RFID_TYPE1_NFC          			(1<<RFID2_TYPE1_NFC)      /* NOT USED */
#define RFID_TYPE1_FELICA       		(1<<RFID2_TYPE1_FELICA)

typedef enum 
{
  RFID2_TYPE2_UID = 0,
  RFID2_TYPE2_TMONEY,
  RFID2_TYPE2_CASHBEE,    
  RFID2_TYPE2_XCASH,    
  RFID2_TYPE2_MIFARE,    
  RFID2_TYPE2_NFC, //Reserved by RFID3
  RFID2_TYPE2_SMARTCARD,    
  RFID2_TYPE2_JAPAN_DRV_LIC,
  
  RFID2_TYPE2_MAX_ENUM
}RFID2_CARD_TYPE2_Etype;
#define RFID_TYPE2_UID           			(1<<RFID2_TYPE2_UID)
#define RFID_TYPE2_TMONEY        		(1<<RFID2_TYPE2_TMONEY)
#define RFID_TYPE2_CASHBEE       		(1<<RFID2_TYPE2_CASHBEE)
#define RFID_TYPE2_XCASH         		(1<<RFID2_TYPE2_XCASH)
#define RFID_TYPE2_MIFARE        		(1<<RFID2_TYPE2_MIFARE)
#define RFID_TYPE2_NFC           			(1<<RFID2_TYPE2_NFC)
#define RFID_TYPE2_SMARTCARD     		(1<<RFID2_TYPE2_SMARTCARD)
#define RFID_TYPE2_JAPAN_DRV_LIC	(1<<RFID2_TYPE2_JAPAN_DRV_LIC)


#define RFID_FW_UPDATE_SEGMENT  	(1024)  //(128) //sjhong 2022_0117

typedef enum
{
    RFID4_AUDIO_NULL0=0,
    RFID4_AUDIO_NULL1,
    RFID4_AUDIO_NULL2,
    RFID4_AUDIO_FUELCARD_REMOVED,    //ī�尡 �ܸ��⿡�� ���� �Ǿ����ϴ�.
    RFID4_AUDIO_FUELCARD_ERROR,         //ī�� ���Ի��¸� Ȯ���� �ּ���.
    RFID4_AUDIO_FUELCARD_INSERT,        //ī�带 �����Ͽ� �ּ���.
    RFID4_AUDIO_FUELCARD_OK,               //ī�尡 ���� �ν� �Ǿ����ϴ�.
    RFID4_AUDIO_WELCOME,                       //�ȳ��ϼ���. īī�� �����Ƽ�� �̿��� �ּ��� �����մϴ�. �����ϰ� ����� ���� �Ǽ���.
    RFID4_AUDIO_REFILL,                             //�⸧�� �����մϴ�. ������ �����Ҹ� �湮�Ͽ� �⸧�� �־� �ּ���.
    RFID4_AUDIO_BEFORE10,                       //�ݳ�10���� �Դϴ�. �ݳ��� �ʾ� �� ��� �߰� ����� �߻� �մϴ�.    

} rfid4_audio_guidance_enum_type ;
    
/*
*********************************************************************************************************
*                                           GLOBAL DATA TYPES
*********************************************************************************************************
*/
typedef PACKED struct
{
    uint8_t t1_14443a : 1;      /* LSB Bit */
    uint8_t t1_14443b : 1;
    uint8_t t1_15693   : 1;
    uint8_t t1_nfc    	    : 1;
    uint8_t t1_felica     : 1;
    uint8_t reserved     : 3;  
} PACKED_POST RFID_TYPE1_Stype;

typedef struct
{
    uint8_t t2_uid       	: 1;   /* LSB Bit */
    uint8_t t2_tmoney    	: 1;
    uint8_t t2_cashbee   	: 1;
    uint8_t t2_xcash     	: 1;
    uint8_t t2_mifare    	: 1;
    uint8_t t2_nfc       	: 1;
    uint8_t t2_smartcard : 1;
    uint8_t t2_japan_drv : 1;  
}RFID_TYPE2_Stype;

typedef enum
{
    eINSERT_OK = 0,		/* ���� �ν� ī�� */
    eINSERT_ERR,			/* ������ �ν� ī�� */
    eINSERT_EMPTY,		/* �� ī�� */
    eINSERT_CHECKING,		/* üũ ���� ī�� */

    eINSERT_STATUS_MAX
}RFID_FUELCARD_STATUS_Stype;

typedef enum
{
    UPDATE_NACK = 0,
    UPDATE_ACK,

    UPDATE_ENUM_MAX
}RFID_FWUPDATE_RSP_Etype;

typedef enum
{
    eSOUND_CMD_PLAY = 0,
    eSOUND_CMD_STOP,

    eSOUND_CMD_MAX
}RFID_SOUND_CMD_Stype;

/*-------------------------------------------------------------------------
    cmd : REVISION
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  dummy1;           /* reserved1 */
    uint8_t  dummy2;           /* reserved2 */ 
} PACKED_POST RFID_REVISION_REQ_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  fw_ver;			/* 10 -> 1.0 */
    uint16_t fw_ver_20;		/* svn �������� ���� ȹ���Ѵ� */
    uint8_t  boot_type;		/* ASCII Boot Type  */
    uint8_t  boot_ver;		/* 10 -> 1.0 */
    uint16_t usSoundVer;		/* Sound Data Version : 0~65535 */
    uint32_t uxSoundCrc32;	/* Sound Data CRC32 :  */

    uint8_t  card_slot_info;	 /* RFID_FUELCARD_STATUS_Stype */
    uint8_t  fuel_card_no [RFID_CARD_NUM_BYTE_MAX];	/* ���� ī�� ��ȣ�� �������� ���, �������� �ʰų� �������� �ʴ� ��� : 0 */
} PACKED_POST RFID_REVISION_RSP_Stype;

/*-------------------------------------------------------------------------
    cmd : FUELCARD_INFO
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  dummy1;           /* reserved1 */
    uint8_t  dummy2;           /* reserved2 */ 
} PACKED_POST RFID_FUELCARD_REQ_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  card_slot_info;	/* RFID_FUELCARD_STATUS_Stype */
    uint8_t  fuel_card_no [RFID_CARD_NUM_BYTE_MAX];	/* ���� ī�� ��ȣ�� �������� ���, �������� �ʰų� �������� �ʴ� ��� : 0 */
} PACKED_POST RFID_FUELCARD_RSP_Stype;

/*-------------------------------------------------------------------------
    cmd : OPERATION_EXT
-------------------------------------------------------------------------*/
typedef PACKED struct
{
    uint16_t id;

    uint8_t reserved1;             /* reserved1 */
    uint8_t reserved2;             /* reserved2 */
} PACKED_POST RFID_OPERATION_EXT_RSP_Stype;

typedef PACKED struct
{
    uint16_t id;

    uint16_t iso14443a_opm;		/* 14443A ���� ��� ���� */
    uint16_t iso14443b_opm;		/* 14443B ���� ��� ���� */
    uint16_t iso15693_opm;		/* 15693 ���� ��� ���� */
    uint16_t felica_opm;			/* FELICA���� ��� ���� */

    uint64_t card_mem_auth_id;	 /* Mifare Memory ����� ��� ���� Ű 8byte */
    uint8_t  card_mem_block_add;	/* Mifare Memory ����� ��� Read Block Address */

    uint8_t  period_sec;			/* rfid auto sleep,wakeup parameter : sleep/wakeup period */
    uint8_t  run_cnt;				/* rfid auto sleep,wakeup parameter : digiparts ��ǰ�� ontime�� �ƴ� rf���� Ƚ���� ����Ѵ�. */
} PACKED_POST RFID_OPERATION_REQ_Stype;

/*-------------------------------------------------------------------------
    cmd : CARD_INFO
-------------------------------------------------------------------------*/
typedef PACKED struct
{
    uint16_t id;

    uint8_t read_ok;			/* 1: read ok, 0: read fail */
    uint8_t card_type1;		/* ī�� �迭 ����, 14443a, 14443b, 15693 */
    uint8_t card_type2;		/* ī�� ���� ����, tmoney,cashbee,xcash,uid */
    uint8_t atqa[2];			/* 2byte ATQA�� */
    uint8_t sak;				/* 8Bit SAK �� */

    uint8_t uid_sz;			/* ��ȿ UID Byte Size */
    uint8_t uid_data[RFID_UID_BYTE_MAX];
    uint8_t card_num_sz;		/* ��ȿ Card Number Byte Size */
    uint8_t card_num_data[RFID_CARD_NUM_BYTE_MAX];
    uint8_t trans_sz;			 /* Trans information Byte Size */
    uint8_t trans_data[RFID_TRANS_BYTE_MAX];
} PACKED_POST RFID_CARD_INFO_RSP_Stype;

typedef PACKED struct
{
    uint16_t id;

    uint8_t read_ok;			/* 1: read ok, 0: read fail */
    uint8_t card_type1;		/* ī�� �迭 ����, 14443a, 14443b, 15693 */
    uint8_t card_type2;		/* ī�� ���� ����, tmoney,cashbee,xcash,uid */
    uint8_t atqa[2];			/* 2byte ATQA�� */
    uint8_t sak;				/* 8Bit SAK �� */

    uint8_t uid_sz;			/* ��ȿ UID Byte Size */
    uint8_t uid_data[RFID_UID_BYTE_MAX];
    uint8_t card_num_sz;		 /* ��ȿ Card Number Byte Size */
    uint8_t card_num_data[RFID_CARD_NUM_BYTE_MAX];
} PACKED_POST RFID_CARD_INFO_EXT_RSP_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint16_t usAckId;	/* ���� ���� message id */
    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_CARD_INFO_ACK_Stype;
/*-------------------------------------------------------------------------
    cmd : FWUPDATE
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint32_t bin_sz;			/* RFID FW Download binary size */
    uint32_t crc32;			/* RFID FW Download crc16 */
    uint16_t frame_segment;	/* frame data�κ��� size */  
    uint16_t frame_idx;		/* Frame Index 0���� �����ؼ� frame_num - 1 ����  */
    uint16_t frame_num;		/* �ִ� Frame ����.... bin_sz/128 + ������ �ø� */

    uint8_t  dummy1;		/* reserved1 */
    uint8_t  dummy2;		/* reserved2 */ 

    uint8_t  frame_data[RFID_FW_UPDATE_SEGMENT];	/* RFID FW Download binary data */
} PACKED_POST RFID_FWUPDATE_REQ_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  status;		/* 1: ACK, 0: NACK */
    uint16_t frame_idx;	/* ���信 �ش�Ǵ� frame index */  
    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_FWUPDATE_RSP_Stype;

typedef PACKED struct 
{
  uint16_t id;

  uint8_t  dummy1;           /* reserved1 */
  uint8_t  dummy2;           /* reserved2 */ 

} PACKED_POST RFID_FWUPDATE_EMERGENCY_RSP_Stype;
/*-------------------------------------------------------------------------
    cmd : LED_CTL
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  led_ctl;		/* 1: on, 0: off */
#if 0
    uint8_t  dummy1;           /* reserved1 */
    uint8_t  dummy2;           /* reserved2 */ 
#else
    uint8_t  ucOnBit;		/* reserved1 */
    uint8_t  ucOffBit;	/* reserved2 */ 
#endif
} PACKED_POST RFID_LED_CTL_REQ_Stype;

/*-------------------------------------------------------------------------
    cmd : POWER_CTL
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  pwr_ctl;		/* ���� power_on�� ����� ���� ���� 1: power_on, 0: power_off */
    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_POWER_CTL_RSP_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  pwr_ctl;		/* ���� power_on�� ����� ���� ���� 1: power_on, 0: power_off */
    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_POWER_CTL_REQ_Stype;
/*-------------------------------------------------------------------------
    cmd : SOUND_OPM
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  ucMode;		/* RFID_SOUND_CMD_Stype : Sound���� ����  */
    uint32_t usParam;	/* ucMode�� ���� Parameter */

    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_SOUND_OPM_REQ_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  dummy1;	/* reserved1 */
    uint8_t  dummy2;	/* reserved2 */ 
} PACKED_POST RFID_SOUND_OPM_RSP_Stype;
/*-------------------------------------------------------------------------
    cmd : LED_OPM
-------------------------------------------------------------------------*/
typedef PACKED struct 
{
    uint16_t id;

    uint8_t  ucAutoRunDisable;	/* RFID scan�� �����ϴ� led disable  */

    uint16_t usLockLedTime;		/* 1ms unit */
    uint16_t usUnLockLedTime;	/* 1ms unit */
    uint16_t usRunLedTime;		/* 1ms unit */  

    uint8_t  dummy1;			/* reserved1 */
    uint8_t  dummy2;			/* reserved2 */
    uint8_t  dummy3;			/* reserved3 */
} PACKED_POST RFID_LED_OPM_REQ_Stype;

typedef PACKED struct 
{
    uint16_t id;

    uint8_t  dummy1;           /* reserved1 */
    uint8_t  dummy2;           /* reserved2 */ 
} PACKED_POST RFID_LED_OPM_RSP_Stype;

/****************************************************************************************************************************/

typedef PACKED struct
{
    uint8_t uid_sz;
    uint8_t uid_data[RFID_UID_BYTE_MAX];
    uint8_t card_num_sz;
    uint8_t card_num_data [RFID_UID_BYTE_MAX];
} PACKED_POST RFID_UID_Stype;

#if defined(FEATURE_USES_FORCED_RFID_POWER)
#define RFID_FORCED_PWR_NONE            0x00
#define RFID_FORCED_PWR_ENABLE_MASK     0x80
#define RFID_FORCED_PWR_ALWAYS_ON       0x01
#define RFID_FORCED_PWR_ALWAYS_OFF      0x00
#endif /* FEATURE_USES_FORCED_RFID_POWER */

typedef PACKED struct
{
    uint16_t mask;
    uint8_t door_open_st;
    uint8_t door_unlock_st;
    uint8_t key_position_st;
    uint8_t fuel_lvl;
} PACKED_POST RFID_CAR_STATUS_Stype;

typedef PACKED struct
{
    bool flag_refill;
    bool flag_welcome_unlock;
    bool flag_welcome_open;
    bool flag_welcome;
    bool flag_welcome_timer;
} PACKED_POST RFID_AUDIO_Stype;

typedef PACKED struct {
    uint8_t OilCard_Status;			/* ����ī�尡 ������ 0, ������ 1, ���� 2 */
    uint8_t card_slot_info;			/* ī�忡�� ���޵� ���� ī�� ���� */
    uint8_t  RFID_BD_FW_Ver;   
    uint16_t RFID_BD_FW_Min_Ver;	/* svn versioin */
    uint8_t RFID_BOOT_Type;		/* RFID3+ISO7816 ���Ӿ��� ��� �߰�  RFRD HW ���п�, 'D':RFID3 'E':RFID3+ISO7816 */
    uint8_t fuel_card_no [RFID_CARD_NUM_BYTE_MAX];		/* ���� ī�� ��ȣ�� �������� ��� */
    uint8_t FORCED_PWR;
    uint8_t led_ctrl_mode;
    
    uint16_t usSoundVer;                        /* Sound Data Version : 0~65535 */
    uint32_t uxSoundCrc32;                   /* Sound Data CRC32 :  */

    RFID_CAR_STATUS_Stype CarStatus;
    RFID_AUDIO_Stype Audio;
    
} PACKED_POST RFID_INFO_Stype;

/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/
extern RFID_CARD_INFO_RSP_Stype RFID_CARD_INFO_RSP;
extern RFID_REVISION_RSP_Stype RFID_REVISION_RSP;


/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#endif	/* __RFID_DATA_TYPE_H__ */
