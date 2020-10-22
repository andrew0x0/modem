/**
 * modem network.
 */

#ifndef __MODME_NET_H__
#define __MODME_NET_H__

#include <stdint.h>

#include "error.h"

#define WAIT_AT_COMMAND_TIMEOUT 100
#define WAIT_PROMPT_TIMEOUT 199
#define MODEM_NET_ONLINE 1
#define MODEM_NET_OFFLINE 0

/**
 * Init UART modem used and set gobal value of uart index which modem used
 * @param speed speed of  modem using
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_ERROR</code>
 */
int32_t modem_net_init(uint32_t index_id, uint32_t speed);

void modem_net_task(void);

int32_t modem_net_select(uint32_t index_id, uint32_t timeout);

int32_t _request_socket_recv_len(uint32_t socket_id,
                                 uint32_t * size, uint32_t timeout);
int32_t modem_net_urc_handle(const uint8_t * response);

/**
 * Check Modem connect to gprs network
 * @param gprs_status @out Modem connect to gprs network or not,1 mean get ip
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> always successful;
 */
int32_t modem_net_check_gprs_status(uint32_t * gprs_status,
                                    uint32_t timeout);

/**
 * Open modem operate as power on until to get ip
 * @param timeout time out of each process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_ERROR</code>
 */
int32_t modem_net_open(uint32_t timeout);

/**
 * Create modem socket
 * @param socket_id @our socket id created
 * @param protocol @in socket's protocol
 * @param timeout time out of each process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>-ERROR_ERROR</code>
 */
int32_t modem_net_socket(uint32_t * socket_id,
                         uint32_t protocol, uint32_t timeout);

/**
 * get ip addr of host
 * @param socket_id socket_id of connect to
 * @param host's domain name
 * @param response,string of host's ip addr 
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> if successful;
 *        otherwise return <code>ERROR_ERROR</code>
 */

int32_t modem_net_getaddrinfo(uint32_t * addr_len,
                              uint32_t * host,
                              char *response, uint32_t timeout);

/**
 * Connect to ip addr with socket modem support
 * @param socket_id socket_id of connect to
 * @param ip string include endpoint's ip addr
 * @param port endpoint's port
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>ERROR_ERROR</code>
 */
int32_t modem_net_connect(uint32_t socket_id,
                          uint32_t * ip_addr,
                          uint32_t * port, uint32_t timeout);

/**
 * Send data to  socket
 * @param socket_id socket_id of send to
 * @param buf pointer to data sendout
 * @param size data's size
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>ERROR_ERROR</code>
 */

int32_t modem_net_send(uint32_t socket_id,
                       uint8_t * buf, uint32_t size, uint32_t timeout);

/**
 * receivd data from  socket
 * @param socket_id socket_id of created
 * @param buf pointer to store received data
 * @param size data's size
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>ERROR_ERROR</code>
 */
int32_t modem_net_recv(uint32_t socket_id,
                       uint8_t * buf, uint32_t * size, uint32_t timeout);

/**
 * Close Modem socket
 * @param socket_id socket_id of created
 * @param timeout time out of totally process
 * @return <code>ERROR_SUCCESS</code> if successful; otherwise return <code>ERROR_ERROR</code>
 */

int32_t modem_net_socket_close(uint32_t socket_id,uint32_t timeout);


#endif                          /* __MODME_NET_H__ */
