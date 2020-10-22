#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cmsis_os.h"
#include "error.h"
#include "limits.h"
#include "task.h"

#include "uart.h"
#include "logger.h"
#include "util.h"

#include "modem.h"
#include "modem_net.h"
#include "at_command.h"
#include "ublox.h"

#define  UBLOX_MODEM_SHUTDOWN_DELAY_MS 5999

SemaphoreHandle_t modem_semaphore;

static xTaskHandle _task_modem_net_handle;
static xTaskHandle _task_modem_user_handle = NULL;
static xTaskHandle _task_mqtt_handle = NULL;
static xTaskHandle _task_nanopb_handle = NULL;
static const uint8_t * modem_net_task_response;
static uint32_t modem_net_task_rc;
uint8_t imei[16];

int32_t modem_net_task_notify(const uint8_t * response);
int32_t modem_imei_check(uint32_t timeout);



void modem_net_task(void)
{
    uart_index_t index = UART_INDEX_MODEM;
    uint32_t gprs_status;
    uint32_t ulNotifiedValue;
    int32_t rc;

    _task_modem_net_handle = xTaskGetCurrentTaskHandle( );
    modem_semaphore = xSemaphoreCreateBinary();
    if ( modem_semaphore == NULL) {
        logger_debug("Create modem semaphore failure!");
        logger_flush( );
    }
    else {
modem_init:
        modem_net_init(index, UART_BAUDRATE_DEBUG);
modem_open:
        /** timeout is the same as network.h SOCKET_TIMEOUT*/
        modem_net_check_gprs_status((uint32_t *)&gprs_status ,UBLOX_MODEM_SHUTDOWN_DELAY_MS);
        if ( gprs_status != 1 ){
            rc = modem_net_open(UBLOX_MODEM_SHUTDOWN_DELAY_MS*2);
            if (rc < 0){
                logger_debug("Modem net open failed!");
                logger_flush( );
//                modem_deinit( );
               
                modem_set_state(MODEM_STATE_OFF);
                uart_deinit(UART_INDEX_MODEM);

                goto modem_init;
            }
        
        } else {
             modem_imei_check(WAIT_AT_COMMAND_TIMEOUT);
        }

        
        if ( xSemaphoreGive( modem_semaphore ) != pdTRUE )  
        {
            logger_debug("Give modem semaphore failure!");
            logger_flush( );
        }
        if (_task_modem_user_handle != NULL) {
            /** If modem user are waiting, give notify when modem re-init*/
            /** First time no needed */
            logger_debug("Give _task_modem_user_handle!");
            logger_flush( );
            xTaskNotifyGive(_task_modem_user_handle);            
        }
        
    }

    taskYIELD();        

    /** task switch*/
    for(;;){
        rc = 0;
       
        if (xTaskNotifyWait( 0x00, ULONG_MAX,&ulNotifiedValue, (TickType_t )portTICK_PERIOD_MS*1000*60*5 ) != pdTRUE)
        {
              modem_set_state(MODEM_STATE_OFF);
              uart_deinit(UART_INDEX_MODEM);
              goto modem_init;
        }
        while ( modem_net_task_response != NULL ) {
            modem_net_task_rc = modem_net_urc_handle(modem_net_task_response);
            rc = atc_check_urc_exist(uart_index, 10,
                                 (uint8_t **) & modem_net_task_response);
            if ( modem_net_task_response == NULL &&
                 rc == -ERROR_UART_SHORTMSG) {

                uart_recv_lite(uart_index, (const uint8_t **) &modem_net_task_response,
                               ATC_SEPARATOR_LEN
                               +ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN
                               , 0, 0);
                if (strncmp((const char *)(modem_net_task_response+ATC_SEPARATOR_LEN),
                            ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT,
                            ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN) == 0 ) {
                    uart_advance_rcursor(uart_index,
                                         ATC_SEPARATOR_LEN
                                         +ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN);
                }
            }   
            if ( modem_net_task_response == NULL &&
                 (modem_net_task_rc == -ERROR_ERROR ||
                  modem_net_task_rc == -ERROR_SOCKET_CLOSED)) {
/*
                modem_set_state(MODEM_STATE_OFF);
                uart_deinit(UART_INDEX_MODEM);
                goto modem_init;
*/
            }
        } 
        if (_task_modem_user_handle != NULL) 
            xTaskNotifyGive(_task_modem_user_handle);
        taskYIELD();        
    }
}


int32_t modem_imei_check(uint32_t timeout)
{
    atc_cmd_num_t cmd_num = IMEI_CHECK;
    const uint8_t *response;
    int32_t rc, send_len;

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (rc < 0) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response ;
        
        cmd_num = OK_CHECK;
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;

    }
    if (rc < 0 ) {
        return -ERROR_ERROR;
    
    }
    memset( imei,0x00,16);
    memcpy(imei,response,15);
    logger_debug("Get IMEI %s!",imei);        
    logger_flush( );
    
    return ERROR_SUCCESS;

}

int32_t modem_net_init(uint32_t uart_index_id, uint32_t speed)
{
    int32_t rc = ERROR_SUCCESS;
//    rc = (int32_t)modem_init(speed);
    
    uart_index = uart_index_id;
    modem_init_on_power();

    uart_init(uart_index, speed);
    uart_recv_dma(uart_index);
    uart_flush(uart_index);

    memset(socket_available_num,0x00,sizeof(socket_available_num));
    modem_set_state(MODEM_STATE_ON);
  
    return rc;
}

int32_t modem_net_task_notify(const uint8_t * response)
{
    uint32_t ulNotifiedValue;

    if ( response == NULL)
        return 0;
    _task_modem_user_handle = xTaskGetCurrentTaskHandle( );
    modem_net_task_response = response;


    xTaskNotifyGive(_task_modem_net_handle);
    taskYIELD();
    xTaskNotifyWait( 0x00, ULONG_MAX,&ulNotifiedValue, portMAX_DELAY );

    _task_modem_user_handle = NULL;
    
    return modem_net_task_rc;
}

int32_t modem_net_select(uint32_t socket_id, uint32_t timeout)
{
    int32_t rc;//, data_len = 0;
    const uint8_t *response;
    uint32_t start_ts = osKernelSysTick();

    do {
    if (socket_available_num[socket_id] == -ERROR_SOCKET_CLOSED)
        return -ERROR_SOCKET_CLOSED;
    
    rc = atc_check_urc_exist(uart_index, timeout/10,
                                 (uint8_t **) & response);
    if (rc == ERROR_SUCCESS && response != NULL) {
        rc = modem_net_task_notify(response);
        if ( rc == ERROR_SUCCESS)
            break;
        
    }

        
    } while (osKernelSysTickOffset(osKernelSysTick(), start_ts) < timeout);

    rc = socket_available_num[socket_id];

    return rc;
    
}

int32_t modem_net_check_gprs_status(uint32_t * gprs_status,
                                    uint32_t timeout)
{
    const uint8_t *response;
    int32_t rc = ERROR_ERROR;
    atc_cmd_num_t cmd_num = CHECK_GPRS_STATUS;

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (  rc < 0 ){
        rc = modem_net_task_notify(response);
        response = modem_net_task_response ;    
        cmd_num = CHECK_GPRS_STATUS_R;
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;
    }
    
    if( rc < 0) {
         *gprs_status = 0;
    }
    else {        
        *gprs_status = 1;
    }

    return ERROR_SUCCESS;
    
}

int32_t modem_net_open(uint32_t timeout)
{
    int32_t rc = ERROR_ERROR;
    uint32_t time_out = timeout;
    atc_cmd_num_t cmd_num;
    int32_t exe_times = 0;
    int32_t cmd_size = sizeof(UBLOX_CMD) / sizeof(UBLOX_CMD[0]);
    const char *response;

    for (cmd_num = ECHO_OFF; cmd_num < cmd_size; cmd_num++) {
        if ((int) cmd_num == (int) PIN_STATUS ||
            (int) cmd_num == (int) ACTIVE_GPRS) {
            time_out = timeout * cmd_num;
        }

        if ((int) cmd_num == (int) CSQ_QUAILTY ||
            (int) cmd_num == (int) GPRS_STATUS ||
            (int) cmd_num == (int) CHECK_GPRS_STATUS)
            osDelay(ATC_RECV_DELAY * cmd_num);

        osDelay(ATC_RECV_DELAY);
        
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], time_out);
        response = (const char *) UBLOX_CMD[cmd_num].response;


        if (rc < 0 ) {
            rc = modem_net_urc_handle(response);
            if ( rc < 0) {
                logger_debug("get response is %s and rc %d\n", (char *) response,
                             (int) rc);
                logger_flush( );
                return -ERROR_ERROR;
                            
            }
        }

        if (exe_times > MAX_REPETITIONS)
            return -ERROR_ERROR;
        
        switch (cmd_num) {
        case CSQ_QUAILTY:
            if (strncmp(response, "+CSQ: 99,99", 11) == 0) {
                cmd_num--;
            }
            break;

            
        case GPRS_STATUS:
            if (strncmp(response, "+CGATT: 0", 9) == 0) {
                cmd_num--;
            }

            break;

        case GSM_STATUS:
            if (strncmp(response, "+COPS: 0,0,\"CHINA  MOBILE\"", 26) == 0)
                cmd_num++;
            break;

        case IMEI_CHECK:
            memset( imei,0x00,16);
            memcpy(imei,response,15);
            logger_debug("Get IMEI %s!",imei);        
            logger_flush( );
            break;

        case REGISTER_UN:
            cmd_num++;

            break;

        case CHECK_GPRS_STATUS:
            cmd_num = ACTIVE_GPRS_URC;
            break;
            
        default:
            if (cmd_num == CHECK_HEX)
                return ERROR_SUCCESS;
            break;

        }
        exe_times++;
    }
    rc = ERROR_SUCCESS;
    return rc;
}

int32_t modem_net_socket(uint32_t * socket_id, uint32_t protocol,
                         uint32_t timeout)
{
    int32_t rc = ERROR_ERROR;
    atc_cmd_num_t cmd_num = CREATE_SOCKET;
    const char *response;

    if (protocol == 6) {
        UBLOX_CMD[cmd_num].cmd_plus = (uint8_t *) "6\r";
        UBLOX_CMD[cmd_num].cmd_plus_len = 2;
    }

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (rc < 0) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response ;

        
        cmd_num++;
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;

    }

    if ( rc < 0 ) {
        logger_debug("get response is %s and rc %d\n", (char *) response,
                     (int) rc);
        logger_flush( );
        return -ERROR_CREATE_SOCKET_FAILURE;
    }
    
    if (strncmp(response, "+USOCR: ", 8) == 0) {
        *socket_id = *(response + 8) - '0';
        socket_available_num[*socket_id] = ( int ) ERROR_SUCCESS ;
        return ERROR_SUCCESS;
    } else {
        return -ERROR_ERROR;
    }

    return ERROR_SUCCESS;
}

int32_t modem_net_getaddrinfo(uint32_t * addr_len, uint32_t * host,
                              char *addr_str, uint32_t timeout)
{
    atc_cmd_num_t cmd_num = ADDR_INFO;
    const char *response, *temp;
    uint8_t data_store[64];
    int32_t rc;
    int32_t str_len = 0;

    sprintf((char *) data_store, "0,\"%s\"\r", (char *) host);

    UBLOX_CMD[cmd_num].cmd_plus = (uint8_t *) & data_store;
    UBLOX_CMD[cmd_num].cmd_plus_len = strlen((const char *) data_store);

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (rc < 0) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response;

        cmd_num ++;

        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;

        if ( rc < 0 ) {
            logger_debug("get response is %s and rc %d\n", (char *) response,
                         (int) rc);
            logger_debug("Expected string starting with %s\n",
                         (char *) data_store);
            logger_flush( );
            return rc;
                    
        }
    }
    
    if (strncmp((const char *) response, "+UDNSRN: ", 9) == 0) {
        response = response + 10;
        temp = response;

        while (*(char *) response++ != '"' || str_len > 15)
            str_len++;
        if (str_len <= 15)
            memmove(addr_str, temp, str_len);
        else
            return -ERROR_ERROR;
        *addr_len = str_len;
    } else {
        return -ERROR_ERROR;
        
    }

    return ERROR_SUCCESS;

}

int32_t modem_net_connect(uint32_t socket_id, uint32_t * ip_addr,
                          uint32_t * port, uint32_t timeout)
{
    atc_cmd_num_t cmd_num = CONNECT_SOCKET;
    uint8_t data_store[64];
    int32_t rc,send_len;
    const char *response;
    
    sprintf((char *) data_store, "%u,\"%s\",%s\r",
            (unsigned int) socket_id, (char *) ip_addr, (char *) port);

    UBLOX_CMD[cmd_num].cmd_plus = (uint8_t *) & data_store;
    UBLOX_CMD[cmd_num].cmd_plus_len = strlen((const char *) data_store);

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (rc < 0) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response;

        cmd_num = OK_CHECK;
        
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num], timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;
    
        if ( rc < 0) {
            logger_debug("Connect to  %s\n", (char *) data_store);        
            logger_debug("response is %s with rc %d\n", (char *) response, rc);
            logger_flush();

            modem_net_socket_close(socket_id, UBLOX_MODEM_SHUTDOWN_DELAY_MS);
            logger_debug("Closed socket after connect failure \n");
            logger_flush();
            return -ERROR_ERROR;
                    
        }

    }
    
    return ERROR_SUCCESS;

}

int32_t modem_net_send(uint32_t socket_id, uint8_t * buf, uint32_t size,
                       uint32_t timeout)
{
    atc_cmd_num_t cmd_num = WRITE_HEAD;
    const char *response;
    uint8_t data_store[32];
    int32_t rc, send_len;

    sprintf((char *) data_store, "%u,%u\r", (unsigned int) socket_id,
            (unsigned int) size);
    send_len = strlen((const char *) data_store);

    UBLOX_CMD[cmd_num].cmd_plus = (uint8_t *) & data_store;
    UBLOX_CMD[cmd_num].cmd_plus_len = send_len;

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num],
                                       WAIT_PROMPT_TIMEOUT);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    
    if ( rc < 0 ) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response;
        if (  rc < 0 ){
            logger_debug("write get response is %s\n", (char *) response);
            logger_flush();
            return -ERROR_ERROR;
        }
    }
    
    cmd_num = WRITE_DATA;
    
    UBLOX_CMD[cmd_num].cmd_str = (uint8_t *) buf;
    UBLOX_CMD[cmd_num].cmd_str_len = size;
    

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;


    if (rc < 0) {
        logger_debug("write get response is %s\n", (char *) response);
        logger_flush();

        modem_net_socket_close(socket_id, UBLOX_MODEM_SHUTDOWN_DELAY_MS);
        logger_debug("Closed socket after send failure \n");
        logger_flush();

        return -ERROR_ERROR;
    }
    if (strncmp((const char *) data_store, (const char *) response + 8, 1)
        == 0) {

    } else
        return -ERROR_ERROR;
    
    if (strncmp((const char *) data_store + 2,
                (const char *) response + 10, send_len - 2) == 0) {

    }

    return ERROR_SUCCESS;

}

int32_t modem_net_recv(uint32_t socket_id, uint8_t * buf, uint32_t * size,
                       uint32_t timeout)
{
    atc_cmd_num_t cmd_num = READ_DATA;
    const char *response;
    uint8_t data_store[64];
    int32_t rc, data_len;

    sprintf((char *) data_store, "%u,%u\r", (unsigned int) socket_id,
            (unsigned int) *size);

    UBLOX_CMD[cmd_num].cmd_plus = (uint8_t *) & data_store;
    UBLOX_CMD[cmd_num].cmd_plus_len = strlen((const char *) data_store);

    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;

    if (rc < 0 ) {
        rc = modem_net_task_notify(response);
        response = modem_net_task_response;

        cmd_num++;            
        rc = atc_execute_singal_at_command(uart_index,
                                           (atc_string_t *) &
                                           UBLOX_CMD[cmd_num],
                                           timeout);
        response = (const char *) UBLOX_CMD[cmd_num].response;
        
        if (rc < 0) {
            logger_debug("get response is %s\n", response);
            logger_debug("Expected string starting with %s\n",
                         (char *) data_store);
            logger_flush();

            modem_net_socket_close(socket_id, UBLOX_MODEM_SHUTDOWN_DELAY_MS);
            logger_debug("Closed socket after send failure \n");
            logger_flush();
 
            return -ERROR_ERROR;
        }
    }

    if (strncmp((const char *) data_store, (const char *) response + 8, 1)
        != 0) {
        logger_debug("get response is 0x%x\n", response);
        logger_debug("get response data is %s \n", response);
        logger_debug("Expected string starting with %s\n",
                     (char *) data_store);
        logger_flush();
        return -ERROR_ERROR;
    }

    data_len = 0;
    response = response + 10;
    if (strncmp((const char *) response, "0", 1) == 0) {
        buf = NULL;
        goto end;
    }

    while (strncmp((const char *) response, ",\"", ATC_SEPARATOR_LEN)
           != 0) {
        data_len = (int32_t) (*(char *) response - '0') + data_len * 10;
        response++;
    }
    response = response + 2;
    memcpy(buf, response, data_len);
    
end:
    *size = data_len;
    data_len = socket_available_num[socket_id] - *size;
    socket_available_num[socket_id] = (data_len >= 0 ? data_len : 0);

    return ERROR_SUCCESS;

}


int32_t modem_net_urc_handle(const uint8_t * response)
{
    int32_t data_len = 0;
    uint32_t socket_num;

    if (strncmp((const char *) response, ATC_SEPARATOR, ATC_SEPARATOR_LEN)
        == 0)
        response = response + ATC_SEPARATOR_LEN;

    if (strncmp((char *) response, "+UUSORD: ", 9) == 0) {
        socket_num = (int32_t) (*(response + 9) - '0');
        response = response + 11;

        while (strncmp((char *) response, ATC_SEPARATOR, ATC_SEPARATOR_LEN)
               != 0) {
            data_len = (int32_t) (*response - '0') + data_len * 10;
            response++;

        }

        socket_available_num[socket_num] = data_len;

        return ERROR_SUCCESS;
    }

    if (strncmp((char *) response, "+UUSOCL: ", 9) == 0) {
        socket_num = (int32_t) (*(response + 9) - '0');
        socket_available_num[socket_num] = (int)-ERROR_SOCKET_CLOSED;
        return -ERROR_SOCKET_CLOSED;
    }

    if (strncmp((char *) response, "+CGREG: ", 8) == 0) {
        return -ERROR_ERROR;
    }

    if (strncmp((char *) response, "+CREG: ", 7) == 0) {
        return -ERROR_ERROR;
    }
        
    if (strncmp((const char *)response,
                ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT,
                ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN) == 0 ) {
        return ERROR_SUCCESS;        
    }

    if (strncmp((char *) response, "+UU", 3) == 0) {
        /* Command URC just skip it */
        return ERROR_SUCCESS;
    }

    if (strncmp((char *) response, "RING", 4) == 0) {
        /* Command URC just skip it */
        return ERROR_SUCCESS;
    }

    return -ERROR_ERROR;
}


int32_t modem_net_socket_close(uint32_t socket_id,uint32_t timeout)
{
    atc_cmd_num_t cmd_num = WRITE_HEAD;
    const char *response;
    uint8_t data_store[32];
    int32_t rc, send_len;

    if( socket_id < 0 || socket_id >= UBLOX_SOCKET_NUM)
        return -ERROR_ERROR;

    if (socket_available_num[socket_id] == -ERROR_SOCKET_CLOSED)
        return ERROR_SUCCESS;
    
    cmd_num = CLOSE_SOCKET;
    sprintf((char *) data_store, "%u\r", (unsigned int) socket_id);
    send_len = strlen((const char *) data_store);
        
    UBLOX_CMD[cmd_num].cmd_plus = data_store;
    UBLOX_CMD[cmd_num].cmd_plus_len = send_len;
    rc = atc_execute_singal_at_command(uart_index,
                                       (atc_string_t *) &
                                       UBLOX_CMD[cmd_num], timeout);
    response = (const char *) UBLOX_CMD[cmd_num].response;
        

    socket_available_num[socket_id] == -ERROR_SOCKET_CLOSED;
    
    
    return ERROR_SUCCESS;

}
