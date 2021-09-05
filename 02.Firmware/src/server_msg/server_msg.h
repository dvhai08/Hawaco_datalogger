#ifndef SERVER_MSG_H
#define SERVER_MSG_H

#include <stdint.h>

/*!
 * @brief       Process cmd from server
 * @param[in]   cmd Server cmd 
 * @param[out]  new_cfg Has new config flag
 */
void server_msg_process_cmd(char *cmd, uint8_t *new_cfg);

/*!
 * @brief       Process broadcast from serve
 */
void server_msg_process_boardcast_cmd(char *cmd);

#endif /* SERVER_MSG_H */
