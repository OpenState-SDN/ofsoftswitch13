/* Copyright (c) 2011, TrafficLab, Ericsson Research, Hungary
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
 *
 * Author: Zoltán Lajos Kis <zoltan.lajos.kis@ericsson.com>
 */

#include <stdlib.h>
#include <string.h>
#include "datapath.h"
#include "dp_exp.h"
#include "packet.h"
#include "oflib/ofl.h"
#include "oflib/ofl-actions.h"
#include "oflib/ofl-structs.h"
#include "oflib/ofl-messages.h"
#include "oflib-exp/ofl-exp-openflow.h"
#include "oflib-exp/ofl-exp-nicira.h"
#include "oflib-exp/ofl-exp-openstate.h"
#include "openflow/openflow.h"
#include "openflow/openflow-ext.h"
#include "openflow/nicira-ext.h"
#include "openflow/openstate-ext.h"
#include "vlog.h"
#include "pipeline.h"

#define LOG_MODULE VLM_dp_exp

static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(60, 60);

void
dp_exp_action(struct packet *pkt, struct ofl_action_experimenter *act) {
    
    if(act->experimenter_id == OPENSTATE_VENDOR_ID)
    {
        struct ofl_exp_openstate_act_header *action;
        action = (struct ofl_exp_openstate_act_header *) act;
        switch(action->act_type){

            case(OFPAT_EXP_SET_STATE):
            {
                struct ofl_exp_action_set_state *wns = (struct ofl_exp_action_set_state *)action;
                if (state_table_is_stateful(pkt->dp->pipeline->tables[wns->table_id]->state_table) && state_table_is_configured(pkt->dp->pipeline->tables[wns->table_id]->state_table))
                {
                    struct state_table *st = pkt->dp->pipeline->tables[wns->table_id]->state_table;
                    VLOG_DBG_RL(LOG_MODULE, &rl, "executing action NEXT STATE at stage %u", wns->table_id);
                    state_table_set_state(st, pkt, NULL, wns);
                }
                else
                {
                    VLOG_WARN_RL(LOG_MODULE, &rl, "ERROR NEXT STATE at stage %u: stage not stateful", wns->table_id);
                }
                break;
            }
            case (OFPAT_EXP_SET_GLOBAL_STATE): 
            {
                struct ofl_exp_action_set_global_state *wns = (struct ofl_exp_action_set_global_state *)action;
                uint32_t global_state = pkt->dp->global_state;
                
                global_state = (global_state & ~(wns->global_state_mask)) | (wns->global_state & wns->global_state_mask);
                pkt->dp->global_state = global_state;
                 break;
            }  
            default:
                VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to execute unknown experimenter action (%zu).", htonl(act->experimenter_id));
                break;
        }
        if (VLOG_IS_DBG_ENABLED(LOG_MODULE)) {
            char *p = packet_to_string(pkt);
            VLOG_DBG_RL(LOG_MODULE, &rl, "action result: %s", p);
            free(p);
        }
    }
}

void
dp_exp_inst(struct packet *pkt UNUSED, struct ofl_instruction_experimenter *inst) {
	VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to execute unknown experimenter instruction (%u).", inst->experimenter_id);
}

ofl_err
dp_exp_stats(struct datapath *dp UNUSED, struct ofl_msg_multipart_request_experimenter *msg, const struct sender *sender UNUSED) {
    ofl_err err;
    switch (msg->experimenter_id) {
        case (OPENSTATE_VENDOR_ID): {
            struct ofl_exp_openstate_msg_multipart_request *exp = (struct ofl_exp_openstate_msg_multipart_request *)msg;

            switch(exp->type) {
                case (OFPMP_EXP_STATE_STATS): {
                    struct ofl_exp_msg_multipart_reply_state reply;
                    err = handle_stats_request_state(dp->pipeline, (struct ofl_exp_msg_multipart_request_state *)msg, sender, &reply);
                    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);
                    free(reply.stats);
                    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
                    return err;
                }
                case (OFPMP_EXP_STATE_STATS_NUM): {
                    struct ofl_exp_msg_multipart_reply_state_num reply;
                    err = handle_stats_request_state_num(dp->pipeline, (struct ofl_exp_msg_multipart_request_state_num *)msg, sender, &reply);
                    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);
                    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
                    return err;
                }
                case (OFPMP_EXP_GLOBAL_STATE_STATS): {
                    struct ofl_exp_msg_multipart_reply_global_state reply;
                    err = handle_stats_request_global_state(dp->pipeline, sender, &reply);
                    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);
                    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
                    return err;
                }
                default: {
                    VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to handle unknown experimenter type (%u).", exp->type);
                    return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
                }
            }
        }
        default: {
            VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to handle unknown experimenter stats (%u).", msg->experimenter_id);
            return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
        }
    }
}


ofl_err
dp_exp_message(struct datapath *dp, struct ofl_msg_experimenter *msg, const struct sender *sender) {

    switch (msg->experimenter_id) {
        case (OPENFLOW_VENDOR_ID): {
            struct ofl_exp_openflow_msg_header *exp = (struct ofl_exp_openflow_msg_header *)msg;

            switch(exp->type) {
                case (OFP_EXT_QUEUE_MODIFY): {
                    return dp_ports_handle_queue_modify(dp, (struct ofl_exp_openflow_msg_queue *)msg, sender);
                }
                case (OFP_EXT_QUEUE_DELETE): {
                    return dp_ports_handle_queue_delete(dp, (struct ofl_exp_openflow_msg_queue *)msg, sender);
                }
                case (OFP_EXT_SET_DESC): {
                    return dp_handle_set_desc(dp, (struct ofl_exp_openflow_msg_set_dp_desc *)msg, sender);
                }
                default: {
                	VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to handle unknown experimenter type (%u).", exp->type);
                    return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
                }
            }
        }
        case (OPENSTATE_VENDOR_ID): {
            struct ofl_exp_openstate_msg_header *exp = (struct ofl_exp_openstate_msg_header *)msg;
            switch(exp->type) {
                case (OFPT_EXP_STATE_MOD): {
                    return handle_state_mod(dp->pipeline, (struct ofl_exp_msg_state_mod *)msg, sender);
                    }
                default: {
                    VLOG_WARN_RL(LOG_MODULE, &rl, "Trying to handle unknown experimenter type (%u).", exp->type);
                    return ofl_error(OFPET_EXPERIMENTER, OFPEC_BAD_EXP_MESSAGE);
                }
            }
        }
        default: {
            return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
        }
    }
}
