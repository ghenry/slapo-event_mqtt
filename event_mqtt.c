/* event_mqtt.c - publish modifications for audit/history purposes via 
 * the MQTT protocol.
 *
 * Copyright 2017 by Gavin Henry, Suretec Systems Limited (SureVoIP).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* ACKNOWLEDGEMENTS:
 * Based on auditlog.c. See ../../../servers/slapd/overlays/auditlog.c
 * for original copyright holders.
 */

#include "portable.h"

#ifdef SLAPD_OVER_EVENT_MQTT

#include <stdio.h>

#include <ac/string.h>
#include <ac/ctype.h>

#include "slap.h"
#include "config.h"
#include "ldif.h"

#include "event_mqtt.h"
#include <mosquitto.h>

/*  We only use the bare min config options right now and will default
 *  the rest. See event_mqtt_config.h */
static ConfigTable event_mqttcfg[] = {
	{ "em_host", "host", 
		2, 2, 0, ARG_STRING|ARG_OFFSET,(void *)offsetof(event_mqtt_data_t, em_host),
		"( OLcfgCtAt:7.1 NAME 'olcEventMQTTHost' "
			"DESC 'MQTT Broker Host' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString SINGLE-VALUE )", NULL, NULL },
	{ "em_port", "port", 
		2, 2, 0, ARG_OFFSET|ARG_INT,(void *)offsetof(event_mqtt_data_t, em_port),
		"( OLcfgCtAt:7.2 NAME 'olcEventMQTTPort' "
			"DESC 'MQTT Broker Port' "
			"EQUALITY integerMatch "
			"SYNTAX OMsInteger SINGLE-VALUE )", NULL, NULL },
	{ "em_topic", "topic", 
		2, 2, 0, ARG_STRING|ARG_OFFSET,(void *)offsetof(event_mqtt_data_t, em_topic),
		"( OLcfgCtAt:7.3 NAME 'olcEventMQTTTopic' "
			"DESC 'MQTT Publish Topic' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString SINGLE-VALUE )", NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED }
};

static ConfigOCs event_mqttocs[] = {
	{	"( OLcfgCtOc:7.1 "
		"NAME 'olcEventMQTTConfig' "
		"DESC 'EventMQTT configuration' "
		"SUP olcOverlayConfig "
		"MAY ( "
			"olcEventMQTTHost "
			"$ olcEventMQTTPort "
			"$ olcEventMQTTTopic "
		") )", Cft_Overlay, event_mqttcfg },
		{ NULL, 0, NULL }
};

static int fprint_ldif(FILE *f, char *name, char *val, ber_len_t len) {
	char *s;
	if((s = ldif_put(LDIF_PUT_VALUE, name, val, len)) == NULL)
		return(-1);
	fputs(s, f);
	ber_memfree(s);
	return(0);
}

static int event_mqtt_response(Operation *op, SlapReply *rs) {
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	event_mqtt_data_t *em = on->on_bi.bi_private;
	em->mosq = NULL;
	Attribute *a;
	Modifications *m;
	struct berval *b, *who = NULL, peername;
	char *what, *whatm, *suffix;
	time_t stamp;
	static int i, rc;

	if ( rs->sr_err != LDAP_SUCCESS ) return SLAP_CB_CONTINUE;

	/*  We'll have some defaults later */
	if ( !em->em_host ) return SLAP_CB_CONTINUE;
	if ( !em->em_port ) return SLAP_CB_CONTINUE;
	if ( !em->em_topic ) return SLAP_CB_CONTINUE;

	Debug( LDAP_DEBUG_ANY,
		"event_mqtt_response: config found: %s, %d and %s\n",
		em->em_host, em->em_port, em->em_topic );
/*
** add or modify: use modifiersName if present
**
*/
	switch(op->o_tag) {
		case LDAP_REQ_MODRDN:	what = "modrdn";	break;
		case LDAP_REQ_DELETE:	what = "delete";	break;
		case LDAP_REQ_ADD:
			what = "add";
			for(a = op->ora_e->e_attrs; a; a = a->a_next)
				if( a->a_desc == slap_schema.si_ad_modifiersName ) {
					who = &a->a_vals[0];
					break;
				}
			break;
		case LDAP_REQ_MODIFY:
			what = "modify";
			for(m = op->orm_modlist; m; m = m->sml_next)
				if( m->sml_desc == slap_schema.si_ad_modifiersName &&
					( m->sml_op == LDAP_MOD_ADD ||
					m->sml_op == LDAP_MOD_REPLACE )) {
					who = &m->sml_values[0];
					break;
				}
			break;
		default:
			return SLAP_CB_CONTINUE;
	}

	suffix = op->o_bd->be_suffix[0].bv_len ? op->o_bd->be_suffix[0].bv_val :
		"global";

/*
** note: this means requestor's dn when modifiersName is null
*/
	if ( !who )
		who = &op->o_dn;

	peername = op->o_conn->c_peer_name;
	ldap_pvt_thread_mutex_lock(&em->em_mutex);

	char *event = ch_calloc(1,1);
  	if (event == NULL) {
		Debug( LDAP_DEBUG_ANY,
			"event_mqtt_response: event_mqtt_response() failed to allocate memory for event.\n",
			rc, 0, 0 );
		return SLAP_CB_CONTINUE;
	}

	stamp = slap_get_time();
	/* Check concatf return codes etc. */
	concatf(&event, "# %s %ld %s%s%s %s conn=%ld\n",
		what, (long)stamp, suffix, who ? " " : "", who ? who->bv_val : "",
		peername.bv_val ? peername.bv_val: "", op->o_conn->c_connid);


	if ( !BER_BVISEMPTY( &op->o_conn->c_dn ) &&
		(!who || !dn_match( who, &op->o_conn->c_dn )))
		concatf(&event, "# realdn: %s\n", op->o_conn->c_dn.bv_val );

	concatf(&event, "dn: %s\nchangetype: %s\n",
		op->o_req_dn.bv_val, what);

	switch(op->o_tag) {
	  case LDAP_REQ_ADD:
		for(a = op->ora_e->e_attrs; a; a = a->a_next)
		  if((b = a->a_vals) != NULL)
			for(i = 0; b[i].bv_val; i++)
				//fprint_ldif(f, a->a_desc->ad_cname.bv_val, b[i].bv_val, b[i].bv_len);
		break;

	  case LDAP_REQ_MODIFY:
		for(m = op->orm_modlist; m; m = m->sml_next) {
			switch(m->sml_op & LDAP_MOD_OP) {
				case LDAP_MOD_ADD:	 whatm = "add";		break;
				case LDAP_MOD_REPLACE:	 whatm = "replace";	break;
				case LDAP_MOD_DELETE:	 whatm = "delete";	break;
				case LDAP_MOD_INCREMENT: whatm = "increment";	break;
				default:
					concatf(&event, "# MOD_TYPE_UNKNOWN:%02x\n", m->sml_op & LDAP_MOD_OP);
					continue;
			}
			concatf(&event, "%s: %s\n", whatm, m->sml_desc->ad_cname.bv_val);
			if((b = m->sml_values) != NULL)
			  for(i = 0; b[i].bv_val; i++)
				//fprint_ldif(f, m->sml_desc->ad_cname.bv_val, b[i].bv_val, b[i].bv_len);
			concatf(&event, "-\n");
		}
		break;

	  case LDAP_REQ_MODRDN:
		concatf(&event, "newrdn: %s\ndeleteoldrdn: %s\n",
			op->orr_newrdn.bv_val, op->orr_deleteoldrdn ? "1" : "0");
		if(op->orr_newSup) concatf(&event, "newsuperior: %s\n", op->orr_newSup->bv_val);
		break;

	  case LDAP_REQ_DELETE:
		/* nothing else needed */
		break;
	}
	concatf(&event, "# end %s %ld\n\n", what, (long)stamp);

    	// Mostly like the pub_client.c example of libmosquitto 
	em_config_t *cfg = ch_calloc(1, sizeof(em_config_t));
	if (cfg == NULL) {
		Debug( LDAP_DEBUG_ANY,
			"event_mqtt_response: failed to allocate memory for MQTT config.\n",
			0, 0, 0 );
	return SLAP_CB_CONTINUE;
	}
	cfg->host  = em->em_host;
	cfg->port  = em->em_port;
	cfg->topic = em->em_topic;
	cfg->mosq  = em->mosq;

	/*  Connect to MQTT broker and publish event. 
	 *
	 *  I'm unsure why if I pass em into em_publish instead of cfg it becomes NULL. When I do,
	 *  I'll come back to here, as cfg should be done in em_publish and use em->em_topic etc. direct. 
	 *  It's the same pointer but the memory it points to, all struct  members are wiped out. 
	 *  Anyone? */
	rc = em_publish( cfg, event ); 
	if ( rc ) {
		Debug( LDAP_DEBUG_ANY,
			"event_mqtt_response: em_publish() failed with rc as: %d\n",
			rc, 0, 0 );
		return SLAP_CB_CONTINUE;
	}

	/*  Move to a em_free( cfg ); */
	/*  Any more clean up here? */
	ch_free(event);

	/* There's a lot to free on this structure, so use the main lib */
	mosquitto_destroy( cfg->mosq );
	mosquitto_lib_cleanup(); /*  Check this is the thread safe one. */
	ch_free(cfg);

	/*  Do we need mutex here? */
	ldap_pvt_thread_mutex_unlock(&em->em_mutex);
	return SLAP_CB_CONTINUE;
}

/*  Our complete overlay instance */
static slap_overinst event_mqtt;

static int
event_mqtt_db_init(
	BackendDB *be,
	ConfigReply *cr
)
{
	/* Get our private pointer, allocate memory and store it in the overlay */
	slap_overinst *on = (slap_overinst *)be->bd_info;
	event_mqtt_data_t *em = ch_calloc(1, sizeof(event_mqtt_data_t));

	on->on_bi.bi_private = em;
	ldap_pvt_thread_mutex_init( &em->em_mutex );
	return 0;
}

static int
event_mqtt_db_destroy(
	BackendDB *be,
	ConfigReply *cr
)
{
	slap_overinst *on = (slap_overinst *)be->bd_info;
	event_mqtt_data_t *em = on->on_bi.bi_private;
	ldap_pvt_thread_mutex_destroy( &em->em_mutex );

	ch_free( em );
	on->on_bi.bi_private = NULL;
	return 0;
}

int event_mqtt_initialise() {

	event_mqtt.on_bi.bi_type = "event_mqtt";
	event_mqtt.on_bi.bi_db_init = event_mqtt_db_init;
	event_mqtt.on_bi.bi_db_destroy = event_mqtt_db_destroy;
	event_mqtt.on_response = event_mqtt_response;

	event_mqtt.on_bi.bi_cf_ocs = event_mqttocs;
	int rc = config_register_schema( event_mqttcfg, event_mqttocs );
	if ( rc ) {
		Debug( LDAP_DEBUG_ANY,
			"event_mqtt_initialise: config_register_schema() failed with rc as: %d\n",
			rc, 0, 0 );
		return rc;
	}

	return overlay_register(&event_mqtt);
}

#if SLAPD_OVER_EVENT_MQTT == SLAPD_MOD_DYNAMIC && defined(PIC)
int
init_module( int argc, char *argv[] )
{
	return event_mqtt_initialise();
}
#endif

#endif /* SLAPD_OVER_EVENT_MQTT */
