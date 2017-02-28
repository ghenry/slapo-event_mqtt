/**
 * =====================================================================================
 *
 *          \file:  test.c
 *
 *    Description:  
 *
 *       \version:  1.0
 *          \date:  23/02/17
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

#include <stdlib.h>
#include <stdio.h>
#include "event_mqtt_config.h"
#include "event_mqtt_pub.h"
#include "event_mqtt_utils.h"


int main(void) {

	event_mqtt_data_t *em;
	em = calloc(1, sizeof(event_mqtt_data_t));

	char *event = calloc(1,1);
	concatf(&event, "This is my event you know\n");

	em->em_topic = "hodor_ldap";
	em->em_host = "localhost"; // This box
	em->em_port = 1883;
	em->mosq = NULL;

	fprintf(stderr, 
			"==%s==\nHost: %s\nPort: %d\nTopic: %s\nEvent: %s\n========\n",
			__func__,em->em_host, em->em_port, em->em_topic, event
	);

	fprintf(stderr, "em pointer is: %p\n", em);
	em_publish(em, event);

	return EXIT_SUCCESS;
}

