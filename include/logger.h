#ifndef LOGGER_H
#define LOGGER_H

#include "messages.h"

int logger_init(void);
void log_process_message(const ProcessMessage *msg);
void log_info(const char *message);
void log_error(const char *message);
void logger_close(void);

#endif
