#ifndef OFL_EXP_OPENSTATE_H
#define OFL_EXP_OPENSTATE_H 1

#include "../lib/hmap.h"
#include "../udatapath/packet.h"
#include "../udatapath/pipeline.h"
#include "../oflib/ofl-structs.h"
#include "../oflib/ofl-messages.h"
#include "../include/openflow/openstate-ext.h"


#define MAX_EXTRACTION_FIELD_COUNT 6
#define MAX_STATE_KEY_LEN 48

#define STATE_DEFAULT 0
/**************************************************************************/
/*                        experimenter messages ofl_exp                   */
/**************************************************************************/
struct ofl_exp_openstate_msg_header {
    struct ofl_msg_experimenter   header; /* OPENSTATE_VENDOR_ID */

    uint32_t   type;
};

struct ofl_exp_openstate_msg_multipart_request {
    struct ofl_msg_multipart_request_experimenter header; /* OPENSTATE_VENDOR_ID */

    uint32_t type;
};

struct ofl_exp_openstate_msg_multipart_reply {
    struct ofl_msg_multipart_reply_experimenter header; /* OPENSTATE_VENDOR_ID */

    uint32_t type;
};
/************************
 * state mod messages
 ************************/

struct ofl_exp_msg_state_mod {
    struct ofl_exp_openstate_msg_header header;   /* OFP_EXP_STATE_MOD */
    enum ofp_exp_msg_state_mod_commands command;
    uint8_t payload[13+OFPSC_MAX_KEY_LEN]; //ugly! for now it's ok XXX
};

struct ofl_exp_stateful_table_config {
    uint8_t table_id;
    uint8_t stateful;
};

struct ofl_exp_set_extractor {
    uint8_t table_id;
    uint32_t field_count;
    uint32_t fields[OFPSC_MAX_FIELD_COUNT];
};

struct ofl_exp_set_flow_state {
    uint8_t table_id;
    uint32_t key_len;
    uint32_t state;
    uint32_t state_mask;
    uint32_t hard_rollback;
    uint32_t idle_rollback;
    uint32_t hard_timeout;
    uint32_t idle_timeout;
    uint8_t key[OFPSC_MAX_KEY_LEN];
};

struct ofl_exp_del_flow_state {
    uint8_t table_id;
    uint32_t key_len;
    uint8_t key[OFPSC_MAX_KEY_LEN];
};

struct ofl_exp_set_global_state {
    uint32_t global_state;
    uint32_t global_state_mask;
};

/*************************
* Multipart reply message: State entry statistics
*************************/
struct ofl_exp_state_entry{
    uint32_t            key_len;
    uint8_t             key[OFPSC_MAX_KEY_LEN];
    uint32_t            state;
};

struct ofl_exp_state_stats {
    uint8_t                         table_id;      /* ID of table flow came from. */
    uint32_t                        duration_sec;  /* Time state entry has been alive in secs. */
    uint32_t                        duration_nsec; /* Time state entry has been alive in nsecs beyond duration_sec. */
    uint32_t                        field_count;    /*number of extractor fields*/
    uint32_t                        fields[OFPSC_MAX_FIELD_COUNT]; /*extractor fields*/
    uint32_t                        hard_rollback;
    uint32_t                        idle_rollback;
    uint32_t                        hard_timeout;  /* Number of seconds before expiration. */
    uint32_t                        idle_timeout;  /* Number of seconds idle before expiration. */
    struct ofl_exp_state_entry      entry;         /* Description of the state entry. */
};

struct ofl_exp_msg_multipart_request_state {
    struct ofl_exp_openstate_msg_multipart_request   header; /* OFPMP_STATE */

    uint8_t                  table_id; /* ID of table to read
                                           (from ofp_table_multipart), 0xff for all
                                           tables. */
    uint8_t                  get_from_state;
    uint32_t                 state;
    struct ofl_match_header  *match;       /* Fields to match. */
};

struct ofl_exp_msg_multipart_reply_state {

    struct ofl_exp_openstate_msg_multipart_reply   header; /* OFPMP_STATE */

    size_t                  stats_num;
    struct ofl_exp_state_stats **stats;
};

struct ofl_exp_msg_multipart_request_state_num {
    struct ofl_exp_openstate_msg_multipart_request   header; /* OFPMP_STATE_NUM */
    uint8_t                  table_id; /* ID of table to read
                                           (from ofp_table_multipart), 0xff for all
                                           tables. */
};

struct ofl_exp_msg_multipart_reply_state_num {
    struct ofl_exp_openstate_msg_multipart_reply   header; /* OFPMP_STATE_NUM */
    uint32_t count;
};

struct ofl_exp_msg_multipart_request_global_state {
    struct ofl_exp_openstate_msg_multipart_request   header; /* OFPMP_GLOBAL_STATE */
};

struct ofl_exp_msg_multipart_reply_global_state {
    struct ofl_exp_openstate_msg_multipart_reply   header; /* OFPMP_GLOBAL_STATE */
    uint32_t global_state;
};

/*************************************************************************/
/*                        experimenter actions ofl_exp                   */
/*************************************************************************/
struct ofl_exp_openstate_act_header {
    struct ofl_action_experimenter   header; /* OPENSTATE_VENDOR_ID */

    uint32_t   act_type;
};

struct ofl_exp_action_set_state {
    struct ofl_exp_openstate_act_header  header; /* OFPAT_EXP_SET_STATE */

    uint32_t state;
    uint32_t state_mask;
    uint8_t table_id; /*we have 64 flow table in the pipeline*/
    uint32_t hard_rollback;
    uint32_t idle_rollback;
    uint32_t hard_timeout;
    uint32_t idle_timeout;

};

struct ofl_exp_action_set_global_state {
    struct ofl_exp_openstate_act_header   header; /* OFPAT_EXP_SET_GLOBAL_STATE */

    uint32_t global_state;
    uint32_t global_state_mask;
};


/*************************************************************************
 *                        experimenter state table
 *************************************************************************/


struct key_extractor {
    uint8_t                     table_id;
    uint32_t                    field_count;
    uint32_t                    fields[MAX_EXTRACTION_FIELD_COUNT];
};

struct state_entry {
    struct hmap_node            hmap_node;
    struct hmap_node            hard_node;
    struct hmap_node            idle_node;
    uint8_t             key[MAX_STATE_KEY_LEN];
    uint32_t                state;
    struct ofl_exp_state_stats   *stats;
    uint64_t                created;  /* time the entry was created at [us] */
    uint64_t                remove_at; /* time the entry should be removed at
                                           due to its hard timeout. [us] */
    uint64_t                last_used; /* last time the flow entry matched a packet [us]*/
};

struct state_table {
    struct key_extractor        read_key;
    struct key_extractor        write_key;
    struct hmap                 state_entries;
    struct hmap                 hard_entries;
    struct hmap                 idle_entries;
    struct state_entry          default_state_entry;
    uint8_t stateful;
};

/*experimenter table functions*/
struct state_table *
state_table_create(void);

void
state_table_destroy(struct state_table *);

uint8_t
state_table_is_stateful(struct state_table *);

struct state_entry *
state_table_lookup(struct state_table*, struct packet *);

void
state_table_write_state(struct state_entry *, struct packet *);

ofl_err
state_table_set_state(struct state_table *, struct packet *, struct ofl_exp_set_flow_state *msg, struct ofl_exp_action_set_state *act);

ofl_err
state_table_set_extractor(struct state_table *, struct key_extractor *, int);

ofl_err
state_table_del_state(struct state_table *, uint8_t *, uint32_t);

void
state_table_timeout(struct state_table *table);

bool state_table_is_configured(struct state_table *table);

/*experimenter message functions*/

char *
ofl_exp_openstate_msg_to_string(struct ofl_msg_experimenter const *msg);

int
ofl_exp_openstate_msg_pack(struct ofl_msg_experimenter const *msg, uint8_t **buf, size_t *buf_len);

ofl_err
ofl_exp_openstate_msg_unpack(struct ofp_header const *oh, size_t *len, struct ofl_msg_experimenter **msg);

int
ofl_exp_openstate_msg_free(struct ofl_msg_experimenter *msg);

char *
OFl_exp_openstate_msg_to_string(struct ofl_msg_experimenter const *msg);


/*experimenter action functions*/


int
ofl_exp_openstate_act_pack(struct ofl_action_header const *src, struct ofp_action_header *dst);

ofl_err
ofl_exp_openstate_act_unpack(struct ofp_action_header const *src, size_t *len, struct ofl_action_header **dst);

size_t
ofl_exp_openstate_act_ofp_len(struct ofl_action_header const *act);

int
ofl_exp_openstate_act_free(struct ofl_action_header *act);

char *
ofl_exp_openstate_act_to_string(struct ofl_action_header const *act);

/*experimenter stats functions*/
int
ofl_exp_openstate_stats_req_pack(struct ofl_msg_multipart_request_experimenter const *ext, uint8_t **buf, size_t *buf_len, struct ofl_exp const *exp);

int
ofl_exp_openstate_stats_reply_pack(struct ofl_msg_multipart_reply_experimenter const *ext, uint8_t **buf, size_t *buf_len, struct ofl_exp const *exp);

char *
ofl_exp_openstate_stats_request_to_string(struct ofl_msg_multipart_request_experimenter const *ext, struct ofl_exp const *exp);

char *
ofl_exp_openstate_stats_reply_to_string(struct ofl_msg_multipart_reply_experimenter const *ext, struct ofl_exp const *exp);

ofl_err
ofl_exp_openstate_stats_req_unpack(struct ofp_multipart_request const *os, uint8_t const *buf, size_t *len, struct ofl_msg_multipart_request_header **msg, struct ofl_exp const *exp);

ofl_err
ofl_exp_openstate_stats_reply_unpack(struct ofp_multipart_reply const *os, uint8_t const *buf, size_t *len, struct ofl_msg_multipart_reply_header **msg, struct ofl_exp const *exp);

int
ofl_exp_openstate_stats_req_free(struct ofl_msg_multipart_request_header *msg);

int
ofl_exp_openstate_stats_reply_free(struct ofl_msg_multipart_reply_header *msg);

/*experimenter match fields functions*/

int
ofl_exp_openstate_field_unpack(struct ofl_match *match, struct oxm_field const *f, void const *experimenter_id, void const *value, void const *mask);

void
ofl_exp_openstate_field_pack(struct ofpbuf *buf, struct ofl_match_tlv const *oft);

void
ofl_exp_openstate_field_match(struct ofl_match_tlv *f, int *packet_header, int *field_len, uint8_t **flow_val, uint8_t **flow_mask);

void
ofl_exp_openstate_field_compare (struct ofl_match_tlv *value, uint8_t **packet_val);

void
ofl_exp_openstate_field_match_std (struct ofl_match_tlv *flow_mod_match, struct ofl_match_tlv *flow_entry_match, int *field_len, uint8_t **flow_mod_val, uint8_t **flow_entry_val, uint8_t **flow_mod_mask, uint8_t **flow_entry_mask);

void
ofl_exp_openstate_field_overlap_a (struct ofl_match_tlv *f_a, int *field_len, uint8_t **val_a, uint8_t **mask_a, int *header, int *header_m, uint64_t *all_mask);

void
ofl_exp_openstate_field_overlap_b (struct ofl_match_tlv *f_b, int *field_len, uint8_t **val_b, uint8_t **mask_b, uint64_t *all_mask);

void
ofl_exp_openstate_error_pack (struct ofl_msg_exp_error const *msg, uint8_t **buf, size_t *buf_len);

void
ofl_exp_openstate_error_free (struct ofl_msg_exp_error *msg);

char *
ofl_exp_openstate_error_to_string(struct ofl_msg_exp_error const *msg);

/* Handles a state_mod message */
ofl_err
handle_state_mod(struct pipeline *pl, struct ofl_exp_msg_state_mod *msg, const struct sender *sender);

/* Handles a state stats request. */
ofl_err
handle_stats_request_state(struct pipeline *pl, struct ofl_exp_msg_multipart_request_state *msg, const struct sender *sender, struct ofl_exp_msg_multipart_reply_state *reply);

ofl_err
handle_stats_request_state_num(struct pipeline *pl, struct ofl_exp_msg_multipart_request_state_num *msg, const struct sender *sender, struct ofl_exp_msg_multipart_reply_state_num *reply);

/* Handles a global state stats request. */
ofl_err
handle_stats_request_global_state(struct pipeline *pl, const struct sender *sender, struct ofl_exp_msg_multipart_reply_global_state *reply);

void
state_table_stats(struct state_table *table, struct ofl_exp_msg_multipart_request_state *msg,
                 struct ofl_exp_state_stats ***stats, size_t *stats_size, size_t *stats_num, uint8_t table_id);

size_t
ofl_structs_state_stats_pack(struct ofl_exp_state_stats const *src, uint8_t *dst, struct ofl_exp const *exp);

size_t
ofl_structs_state_stats_ofp_total_len(struct ofl_exp_state_stats ** stats, size_t stats_num, struct ofl_exp const *exp);

size_t
ofl_structs_state_stats_ofp_len(struct ofl_exp_state_stats *stats, struct ofl_exp const *exp);

void
ofl_structs_state_entry_print(FILE *stream, uint32_t field, uint8_t *key, uint8_t *offset);

void
ofl_structs_state_entry_print_default(FILE *stream, uint32_t field);

void
ofl_structs_state_stats_print(FILE *stream, struct ofl_exp_state_stats *s, struct ofl_exp const *exp);

ofl_err
ofl_structs_state_stats_unpack(struct ofp_exp_state_stats const *src, uint8_t const *buf, size_t *len, struct ofl_exp_state_stats **dst, struct ofl_exp const *exp);

ofl_err
ofl_utils_count_ofp_state_stats(void *data, size_t data_len, size_t *count);

void
ofl_exp_stats_type_print(FILE *stream, uint32_t type);

void
ofl_structs_match_exp_put8(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint8_t value);

void
ofl_structs_match_exp_put8m(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint8_t value, uint8_t mask);

void
ofl_structs_match_exp_put16(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint16_t value);

void
ofl_structs_match_exp_put16m(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint16_t value, uint16_t mask);

void
ofl_structs_match_exp_put32(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint32_t value);

void
ofl_structs_match_exp_put32m(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint32_t value, uint32_t mask);

void
ofl_structs_match_exp_put64(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint64_t value);

void
ofl_structs_match_exp_put64m(struct ofl_match *match, uint32_t header, uint32_t experimenter_id, uint64_t value, uint64_t mask);

uint32_t
get_experimenter_id(struct ofl_msg_header const *msg);

uint32_t
get_experimenter_id_from_match(struct ofl_match const *flow_mod_match);

uint32_t
get_experimenter_id_from_action(struct ofl_instruction_actions const *act);

#endif /* OFL_EXP_OPENSTATE_H */
