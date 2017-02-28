/**
 * =====================================================================================
 *
 *          \file:  event_mqtt_pub.c
 *
 *    Description:  As per parts of main() in
 *		    https://github.com/eclipse/mosquitto/blob/master/client/pub_client.c
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

#include "portable.h" /*  Needed before most OpenLDAP header files */

#include <stdio.h>
#include <stdlib.h>

#include <ac/string.h>
#include <ac/ctype.h>

#include <errno.h>
#include <assert.h>

#include "slap.h"

#include "event_mqtt_pub.h"


int em_mosq_init(em_config_t *cfg)
{
    return EXIT_SUCCESS;
}

/**  
 *  Publish the event data
 *
 *  \param cfg pointer to main cfg structure
 *  \param event Event string
 */
int em_publish( em_config_t *cfg, char *event ) {

    /*  This is tested in event_mqtt_response too, but just make sure */
    assert(cfg->host  != NULL);
    assert(cfg->port  != 0);
    assert(cfg->topic != NULL); 

    Debug( LDAP_DEBUG_ANY,
	"em_publish: Getting ready to connect and send on topic: %s, with event: %s\n",
    	cfg->topic, event, 0 );

    Debug( LDAP_DEBUG_ANY,
	"em_publish: Using host: %s and port: %d\n",
    	cfg->host, cfg->port, 0 );

    // Use em_mosq_init() ????

    // Mostly like the pub_client.c example of mosquitto 
    // Apart from host, port and topic, the rest need to be added to cn=config 
    // options
    cfg->max_inflight = 20;
    cfg->keepalive = 60;
    cfg->clean_session = true;
    cfg->protocol_version = MQTT_PROTOCOL_V31;
    cfg->bind_address = NULL; // No fussed

    cfg->retain = 0;
    cfg->mid_sent = 0;
    cfg->qos = 0;

    //mosquitto_lib_init();

    // Do we want to allow the client ID to be set?
    cfg->mosq = mosquitto_new("hodor", true, NULL);   
    if(!cfg->mosq){
	switch(errno){
		case ENOMEM:
		    fprintf(stderr, "Error: Out of memory.\n");
		    break;
		case EINVAL:
		    fprintf(stderr, "Error: Invalid id.\n");
		    break;
        }
	mosquitto_lib_cleanup();
	return EXIT_FAILURE;
    }
    Debug( LDAP_DEBUG_ANY,
    	"em_publish: Connecting and sending topic: %s, and event: %s\n",
    	cfg->topic, event, 0 );

    mosquitto_log_callback_set(cfg->mosq, em_log_callback);
    mosquitto_connect_bind(cfg->mosq, cfg->host, cfg->port, cfg->keepalive, cfg->bind_address);
    mosquitto_loop_start(cfg->mosq);
    mosquitto_publish(cfg->mosq, &cfg->mid_sent, cfg->topic, strlen(event), event, cfg->qos, cfg->retain);
    return EXIT_SUCCESS;
}

void em_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    Debug( LDAP_DEBUG_ANY,
    	"em_log_callback: %s\n", 
	str, 0, 0 );
}

int em_free( event_mqtt_data_t *em ) 
{
    return EXIT_SUCCESS;
/* 
--------------
Termination
--------------
client_clean_config() Maybe word as destroy()
client_config_cleanup()
mosquitto_destroy()
mosquitto_desconnect()
mosquitto_lib_cleanup()
mosquitto_loop_stop()
*/
}
