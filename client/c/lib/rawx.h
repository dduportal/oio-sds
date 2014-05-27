#ifndef __CHUNKINFO_NEON_SESSION_H__
# define __CHUNKINFO_NEON_SESSION_H__
# include "./gs_internals.h"

#define RAWX_ATTR_CHUNK_POSITION "chunkpos"
#define RAWX_ATTR_CONTENT_CHUNKNB "chunknb"
#define RAWX_ATTR_CONTENT_SIZE "contentsize"

typedef struct ne_request_param_s {
	ne_session *session;
	const char *method;
	char *cPath;
	const char *containerid;
	const char *contentpath;
	chunk_position_t chunkpos;
	guint32 chunknb;
	chunk_size_t chunksize;
	int64_t contentsize;
} ne_request_param_t;

/**
 * Chunk
 */
struct chunk_attr_s {
	const char *key;
	const char *val;
};

/* delete one remote chunk */
gs_status_t rawx_delete (gs_chunk_t *chunk, GError **err);

/**
 * Delete a chunk from a RawX
 *
 * @param chunk a pointer to a (struct bean_CHUNKS_s*) or (struct bean_CONTENTS_s*)
 * @param err a pointer to a GError (must not be NULL)
 * @return FALSE in case of error
 */
gboolean rawx_delete_v2(gpointer chunk, GError **err);

/*  */
gs_status_t rawx_upload_v2 (gs_chunk_t *chunk, GError **err,
		gs_input_f input, void *user_data, GByteArray *user_metadata,
		GByteArray *system_metadata, gboolean process_md5);

/*  */
gs_status_t rawx_upload (gs_chunk_t *chunk, GError **err,
		gs_input_f input, void *user_data, GByteArray *system_metadata,
		gboolean process_md5);

void clean_after_upload(void *user_data);
void finalize_content_hash(void);
content_hash_t *get_content_hash(void);

/*  */
gboolean rawx_download (gs_chunk_t *chunk, GError **err,
		struct dl_status_s *status, GSList **p_broken_rawx_list);
int rawx_init (void);

gboolean rawx_update_chunk_attr(struct meta2_raw_chunk_s *c, const char *name,
		const char *val, GError **err);

/**
 * Update chunk extended attributes.
 *
 * @param url The URL of the chunk (a.k.a "chunk id")
 * @param attrs A list of (struct chunk_attr_s *)
 * @param err
 * @return TRUE on success, FALSE otherwise
 */
gboolean rawx_update_chunk_attrs(const gchar *chunk_url, GSList *attrs,
		GError **err);

ne_request_param_t* new_request_param(void);
void free_request_param(ne_request_param_t *param);

char* create_rawx_request_common(ne_request **req, ne_request_param_t *param,
		GError **err);
char* create_rawx_request_from_chunk(ne_request **req, ne_session *session,
		const char *method, gs_chunk_t *chunk, GByteArray *system_metadata,
		GError **err);

ne_session *opensession_common(const addr_info_t *addr_info,
		int connect_timeout, int read_timeout, GError **err);

#endif /*__CHUNKINFO_NEON_SESSION_H__*/
