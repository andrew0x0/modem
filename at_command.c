#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cmsis_os.h"

#include "uart.h"
#include "logger.h"
#include "util.h"

#include "at_command.h"

static atc_cmd_num_t at_cmd_num;

int32_t atc_check_urc_exist(uint32_t index, uint32_t timeout,
                            uint8_t ** urc)
{
    int32_t data_len = 0, total_len;
    uint8_t *data_recv;
    uint32_t start_ts = osKernelSysTick();
    *urc = NULL;

    while (osKernelSysTickOffset(osKernelSysTick(), start_ts) < timeout) {
        total_len = uart_recv_lite(index, (const uint8_t **) &data_recv,
                                   MODEM_BUFFER_SIZE, 0, 0);

        if (atc_get_current_msg_len(index, (const uint8_t **) &data_recv,
                                    (int32_t *) & data_len,
                                    (int32_t *) & total_len) < 0) {
            continue;
        }
        if (data_len > ATC_URC_PRE_LEN) {
            if (strncmp((const char *) data_recv, ATC_URC_PRE_STR,
                        ATC_URC_PRE_LEN) == 0) {
                *urc = data_recv;
                uart_advance_rcursor(index, total_len);
            }
            return ERROR_SUCCESS;
        }
        else {
            return -ERROR_UART_SHORTMSG;
        }

    }

    return -ERROR_UART_TIMEOUT;
}

int32_t atc_get_current_msg_len(uint32_t index, const uint8_t ** data_pos,
                                int32_t * msg_len, int32_t * total_len)
{
    int32_t rc = ERROR_UART_SHORTMSG;
    int32_t data_len = *total_len, temp_len = data_len;
    const uint8_t *data = *data_pos, *start_pos;

    while (data_len > 0) {

        if (strncmp((const char *) data, ATC_SEPARATOR, ATC_SEPARATOR_LEN)
            == 0) {
            start_pos = data;
            data = data + ATC_SEPARATOR_LEN;
            data_len -= ATC_SEPARATOR_LEN;

            if (strncmp((const char *) data,
                        ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT,
                        ATC_RESULT_CODE_VERBOSE_SOCKET_WRITE_PROMPT_LEN)
                == 0) {
                return ERROR_SUCCESS;
            }

            while (data_len > 0) {
                if (strncmp((const char *) data, ATC_SEPARATOR,
                            ATC_SEPARATOR_LEN) == 0) {
                    /*total len include unexpect data */
                    *total_len = temp_len - data_len + ATC_SEPARATOR_LEN;
                    *msg_len = data - start_pos + ATC_SEPARATOR_LEN;
                    *data_pos = start_pos;
                    return ERROR_SUCCESS;
                }
                data++;
                data_len--;

            }
            return -rc;
        }
        else {
            data++;
            data_len--;
        }

    }
    return -ERROR_ERROR;

}

int32_t atc_execute_singal_at_command(uint32_t index,
                                      atc_string_t * at_cmd,
                                      uint32_t timeout)
{
    const uint8_t *data_recv;
    int32_t rc = ERROR_ERROR;
    int32_t data_len, send_len = 0, echo_len = 0;
    atc_status_t modem_status = 0;
    int32_t total_len = 0;;
    uint32_t start_ts, timeout_temp = timeout;

    at_cmd_num = at_cmd->cmd_num;

    if (**(const char ***) &at_cmd->cmd_str != NULL) {
        if (at_cmd->cmd_str_len == 0) {
            send_len = strlen((const char *) at_cmd->cmd_str);
        }
        else {
            osDelay(50);
            send_len = at_cmd->cmd_str_len;
        }
        uart_send(index, (const uint8_t *) at_cmd->cmd_str, send_len);
        echo_len += send_len;
    }

    if (**(const char ***) &at_cmd->cmd_plus != NULL) {
        if (at_cmd->cmd_plus_len == 0)
            send_len = strlen((const char *) at_cmd->cmd_plus);
        else {
            osDelay(50);
            send_len = at_cmd->cmd_plus_len;
        }
        send_len = strlen((const char *) at_cmd->cmd_plus);
        uart_send(index, (const uint8_t *) at_cmd->cmd_plus, send_len);
        echo_len += send_len;
    }

    osDelay(50);
    modem_status = ATC_PROCESS_ECHO;
    start_ts = osKernelSysTick();

    while (osKernelSysTickOffset(osKernelSysTick(), start_ts) <
           timeout_temp) {

        total_len = uart_recv_lite(index, (const uint8_t **) &data_recv,
                                   MODEM_BUFFER_SIZE, 0, 0);

        if (atc_get_current_msg_len(index, (const uint8_t **) &data_recv,
                                    (int32_t *) & data_len,
                                    (int32_t *) & total_len) < 0) {
            continue;
        }

        rc = ERROR_SUCCESS;
        switch (modem_status) {
        case ATC_PROCESS_ECHO:
            if ((strncmp((const char *) data_recv, ATC_COMMAND_VERBOSE_AT,
                         ATC_COMMAND_VERBOSE_AT_LEN) == 0)) {
                uart_advance_rcursor(index, total_len);
            }
            break;

        case ATC_PROCESS_RESPONSE:
            if ((strncmp((const char *) data_recv, ATC_SEPARATOR,
                         ATC_SEPARATOR_LEN) != 0)) {
                rc = -ERROR_ERROR;
            }

            if (strncmp((const char *) (data_recv + ATC_SEPARATOR_LEN +
                                        ATC_RESULT_CODE_VERBOSE_ERROR_LEN),
                        (const char *) "ERROR",
                        ATC_RESULT_CODE_VERBOSE_ERROR_LEN) == 0) {
                rc = -ERROR_ERROR;

            }
            if (strncmp((const char *) (data_recv + ATC_SEPARATOR_LEN),
                        (const char *) "ERROR",
                        ATC_RESULT_CODE_VERBOSE_ERROR_LEN) == 0) {
                rc = -ERROR_ERROR;
            }
            break;

        case ATC_PROCESS_RESULT:
            if ((**(uint8_t ***) & at_cmd->cmd_nrp == NULL) &&
                (**(uint8_t ***) & at_cmd->cmd_erp == NULL)) {
                break;
            }

            if ((strncmp((const char *) data_recv, ATC_SEPARATOR,
                         ATC_SEPARATOR_LEN) != 0)) {
                rc = -ERROR_ERROR;
            }

            if (**(uint8_t ***) & at_cmd->cmd_nrp != NULL) {
                if (strncmp((const char *) (data_recv + ATC_SEPARATOR_LEN),
                            (const char *) at_cmd->cmd_nrp,
                            at_cmd->cmd_nrp_len) == 0) {
                    rc = ERROR_SUCCESS;
                }
                else {
                    rc = -ERROR_ERROR;
                }
            }

            if (rc == ERROR_SUCCESS) {
                at_cmd->response =
                    (uint8_t *) data_recv + ATC_SEPARATOR_LEN;
                uart_advance_rcursor(index, total_len);

            }
            break;

        case ATC_PROCESS_OK:

            if ((strncmp((const char *) data_recv, ATC_SEPARATOR,
                         ATC_SEPARATOR_LEN) != 0)) {
                rc = -ERROR_ERROR;
            }

            if (strncmp((const char *) (data_recv + ATC_SEPARATOR_LEN),
                        ATC_RESULT_CODE_VERBOSE_OK,
                        ATC_RESULT_CODE_VERBOSE_OK_LEN) == 0) {
                rc = ERROR_SUCCESS;
                uart_advance_rcursor(index, total_len);
                return rc;
            }
            break;

        default:
            break;
        }

        if (rc < 0) {
            at_cmd->response = (uint8_t *) data_recv + ATC_SEPARATOR_LEN;
            uart_advance_rcursor(index, total_len);

            return rc;

        }
        modem_status++;

    }
    return -ERROR_ERROR;

}
