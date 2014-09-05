/*
 * Copyright (c) 2012, CPqD, Brazil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Ericsson Research nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>

#include "action_set.h"
#include "compiler.h"
#include "dp_actions.h"
#include "dp_buffers.h"
#include "dp_exp.h"
#include "dp_ports.h"
#include "datapath.h"
#include "packet.h"
#include "pipeline.h"
#include "flow_table.h"
#include "flow_entry.h"
#include "meter_table.h"
#include "oflib/ofl.h"
#include "oflib/ofl-structs.h"
#include "nbee_link/nbee_link.h"
#include "util.h"
#include "hash.h"
#include "oflib/oxm-match.h"
#include "vlog.h"
#include "state_table.h"
#include "dp_capabilities.h"

#define LOG_MODULE VLM_pipeline

static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(60, 60);

static void
execute_entry(struct pipeline *pl, struct flow_entry *entry,
              struct flow_table **table, struct packet **pkt);
/****
struct pipeline *pipeline_create(struct datapath *dp) {
    struct pipeline *pl;
    int i;
    printf("here is the create pipeline\n");
    printf("capabilities %2x\n",DP_SUPPORTED_CAPABILITIES);

    // hardcoded statefull table init 
    // in table 0 set OFPCT_TABLE_STATEFULL 
    pl = xmalloc(sizeof(struct pipeline));

    for (i=0; i<PIPELINE_TABLES; i++)
    {
        pl->tables[i] = flow_table_create(dp, i);

    }
    pl->dp = dp;

    nblink_initialize();

    //Haniehs' added lines
    if (pl->tables[0])
    {
        
        pl->tables[0]->features->config=OFPTC_TABLE_STATEFUL;
	
	struct state_table *stable = pl->tables[0]->state_table;
        struct key_extractor *kext;
        kext= xmalloc(sizeof(struct key_extractor));
	kext->field_count=1;
	
	kext->fields[0]=OXM_OF_ETH_SRC;		//update
	state_table_set_extractor(stable,kext,0);
	kext->fields[0]=OXM_OF_ETH_SRC;		//lookup	
	state_table_set_extractor(stable,kext,1);

//	printf("key field extractor is %d \n",kext->fields[0]);	
	
      
	////// int update;
	///// for (update=0;update<2;++update)
	////{
	////    state_table_set_extractor(stable,kext,update);
	//////}
	


	int number_flow_entry=0;
	for(number_flow_entry;number_flow_entry<2;number_flow_entry++)
	{
		ofl_err error=0;
		bool match_kept = false;
		bool insts_kept = false;
		struct ofl_msg_flow_mod *msg=xmalloc(sizeof(struct ofl_msg_flow_mod));
		struct ofl_match *m = xmalloc(sizeof(struct ofl_match));
		ofl_structs_match_init(m);	

		msg->table_id=0;
		msg->command=OFPFC_ADD;
		
		uint64_t metadata=number_flow_entry;
		ofl_structs_match_put64(m, OXM_OF_METADATA, metadata);
		msg-> match = (struct ofl_match_header *)m;
	//	printf("match header is %d \n", msg->match->type);
		
		msg->instructions_num=1;
		msg->instructions=xmalloc(sizeof(struct ofl_instruction_header *) * msg->instructions_num);	
		struct ofl_instruction_set_state  *i = xmalloc(sizeof(struct ofl_instruction_set_state ));
		i->header.type = OFPIT_SET_STATE;
		i->state=number_flow_entry+1;
		msg->instructions[0]= (struct ofl_instruction_header *)i;
	//	printf("msg instruction type should be the following %d \n",msg->instructions[0]->type);	
		
		msg->hard_timeout=OFP_FLOW_PERMANENT;
		msg->idle_timeout=OFP_FLOW_PERMANENT;
		msg->priority = OFP_DEFAULT_PRIORITY;

		
		error=flow_table_flow_mod(pl->tables[0],msg,&match_kept,&insts_kept);     
		if (error){
			printf("error for flow mod\n");
		}
		else{
			ofl_msg_free_flow_mod(msg, !match_kept, !insts_kept, pl->dp->exp);
			printf("free flow mod msg\n");
		}
	}	
}

   
    return pl;
} 
****/
/* replaced with upper func. instructions*/

struct pipeline *
pipeline_create(struct datapath *dp) {
    struct pipeline *pl;
    int i;

    printf("here is the create pipeline %02x \n",OXM_OF_ETH_SRC);
    pl = xmalloc(sizeof(struct pipeline));
    for (i=0; i<PIPELINE_TABLES; i++) {
        pl->tables[i] = flow_table_create(dp, i);
    }
    pl->dp = dp;

    nblink_initialize();

    return pl;
}

static bool
is_table_miss(struct flow_entry *entry){

    //printf("here is table miss\n");
    return ((entry->stats->priority) == 0 && (entry->match->length <= 4));

}


/* Sends a packet to the controller in a packet_in message */
static void
send_packet_to_controller(struct pipeline *pl, struct packet *pkt, uint8_t table_id, uint8_t reason) {

    printf("here is send packet to controller\n");
    struct ofl_msg_packet_in msg;
    struct ofl_match *m;
    msg.header.type = OFPT_PACKET_IN;
    msg.total_len   = pkt->buffer->size;
    msg.reason      = reason;
    msg.table_id    = table_id;
    msg.cookie      = 0xffffffffffffffff;
    msg.data = pkt->buffer->data;


    /* A max_len of OFPCML_NO_BUFFER means that the complete
        packet should be sent, and it should not be buffered.*/
    if (pl->dp->config.miss_send_len != OFPCML_NO_BUFFER){
        dp_buffers_save(pl->dp->buffers, pkt);
        msg.buffer_id   = pkt->buffer_id;
        msg.data_length = MIN(pl->dp->config.miss_send_len, pkt->buffer->size);
    }else {
        msg.buffer_id   = OFP_NO_BUFFER;
        msg.data_length = pkt->buffer->size;
    }

    m = &pkt->handle_std->match;
    /* In this implementation the fields in_port and in_phy_port
        always will be the same, because we are not considering logical
        ports                                 */
    msg.match = (struct ofl_match_header*)m;
    dp_send_message(pl->dp, (struct ofl_msg_header *)&msg, NULL);
    ofl_structs_free_match((struct ofl_match_header* ) m, NULL);
}

void
pipeline_process_packet(struct pipeline *pl, struct packet *pkt) {
    struct flow_table *table, *next_table;


    //printf("here is pipeline processing packet\n");
    if (VLOG_IS_DBG_ENABLED(LOG_MODULE)) {
        char *pkt_str = packet_to_string(pkt);
        VLOG_DBG_RL(LOG_MODULE, &rl, "processing packet: %s", pkt_str);
        free(pkt_str);
    }

    if (!packet_handle_std_is_ttl_valid(pkt->handle_std)) {
        if ((pl->dp->config.flags & OFPC_INVALID_TTL_TO_CONTROLLER) != 0) {
            VLOG_DBG_RL(LOG_MODULE, &rl, "Packet has invalid TTL, sending to controller.");

            send_packet_to_controller(pl, pkt, 0/*table_id*/, OFPR_INVALID_TTL);
        } else {
            VLOG_DBG_RL(LOG_MODULE, &rl, "Packet has invalid TTL, dropping.");
        }
        packet_destroy(pkt);
        return;
    }

    next_table = pl->tables[0];
    while (next_table != NULL) {
        struct flow_entry *entry;
	struct state_entry *state_entry;

        VLOG_DBG_RL(LOG_MODULE, &rl, "trying table %u.", next_table->stats->table_id);

        pkt->table_id = next_table->stats->table_id;
        table         = next_table;
        next_table    = NULL;
		
    		//printf("before controlling the table feature config\n");
		if (table->features->config &OFPTC_TABLE_STATEFUL) {
			
			state_entry = state_table_lookup(table->state_table, pkt);
			state_table_write_metadata(state_entry, pkt);
		}

    		//printf("after statetable entry\n");
		// EEDBEH: additional printout to debug table lookup
		if (VLOG_IS_DBG_ENABLED(LOG_MODULE)) {
			char *m = ofl_structs_match_to_string((struct ofl_match_header*)&(pkt->handle_std->match), pkt->dp->exp);
			VLOG_DBG_RL(LOG_MODULE, &rl, "searching table entry for packet match: %s.", m);
			printf("searching table entry for pkt match\n");
			free(m);
		}

		entry = flow_table_lookup(table, pkt);

        if (entry != NULL) {
	        if (VLOG_IS_DBG_ENABLED(LOG_MODULE)) {
                char *m = ofl_structs_flow_stats_to_string(entry->stats, pkt->dp->exp);
                VLOG_DBG_RL(LOG_MODULE, &rl, "found matching entry: %s.", m);
                free(m);
		printf("find matching entry\n");
            } 

            pkt->handle_std->table_miss = is_table_miss(entry);
            execute_entry(pl, entry, &next_table, &pkt);
            /* Packet could be destroyed by a meter instruction */
            if (!pkt)
                return;

            if (next_table == NULL) {
               /* Cookie field is set 0xffffffffffffffff
                because we cannot associate it to any
                particular flow */
                action_set_execute(pkt->action_set, pkt, 0xffffffffffffffff);
                packet_destroy(pkt);
                return;
            }

        } else {
			/* OpenFlow 1.3 default behavior on a table miss */
			VLOG_DBG_RL(LOG_MODULE, &rl, "No matching entry found. Dropping packet.");
			packet_destroy(pkt);
			printf("No matching entry found. Dropping packet.\n");
			return;
        }
    }
    VLOG_WARN_RL(LOG_MODULE, &rl, "Reached outside of pipeline processing cycle.");
}

static
int inst_compare(const void *inst1, const void *inst2){
    printf("here is comparing priority instructions\n");
    struct ofl_instruction_header * i1 = *(struct ofl_instruction_header **) inst1;
    struct ofl_instruction_header * i2 = *(struct ofl_instruction_header **) inst2;
    if ((i1->type == OFPIT_APPLY_ACTIONS && i2->type == OFPIT_CLEAR_ACTIONS) ||
        (i1->type == OFPIT_CLEAR_ACTIONS && i2->type == OFPIT_APPLY_ACTIONS))
        return i1->type > i2->type;

    return i1->type < i2->type;
}

ofl_err
pipeline_handle_state_mod(struct pipeline *pl, struct ofl_msg_state_mod *msg,
                                                const struct sender *sender) {
    printf("here is handle state mod func\n");
    ofl_err error;
	struct state_table *st = pl->tables[msg->table_id]->state_table;
//	int update;

	if (msg->command == OFPSC_SET_L_EXTRACTOR || msg->command == OFPSC_SET_U_EXTRACTOR) {
		struct ofl_msg_extraction *p = (struct ofl_msg_extraction *) msg->payload;	
		int update=0;
		if (msg->command == OFPSC_SET_U_EXTRACTOR) 
			update = 1;
		state_table_set_extractor(st, (struct key_extractor *)p, update);
	}
	else if (msg->command == OFPSC_ADD_FLOW_STATE) {
		struct ofl_msg_state_entry *p = (struct ofl_msg_state_entry *) msg->payload;
		
		state_table_set_state(st, NULL, p->state, p->key, p->key_len);
	}
	else if (msg->command == OFPSC_DEL_FLOW_STATE) {
		struct ofl_msg_state_entry *p = (struct ofl_msg_state_entry *) msg->payload;
		state_table_del_state(st, p->key, p->key_len);
	}
	else
		return 1;

	return 0;
}

ofl_err
pipeline_handle_flow_mod(struct pipeline *pl, struct ofl_msg_flow_mod *msg,
                                                const struct sender *sender) {
    printf("here is handle flow mod func\n");
    /* Note: the result of using table_id = 0xff is undefined in the spec.
     *       for now it is accepted for delete commands, meaning to delete
     *       from all tables */
    ofl_err error;
    size_t i;
    bool match_kept,insts_kept;

    if(sender->remote->role == OFPCR_ROLE_SLAVE)
        return ofl_error(OFPET_BAD_REQUEST, OFPBRC_IS_SLAVE);

    match_kept = false;
    insts_kept = false;

    /*Sort by execution oder*/
    qsort(msg->instructions, msg->instructions_num,
        sizeof(struct ofl_instruction_header *), inst_compare);

    // Validate actions in flow_mod
    for (i=0; i< msg->instructions_num; i++) {
        if (msg->instructions[i]->type == OFPIT_APPLY_ACTIONS ||
            msg->instructions[i]->type == OFPIT_WRITE_ACTIONS) {
            struct ofl_instruction_actions *ia = (struct ofl_instruction_actions *)msg->instructions[i];

            error = dp_actions_validate(pl->dp, ia->actions_num, ia->actions);
            if (error) {
                return error;
            }
            error = dp_actions_check_set_field_req(msg, ia->actions_num, ia->actions);
            if (error) {
                return error;
            }
        }
    }

    if (msg->table_id == 0xff) {
        if (msg->command == OFPFC_DELETE || msg->command == OFPFC_DELETE_STRICT) {
            size_t i;

            error = 0;
            for (i=0; i < PIPELINE_TABLES; i++) {
                error = flow_table_flow_mod(pl->tables[i], msg, &match_kept, &insts_kept);
                if (error) {
                    break;
                }
            }
            if (error) {
                return error;
            } else {
                ofl_msg_free_flow_mod(msg, !match_kept, !insts_kept, pl->dp->exp);
                return 0;
            }
        } else {
            return ofl_error(OFPET_FLOW_MOD_FAILED, OFPFMFC_BAD_TABLE_ID);
        }
    } else {
        error = flow_table_flow_mod(pl->tables[msg->table_id], msg, &match_kept, &insts_kept);
        if (error) {
            return error;
        }
        if ((msg->command == OFPFC_ADD || msg->command == OFPFC_MODIFY || msg->command == OFPFC_MODIFY_STRICT) &&
                            msg->buffer_id != NO_BUFFER) {
            /* run buffered message through pipeline */
            struct packet *pkt;

            pkt = dp_buffers_retrieve(pl->dp->buffers, msg->buffer_id);
            if (pkt != NULL) {
		      pipeline_process_packet(pl, pkt);
            } else {
                VLOG_WARN_RL(LOG_MODULE, &rl, "The buffer flow_mod referred to was empty (%u).", msg->buffer_id);
            }
        }

        ofl_msg_free_flow_mod(msg, !match_kept, !insts_kept, pl->dp->exp);
        return 0;
    }

}

ofl_err
pipeline_handle_table_mod(struct pipeline *pl,
                          struct ofl_msg_table_mod *msg,
                          const struct sender *sender) {

    printf("here is handle table mod func\n");
    if(sender->remote->role == OFPCR_ROLE_SLAVE)
        return ofl_error(OFPET_BAD_REQUEST, OFPBRC_IS_SLAVE);

    if (msg->table_id == 0xff) {
        size_t i;

        for (i=0; i<PIPELINE_TABLES; i++) {
            pl->tables[i]->features->config = msg->config;
        }
    } else {
        pl->tables[msg->table_id]->features->config = msg->config;
    }

    ofl_msg_free((struct ofl_msg_header *)msg, pl->dp->exp);
    return 0;
}

ofl_err
pipeline_handle_stats_request_flow(struct pipeline *pl,
                                   struct ofl_msg_multipart_request_flow *msg,
                                   const struct sender *sender) {

    printf("here is handle statistic request flow\n");
    struct ofl_flow_stats **stats = xmalloc(sizeof(struct ofl_flow_stats *));
    size_t stats_size = 1;
    size_t stats_num = 0;

    if (msg->table_id == 0xff) {
        size_t i;
        for (i=0; i<PIPELINE_TABLES; i++) {
            flow_table_stats(pl->tables[i], msg, &stats, &stats_size, &stats_num);
        }
    } else {
        flow_table_stats(pl->tables[msg->table_id], msg, &stats, &stats_size, &stats_num);
    }

    {
        struct ofl_msg_multipart_reply_flow reply =
                {{{.type = OFPT_MULTIPART_REPLY},
                  .type = OFPMP_FLOW, .flags = 0x0000},
                 .stats     = stats,
                 .stats_num = stats_num
                };

        dp_send_message(pl->dp, (struct ofl_msg_header *)&reply, sender);
    }

    free(stats);
    ofl_msg_free((struct ofl_msg_header *)msg, pl->dp->exp);
    return 0;
}

ofl_err
pipeline_handle_stats_request_table(struct pipeline *pl,
                                    struct ofl_msg_multipart_request_header *msg UNUSED,
                                    const struct sender *sender) {
    printf("here is handle statistic request table\n");
    struct ofl_table_stats **stats;
    size_t i;

    stats = xmalloc(sizeof(struct ofl_table_stats *) * PIPELINE_TABLES);

    for (i=0; i<PIPELINE_TABLES; i++) {
        stats[i] = pl->tables[i]->stats;
    }

    {
        struct ofl_msg_multipart_reply_table reply =
                {{{.type = OFPT_MULTIPART_REPLY},
                  .type = OFPMP_TABLE, .flags = 0x0000},
                 .stats     = stats,
                 .stats_num = PIPELINE_TABLES};

        dp_send_message(pl->dp, (struct ofl_msg_header *)&reply, sender);
    }

    free(stats);
    ofl_msg_free((struct ofl_msg_header *)msg, pl->dp->exp);
    return 0;
}

ofl_err
pipeline_handle_stats_request_table_features_request(struct pipeline *pl,
                                    struct ofl_msg_multipart_request_header *msg,
                                    const struct sender *sender) {
    printf("here is handle statistic request table feature\n");
    size_t i, j;
    struct ofl_table_features **features;
    struct ofl_msg_multipart_request_table_features *feat =
                       (struct ofl_msg_multipart_request_table_features *) msg;

    /* Further validation of request not done in
     * ofl_structs_table_features_unpack(). Jean II */
    if(feat->table_features != NULL) {
        for(i = 0; i < feat->tables_num; i++){
	    if(feat->table_features[i]->table_id >= PIPELINE_TABLES)
	        return ofl_error(OFPET_TABLE_FEATURES_FAILED, OFPTFFC_BAD_TABLE);
	    /* We may want to validate things like config, max_entries,
	     * metadata... */
        }
    }

    /* Check if we already received fragments of a multipart request. */
    if(sender->remote->mp_req_msg != NULL) {
      bool nomore;

      /* We can only merge requests having the same XID. */
      if(sender->xid != sender->remote->mp_req_xid)
	{
	  VLOG_ERR(LOG_MODULE, "multipart request: wrong xid (0x%X != 0x%X)", sender->xid, sender->remote->mp_req_xid);

	  /* Technically, as our buffer can only hold one pending request,
	   * this is a buffer overflow ! Jean II */
	  /* Return error. */
	  return ofl_error(OFPET_BAD_REQUEST, OFPBRC_MULTIPART_BUFFER_OVERFLOW);
	}

      VLOG_DBG(LOG_MODULE, "multipart request: merging with previous fragments (%d+%d)", ((struct ofl_msg_multipart_request_table_features *) sender->remote->mp_req_msg)->tables_num, feat->tables_num);

      /* Merge the request with previous fragments. */
      nomore = ofl_msg_merge_multipart_request_table_features((struct ofl_msg_multipart_request_table_features *) sender->remote->mp_req_msg, feat);

      /* Check if incomplete. */
      if(!nomore)
	return 0;

      VLOG_DBG(LOG_MODULE, "multipart request: reassembly complete (%d)", ((struct ofl_msg_multipart_request_table_features *) sender->remote->mp_req_msg)->tables_num);

      /* Use the complete request. */
      feat = (struct ofl_msg_multipart_request_table_features *) sender->remote->mp_req_msg;

#if 0
      {
	char *str;
	str = ofl_msg_to_string((struct ofl_msg_header *) feat, pl->dp->exp);
	VLOG_DBG(LOG_MODULE, "\nMerged request:\n%s\n\n", str);
	free(str);
      }
#endif

    } else {
      /* Check if the request is an initial fragment. */
      if(msg->flags & OFPMPF_REQ_MORE) {
	struct ofl_msg_multipart_request_table_features* saved_msg;

	VLOG_DBG(LOG_MODULE, "multipart request: create reassembly buffer (%d)", feat->tables_num);

	/* Create a buffer the do reassembly. */
	saved_msg = (struct ofl_msg_multipart_request_table_features*) malloc(sizeof(struct ofl_msg_multipart_request_table_features));
	saved_msg->header.header.type = OFPT_MULTIPART_REQUEST;
	saved_msg->header.type = OFPMP_TABLE_FEATURES;
	saved_msg->header.flags = 0;
	saved_msg->tables_num = 0;
	saved_msg->table_features = NULL;

	/* Save the fragment for later use. */
	ofl_msg_merge_multipart_request_table_features(saved_msg, feat);
	sender->remote->mp_req_msg = (struct ofl_msg_multipart_request_header *) saved_msg;
	sender->remote->mp_req_xid = sender->xid;

	return 0;
      }

      /* Non fragmented request. Nothing to do... */
      VLOG_DBG(LOG_MODULE, "multipart request: non-fragmented request (%d)", feat->tables_num);
    }

    /*Check to see if the body is empty.*/
    /* Should check merge->tables_num instead. Jean II */
    if(feat->table_features != NULL){
        /* Disable all tables, they will be selectively re-enabled. */
        for(i = 0; i < PIPELINE_TABLES; i++){
	    pl->tables[i]->disabled = true;
	}
        /* Change tables configuration
           TODO: Remove flows*/
        /* TODO : In theory, tables should be in ascending order !
	 * Maybe we could return an error if table number not
	 * expected, like OFPTFFC_BAD_TABLE... Jean II  */
        VLOG_DBG(LOG_MODULE, "pipeline_handle_stats_request_table_features_request: updating features");
        for(i = 0; i < feat->tables_num; i++){
	    /* Obvious memory leak.
	     * Obvious memory ownership issue when non-frag requests.
	     * Jean II */
            pl->tables[feat->table_features[i]->table_id]->features = feat->table_features[i];
	    pl->tables[i]->disabled = false;
        }
    }

    /* Cleanup request. */
    if(sender->remote->mp_req_msg != NULL) {
      /* Can't free entire structure, we are pointing to it ! */
      //ofl_msg_free((struct ofl_msg_header *) sender->remote->mp_req_msg, NULL);
      free(sender->remote->mp_req_msg);
      sender->remote->mp_req_msg = NULL;
      sender->remote->mp_req_xid = 0;  /* Currently not needed. Jean II. */
    }

    j = 0;
    /* Query for table capabilities */
    loop: ;
    features = (struct ofl_table_features**) xmalloc(sizeof(struct ofl_table_features *) * 8);
    /* Return 8 tables per reply segment. */
    for (i = 0; i < 8; i++){
        /* Skip disabled tables. */
        while((j < PIPELINE_TABLES) && (pl->tables[j]->disabled == true))
	    j++;
	/* Stop at the last table. */
	if(j >= PIPELINE_TABLES)
	    break;
	/* Use that table in the reply. */
        features[i] = pl->tables[j]->features;
        j++;
    }
    VLOG_DBG(LOG_MODULE, "multipart reply: returning %d tables, next table-id %d", i, j);
    {
    struct ofl_msg_multipart_reply_table_features reply =
         {{{.type = OFPT_MULTIPART_REPLY},
           .type = OFPMP_TABLE_FEATURES,
           .flags = (j == PIPELINE_TABLES ? 0x00000000 : OFPMPF_REPLY_MORE) },
          .table_features     = features,
          .tables_num = i };
          dp_send_message(pl->dp, (struct ofl_msg_header *)&reply, sender);
    }
    if (j < PIPELINE_TABLES){
           goto loop;
    }

    return 0;
}

ofl_err
pipeline_handle_stats_request_aggregate(struct pipeline *pl,
                                  struct ofl_msg_multipart_request_flow *msg,
                                  const struct sender *sender) {
    printf("here is handle statistic request aggregatation\n");
    struct ofl_msg_multipart_reply_aggregate reply =
            {{{.type = OFPT_MULTIPART_REPLY},
              .type = OFPMP_AGGREGATE, .flags = 0x0000},
              .packet_count = 0,
              .byte_count   = 0,
              .flow_count   = 0};

    if (msg->table_id == 0xff) {
        size_t i;

        for (i=0; i<PIPELINE_TABLES; i++) {
            flow_table_aggregate_stats(pl->tables[i], msg,
                                       &reply.packet_count, &reply.byte_count, &reply.flow_count);
        }

    } else {
        flow_table_aggregate_stats(pl->tables[msg->table_id], msg,
                                   &reply.packet_count, &reply.byte_count, &reply.flow_count);
    }

    dp_send_message(pl->dp, (struct ofl_msg_header *)&reply, sender);

    ofl_msg_free((struct ofl_msg_header *)msg, pl->dp->exp);
    return 0;
}


void
pipeline_destroy(struct pipeline *pl) {
    struct flow_table *table;
    int i;

    printf("here is handle destroy pipeline\n");
    for (i=0; i<PIPELINE_TABLES; i++) {
        table = pl->tables[i];
        if (table != NULL) {
            flow_table_destroy(table);
        }
    }
    free(pl);
}


void
pipeline_timeout(struct pipeline *pl) {
    int i;

    printf("here is handle timeout pipeline\n");
    for (i = 0; i < PIPELINE_TABLES; i++) {
        flow_table_timeout(pl->tables[i]);
    }
}


/* Executes the instructions associated with a flow entry */
static void
execute_entry(struct pipeline *pl, struct flow_entry *entry,
              struct flow_table **next_table, struct packet **pkt) {
    /* NOTE: instructions, when present, will be executed in
            the following order:
            Meter
            Apply-Actions
            Clear-Actions
            Write-Actions
            Write-Metadata
            Goto-Table
    */
    printf("here is handle execution of intruction for each flow entry\n");
    size_t i;
    struct ofl_instruction_header *inst;

    for (i=0; i < entry->stats->instructions_num; i++) {
        /*Packet was dropped by some instruction or action*/

        if(!(*pkt)){
            return;
        }

        inst = entry->stats->instructions[i];
        switch (inst->type) {
            case OFPIT_GOTO_TABLE: {
                struct ofl_instruction_goto_table *gi = (struct ofl_instruction_goto_table *)inst;

                *next_table = pl->tables[gi->table_id];
                break;
            }
            case OFPIT_WRITE_METADATA: {
                struct ofl_instruction_write_metadata *wi = (struct ofl_instruction_write_metadata *)inst;
                struct  ofl_match_tlv *f;

                /* NOTE: Hackish solution. If packet had multiple handles, metadata
                 *       should be updated in all. */
                packet_handle_std_validate((*pkt)->handle_std);
                /* Search field on the description of the packet. */
                HMAP_FOR_EACH_WITH_HASH(f, struct ofl_match_tlv,
                    hmap_node, hash_int(OXM_OF_METADATA,0), &(*pkt)->handle_std->match.match_fields){
                    uint64_t *metadata = (uint64_t*) f->value;
                    *metadata = (*metadata & ~wi->metadata_mask) | (wi->metadata & wi->metadata_mask);
                    VLOG_DBG_RL(LOG_MODULE, &rl, "Executing write metadata: %llx", *metadata);
                }
                break;
            }
            case OFPIT_WRITE_ACTIONS: {
                struct ofl_instruction_actions *wa = (struct ofl_instruction_actions *)inst;
                action_set_write_actions((*pkt)->action_set, wa->actions_num, wa->actions);
                break;
            }
            case OFPIT_APPLY_ACTIONS: {
                struct ofl_instruction_actions *ia = (struct ofl_instruction_actions *)inst;
                dp_execute_action_list((*pkt), ia->actions_num, ia->actions, entry->stats->cookie);
                break;
            }
            case OFPIT_CLEAR_ACTIONS: {
                action_set_clear_actions((*pkt)->action_set);
                break;
            }
            case OFPIT_METER: {
            	struct ofl_instruction_meter *im = (struct ofl_instruction_meter *)inst;
                meter_table_apply(pl->dp->meters, pkt, im->meter_id);
                break;
            }
            case OFPIT_EXPERIMENTER: {
                dp_exp_inst((*pkt), (struct ofl_instruction_experimenter *)inst);
                break;
            }
        }
    }
}
