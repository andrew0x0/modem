/**
 * AT Command.
 */

#ifndef __AT_COMMAND_H__
#define __AT_COMMAND_H__

#include <stdint.h>

#include "error.h"

/** 'Create Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_CREATE_SOCKET              "+USOCR"

/** Length of 'Create Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_CREATE_SOCKET_LEN          (6)

/** 'Connect Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_CONNECT_SOCKET             "+USOCO"

/** 'Read Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_READ_SOCKET                "+USORD"

/** Length of 'Read Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_READ_SOCKET_LEN            (6)

/** 'Write Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_WRITE_SOCKET               "+USOWR"

/** Length of 'Connect Socket' AT command */
#define ATC_COMMAND_VERBOSE_TCPUDPIP_CONNECT_SOCKET_LEN         (6)

/** 'Packet Switched Network-Assigned Data' AT command */
#define ATC_COMMAND_VERBOSE_PSD_NETWORK_ASSIGNED_DATA           "+UPSND"

/** Length of 'Packet Switched Network-Assigned Data' AT command */
#define ATC_COMMAND_VERBOSE_PSD_NETWORK_ASSIGNED_DATA_LEN       (6)

/** 'Packet Switched Data Configuration' AT command */
#define ATC_COMMAND_VERBOSE_PSD_CONFIGURATION                   "+UPSD"

/** Length of 'Packet Switched Network-Assigned Data' AT command */
#define ATC_COMMAND_VERBOSE_PSD_CONFIGURATION_LEN               (5)

/** 'Packet Switched Data Action' AT command */
#define ATC_COMMAND_VERBOSE_PSD_ACTION                          "+UPSDA"

/** Length of 'Packet Switched Data Action' AT command */
#define ATC_COMMAND_VERBOSE_PSD_ACTION_LEN                      (6)

/** 'V24 Control and V25ter' AT command */
#define ATC_COMMAND_VERBOSE_V24CV25TER_ENABLE_ECHO              "E1"

/** Length of 'V24 Control and V25ter' AT command */
#define ATC_COMMAND_VERBOSE_V24CV25TER_ENABLE_ECHO_LEN          (2)

/** 'AT' AT command */
#define ATC_COMMAND_VERBOSE_AT                                  "AT"

/** Length of 'AT' AT command */
#define ATC_COMMAND_VERBOSE_AT_LEN                              (2)

/** AT command separator */
#define ATC_SEPARATOR                                           "\r\n"

/** Length of AT command separator */
#define ATC_SEPARATOR_LEN                                       (2)

/** OK AT command result code */
#define ATC_RESULT_CODE_VERBOSE_OK                              "OK"

/** Length of OK AT command result code */
#define ATC_RESULT_CODE_VERBOSE_OK_LEN                          (2)

/** ERROR AT command result code */
#define ATC_RESULT_CODE_VERBOSE_ERROR                           "ERROR"

/** Length of ERROR AT command result code */
#define ATC_RESULT_CODE_VERBOSE_ERROR_LEN                       (5)

/** Socket Write Prompt AT command result code */
#define ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT             "@"

/** Length of Socket Write Prompt AT command result code */
#define ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN         (1)

/** Minimum length of result code excluding start/end separators */
#define ATC_RESULT_CODE_MIN_LEN                                 (1)

/** Minimum length of unsolicited result code excluding start/end separators */
#define ATC_URC_LEN_MIN                                         (1)

/** URC prefix length */
#define ATC_URC_PRE_LEN                                         (5)

/** URC prefix string */
#define ATC_URC_PRE_STR                                         "\r\n+UU"

/** Maximum length of request */
#define ATC_REQUEST_DATA_LEN_MAX                                (256)

/** Maximum number of response formats */
#define ATC_RESPONSE_FORMAT_COUNT_MAX                           (2)

/** Invalid ID of response format */
#define ATC_RESPONSE_FORMAT_INVALID_ID                          (-1)

/** Sleep time in receiving AT command response (in millisecond) */
#define ATC_RECV_DELAY                                          (1000)

/** Max re-try times when connect to network*/
#define MAX_REPETITIONS 30

/** Max size of modem buffer*/
#define MODEM_BUFFER_SIZE 2048

/** Num of AT Command*/
typedef enum atc_cmd_num_t {
    ECHO_ON = 0,
    ECHO_OFF,
    SET_ERR_VERSION,
    IMEI_CHECK,
    PIN_STATUS,
    ICCID_CHECK,
    CSQ_QUAILTY,
    GPRS_STATUS,
    GSM_STATUS,
    REGISTER_UN,
    REGISTER_CM,
    REQUIRE_IP_ADDR,
    FLASH_GPRSCFG,
    ACTIVE_GPRS,
    CHECK_GPRS_STATUS,
    CHECK_GPRS_STATUS_R,
    ACTIVE_GPRS_URC,
    SOCKET_HEX,
    CHECK_HEX,
    CREATE_SOCKET,
    CREATE_SOCKET_CHECK,
    WRITE_HEAD,
    WRITE_DATA,
    READ_DATA,
    READ_AGAIN,
    CLOSE_SOCKET,
    SHUTDOWN_MODEM,
    CONNECT_SOCKET,
    ADDR_INFO,
    ADDR_INFO_CHECK,
    OK_CHECK,
} atc_cmd_num_t;

/** AT command's sendout and response string point*/
typedef struct atc_string_t {
    atc_cmd_num_t cmd_num;           /**< AT cmd number*/
    uint8_t *cmd_name;               /**< AT cmd name in string*/
    uint8_t *cmd_str;                /**< String send out part*/
    int32_t cmd_str_len;
    uint8_t *cmd_plus;               /**< Extern part of AT CMD*/
    int32_t cmd_plus_len;
    uint8_t *cmd_nrp;                /**< normal response which must get*/
    int32_t cmd_nrp_len;
    uint8_t *cmd_erp;                /**< expected response with value*/
    int32_t cmd_erp_len;
    uint8_t *response;               /**< response string */
} atc_string_t;

/** Status of process AT reponse*/
typedef enum atc_status_t {
    ATC_IDLE = 0,
    ATC_PROCESS_ECHO,
    ATC_PROCESS_RESPONSE,
    ATC_PROCESS_RESULT,
    ATC_PROCESS_OK,
} atc_status_t;

/**
 * Check and call URC handler if URC exist
 * @param index  UART index of modem using
 * @param timeout time out of each process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_***</code>
 */
int32_t atc_check_urc_exist(uint32_t index, uint32_t timeout,
                            uint8_t ** urc);

/**
 * Call this to execute one AT command and handle response
 * @param index  UART index of modem using
 * @param at_cmd point to AT command's in and out information
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_***</code>
 */
int32_t atc_execute_singal_at_command(uint32_t index,
                                      atc_string_t * atc_cmd,
                                      uint32_t timeout);

/**
 * Get current msg completed len 
 * @param index  UART index of modem using
 * @param data_pos value in start postion of read, when return point to \r\n start string
 * @param msg_len @out len of message only start with \r\n and end with \r\n
 * @param total_len @in read available len of buffer
                   @out total len of read postion need to move 
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_***</code>
 */
int32_t atc_get_current_msg_len(uint32_t index,
                                const uint8_t ** data_pos,
                                int32_t * msg_len, int32_t * total_len);

#endif                          /* __AT_COMMAND_H__ */
