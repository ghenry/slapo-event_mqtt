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
#include <mosquitto.h>


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

    fprintf( stderr,
	"em_publish: Getting ready to connect and send on topic: %s, with event: %s\n",
    	cfg->topic, event );

    fprintf( stderr,
	"em_publish: Using host: %s and port: %d\n",
    	cfg->host, cfg->port );

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
    cfg->last_mid = -1;
    cfg->last_mid_sent = -1;
    cfg->disconnect_sent = false;
    cfg->qos = 0;
    cfg->message = event;

    mosquitto_lib_init();

    // Do we want to allow the client ID to be set?
    cfg->mosq = mosquitto_new("hodor", true, cfg); // cfg here, to share cfg across callbacks  
    if(!cfg->mosq){
	switch(errno){
		case ENOMEM:
		    fprintf( stderr, 
			    "Error: Out of memory.\n" 
			    );
		    break;
		case EINVAL:
		    fprintf( stderr, 
			    "Error: Invalid id.\n" 
			    );
		    break;
        }
	mosquitto_lib_cleanup();
	return EXIT_FAILURE;
    }
    
    mosquitto_log_callback_set(cfg->mosq, em_log_callback);
    mosquitto_connect_callback_set(cfg->mosq, em_connect_callback);
    mosquitto_disconnect_callback_set(cfg->mosq, em_disconnect_callback);
    mosquitto_publish_callback_set(cfg->mosq, em_publish_callback);
    
    // Expand to offer mosquitto_connect_bind_async to restrict network communication over a 
    // particular interface.
    int rc = MOSQ_ERR_SUCCESS;
    rc = mosquitto_connect_async(cfg->mosq, cfg->host, cfg->port, cfg->keepalive);
    char err[1024];
    if (rc > 0) {
	if(rc == MOSQ_ERR_ERRNO) {
	    strerror_r(errno, err, 1024);
	    fprintf( stderr, 
		    "Error: %s\n", 
		    err);
	}
	else {
	    fprintf( stderr, 
		    "Unable to connect (%s).\n", 
		    mosquitto_strerror(rc));
	}
	mosquitto_lib_cleanup();
	return rc;
    }
    
    rc = mosquitto_loop_forever(cfg->mosq, -1, 1); // -1 is default of 1000ms to wait for packets.
    if ( rc > 0 ) {
	switch(rc){
		case MOSQ_ERR_INVAL:
			fprintf( stderr, "Error: Invalid input.\n");
			break;
		case MOSQ_ERR_NOMEM:
			fprintf( stderr, "Error: An out of memory condition occured.\n");
			break;
		case MOSQ_ERR_NO_CONN:
			fprintf( stderr, "Error: we're not connected to a broker.\n");
			break;
		case MOSQ_ERR_CONN_LOST:
			fprintf( stderr, "Error: connection to the broker was lost.\n");
			break;
		case MOSQ_ERR_PROTOCOL:
			fprintf( stderr, "Error: protocol error communicating with the broker.\n");
			break;
		case MOSQ_ERR_ERRNO:
			strerror_r(errno, err, 1024);
			    fprintf( stderr, 
			    "Error: %s\n", 
			    err);
			break;
	}	
	mosquitto_disconnect(cfg->mosq);
	mosquitto_loop_stop(cfg->mosq, false);
    }
     

    /* rc = mosquitto_loop_start(cfg->mosq);
    if ( rc > 0 ) {
	switch(rc){
		case MOSQ_ERR_INVAL:
			fprintf( stderr, "Error: Invalid input.\n");
			break;
		case MOSQ_ERR_NOT_SUPPORTED:
			fprintf( stderr, "Error: thread support is not available.\n");
			break;
	}	
	mosquitto_disconnect(cfg->mosq);
	mosquitto_loop_stop(cfg->mosq, false);
    }
    */
    return EXIT_SUCCESS;
}

/** */
void em_connect_callback(struct mosquitto *mosq, void *obj, int result) {

    int rc = MOSQ_ERR_SUCCESS;
    em_config_t *cfg = obj;

    fprintf( stderr,
	"em_connect_callback: Getting ready to connect and send on topic: %s, with event: %s\n",
    	cfg->topic, cfg->message );

    rc = mosquitto_publish(mosq, &cfg->mid_sent, cfg->topic, strlen(cfg->message), cfg->message, cfg->qos, cfg->retain);
    if (!result) {
        if ( rc ) {
		switch(rc){
			case MOSQ_ERR_INVAL:
				fprintf( stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
				break;
			case MOSQ_ERR_NOMEM:
				fprintf( stderr, "Error: Out of memory when trying to publish message.\n");
				break;
			case MOSQ_ERR_NO_CONN:
				fprintf( stderr, "Error: Client not connected when trying to publish.\n");
				break;
			case MOSQ_ERR_PROTOCOL:
				fprintf( stderr, "Error: Protocol error when communicating with broker.\n");
				break;
			case MOSQ_ERR_PAYLOAD_SIZE:
				fprintf( stderr, "Error: Message payload is too large.\n");
				break;
		}	
	mosquitto_disconnect(cfg->mosq);
	mosquitto_loop_stop(cfg->mosq, false);
	}
    } 
    else {
	fprintf( stderr,
	    "%s\n", 
	    mosquitto_connack_string(result) );
    }
}

/** */
void em_disconnect_callback(struct mosquitto *mosq, void *obj, int rc) {
    em_config_t *cfg = obj;
    cfg->connected = false;
}

/** */
void em_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{	
    em_config_t *cfg = obj;
    cfg->last_mid_sent = mid;

    if(cfg->disconnect_sent == false){
	mosquitto_disconnect(mosq);
	cfg->disconnect_sent = true;
    }
}

/** */
void em_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    fprintf( stderr,
    	"em_log_callback: %s\n", 
	str );
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
