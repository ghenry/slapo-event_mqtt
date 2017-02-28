/**
 * =====================================================================================
 *
 *          \file:  event_mqtt_pub.h
 *
 *    Description:  Publish Client 
 *
 *       \version:  1.0
 *          \date:  09/02/17
 *       Revision:  none
 *       Compiler:  gcc
 *       Template:  cvim <http://www.vim.org/scripts/script.php?script_id=213>
 *     Formatting:  clang-format
 *
 *        \author:  Gavin Henry <ghenry@suretecsystems.com>
 *       \license:  GPLv2
 *     \copyright:  (c) 2017, Gavin Henry - Suretec Systems Ltd. T/A SureVoIP
 *   Organization:  Suretec Systems Ltd. T/A SureVoIP
 *
 * =====================================================================================
 */


#ifndef _EVENT_MQTT_PUB_H
#define _EVENT_MQTT_PUB_H

#include "event_mqtt.h"

int em_mosq_init(em_config_t *cfg);
int em_publish( em_config_t *cfg, char *event ); 
int em_free(event_mqtt_data_t *em);
void em_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str);

#endif /* _EVENT_MQTT_PUB_H */
