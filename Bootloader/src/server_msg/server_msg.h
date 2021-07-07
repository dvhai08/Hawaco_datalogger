#ifndef SERVER_MSG_H
#define SERVER_MSG_H

#include <stdint.h>

/**
 * @brief       Process cmd from server
 * @param[in]   cmd Server cmd 
 * @param[out]  new_cfg Has new config flag
 */
void server_msg_process_cmd(char *cmd, uint8_t *new_cfg);

#endif /* SERVER_MSG_H */
