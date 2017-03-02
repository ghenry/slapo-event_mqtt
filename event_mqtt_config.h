/**
 * =====================================================================================
 *
 *          \file:  event_mqtt_config.h
 *
 *    Description:  Mosquitto config structures and routines mostly based on 
 *                  mosquitto/client/client_shared.h and client_shared.c
 *
 *       \version:  1.0
 *          \date:  03/02/17
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


#ifndef _EVENT_MQTT_CONFIG_H
#define _EVENT_MQTT_CONFIG_H

#include <mosquitto.h> 
#include "ldap_pvt_thread.h"

struct em_config {

	char *id;
	char *id_prefix;
	int protocol_version;
	int keepalive;
	char *host;
	int port;
	int qos;
	int mid_sent;
	int last_mid;
	int last_mid_sent;
	bool connected;
	bool disconnect_sent;
	bool clean_session;
	bool retain;
	char *message; /* pub */
	long msglen; /* pub */
	char *topic; /* pub */
	char *bind_address;
	struct mosquitto *mosq;
#ifdef WITH_SRV
	bool use_srv;
#endif
	bool debug;
	bool quiet;
	unsigned int max_inflight;
	char *username;
	char *password;
	char *will_topic;
	char *will_payload;
	long will_payloadlen;
	int will_qos;
	bool will_retain;
#ifdef WITH_TLS
	char *cafile;
	char *capath;
	char *certfile;
	char *keyfile;
	char *ciphers;
	bool insecure;
	char *tls_version;
#  ifdef WITH_TLS_PSK
	char *psk;
	char *psk_identity;
#  endif
#endif
};

typedef struct em_config em_config_t;

struct event_mqtt_data {
	ldap_pvt_thread_mutex_t em_mutex;
	char *em_host;
	int  em_port;
	char *em_topic;
	struct mosquitto *mosq;
};

typedef struct event_mqtt_data event_mqtt_data_t;

int client_config_load(em_config_t *config, int pub_or_sub, int argc, char *argv[]);
void client_config_cleanup(em_config_t *cfg);
int client_opts_set(struct mosquitto *mosq, em_config_t *cfg);
int client_id_generate(em_config_t *cfg, const char *id_base);
int client_connect(struct mosquitto *mosq, em_config_t *cfg);

#endif /* _EVENT_MQTT_CONFIG_H */
