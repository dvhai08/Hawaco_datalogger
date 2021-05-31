#ifndef SERVER_MSG_H
#define SERVER_MSG_H

#include <stdint.h>

/**
 * @brief       Process cmd from server
 * @param[in]   cmd Server cmd 
 */
void server_msg_process_cmd(char *cmd);

#endif /* SERVER_MSG_H */
