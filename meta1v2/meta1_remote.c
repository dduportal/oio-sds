/*
OpenIO SDS meta1v2
Copyright (C) 2014 Worldine, original work as part of Redcurrant
Copyright (C) 2015 OpenIO, modified as part of OpenIO Software Defined Storage

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.
*/

#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "meta1.remote"
#endif

#include <metautils/lib/metautils.h>

#include "./internals.h"
#include "./meta1_remote.h"

static void
meta1_container_request_common_v2 (MESSAGE m, const gchar *op, const container_id_t id,
		const gchar *name, const gchar *vns)
{
	gsize nameSize;
	gsize vnsSize;

	g_assert(m != NULL);

	nameSize = name ? strlen(name) : 0;
	vnsSize = vns ? strlen(vns) : 0;
	message_set_NAME (m, op, strlen(op), NULL);

	if (id!=NULL || name!=NULL) {
		container_id_t usedID;
		if (id)
			memcpy(usedID, id, sizeof(container_id_t));
		else
			meta1_name2hash(usedID, vns, name);
		message_add_field (m, NAME_MSGKEY_CONTAINERID, usedID, sizeof(container_id_t));
	}

	if (name && nameSize>0)
		message_add_field (m, NAME_MSGKEY_CONTAINERNAME, name, nameSize);

	if (vns && vnsSize>0)
		message_add_field (m, NAME_MSGKEY_VIRTUALNAMESPACE, vns, vnsSize);
}

static void
meta1_container_request_common (MESSAGE m, const gchar *op, const container_id_t id,
		const gchar *name)
{
	return meta1_container_request_common_v2(m, op, id, name, NULL);
}

/* M1V1 -------------------------------------------------------------------- */

// TODO remove this as soon as the hunk_checker has been replaced
gboolean 
meta1_remote_create_container_v2 (addr_info_t *meta1, gint ms, GError **err,
		const char *cname, const char *vns, container_id_t cid,
		gdouble to_step, gdouble to_overall, gchar **master)
{
	(void) ms;
	struct gridd_client_s *client = NULL;
	gboolean status = FALSE;

	MESSAGE request = message_create();
	meta1_container_request_common_v2 (request, NAME_MSGNAME_M1_CREATE, cid, cname, vns);
	GByteArray *packed = message_marshall_gba_and_clean(request);

	gchar target[64];
	addr_info_to_string(meta1, target, sizeof(target));
	client = gridd_client_create(target, packed, NULL, NULL);

	if(to_step > 0 && to_overall > 0)
		gridd_client_set_timeout(client, to_step, to_overall);

	gridd_client_start(client);
	if ((*err = gridd_client_loop(client)) != NULL)
		goto end_label;

	if (g_ascii_strcasecmp(target, gridd_client_url(client)) && NULL != master)
		*master = g_strdup(gridd_client_url(client));

	if((*err = gridd_client_error(client)) != NULL)
		goto end_label;

	status = TRUE;

end_label:
	if (packed)
		g_byte_array_unref(packed);
	gridd_client_free(client);
	return status;
}

// TODO to be removed as soon ad the C SDK has been rewriten
struct meta1_raw_container_s* 
meta1_remote_get_container_by_id (struct metacnx_ctx_s *ctx,
		container_id_t container_id, GError **err,
		gdouble to_step, gdouble to_overall)
{
	struct meta1_raw_container_s *raw_container = NULL;
	struct gridd_client_s *client = NULL;

	gboolean on_reply(gpointer c1, MESSAGE reply) {
		void *b = NULL;
		gsize bsize = 0;
		(void) c1;
		if (0 < message_get_BODY(reply, &b, &bsize, NULL)) {
			raw_container = meta1_raw_container_unmarshall(b, bsize, err);
		}
		return TRUE;
	}

	if (!ctx || !container_id) {
		GSETERROR(err,"Invalid parameter (%p %p)", ctx, container_id);
		return NULL;
	}

	MESSAGE request = message_create ();
	meta1_container_request_common (request, NAME_MSGNAME_M1_CONT_BY_ID, container_id, NULL);
	GByteArray *packed = message_marshall_gba_and_clean(request);

	gchar target[64];
	addr_info_to_string(&(ctx->addr), target, sizeof(target));
	client = gridd_client_create(target, packed, NULL, on_reply);

	if (to_step > 0 && to_overall > 0)
		gridd_client_set_timeout(client, to_step, to_overall);
	gridd_client_start(client);
	if ((*err = gridd_client_loop(client)) != NULL)
		goto end_label;

	do{
		struct gridd_client_s *clients[2];
		clients[0] = client;
		clients[1] = NULL;
		if((*err = gridd_clients_error(clients)) != NULL)
			goto end_label;
	} while(0);

end_label:
	g_byte_array_unref(packed);
	gridd_client_free(client);
	return(raw_container);
}

/* M1V2 -------------------------------------------------------------------- */

static gboolean
on_reply(gpointer ctx, MESSAGE reply)
{
	GByteArray *out = ctx;
	void *b = NULL;
	gsize bsize = 0;

	if (0 < message_get_BODY(reply, &b, &bsize, NULL)) {
		if (out != NULL)
			g_byte_array_append(out, b, bsize);
	}

	g_byte_array_append(out, (const guint8*)"", 1);
	g_byte_array_set_size(out, out->len - 1);
	return TRUE;
}

static gchar **
list_request(const addr_info_t *a, gdouble to_step, gdouble to_overall,
		GError **err, GByteArray *req, gchar **master)
{
	gchar stra[128];
	struct gridd_client_s *client = NULL;
	GByteArray *gba;
	GError *e = NULL;

	EXTRA_ASSERT(a != NULL);
	EXTRA_ASSERT(req != NULL);
	GRID_TRACE2("%s:%d", __FUNCTION__, __LINE__);

	gba = g_byte_array_new();
	grid_addrinfo_to_string(a, stra, sizeof(stra));
	client = gridd_client_create(stra, req, gba, on_reply);

	if(to_step > 0 && to_overall > 0)
		gridd_client_set_timeout(client, to_step, to_overall);

	gscstat_tags_start(GSCSTAT_SERVICE_META1, GSCSTAT_TAGS_REQPROCTIME);
	gridd_client_start(client);
	if (!(e = gridd_client_loop(client)))
		e = gridd_client_error(client);
	gscstat_tags_end(GSCSTAT_SERVICE_META1, GSCSTAT_TAGS_REQPROCTIME);

	/* in RO request, we don't need this information */
	if(NULL != master) {
		char tmp[64];
		bzero(tmp, sizeof(tmp));
		addr_info_to_string(a, tmp, sizeof(tmp));

		if(g_ascii_strcasecmp(tmp, gridd_client_url(client)))
			*master = g_strdup(gridd_client_url(client));
	}

	gridd_client_free(client);

	if (e) {
		if (err)
			*err = e;
		else
			g_clear_error(&e);
		g_byte_array_free(gba, TRUE);
		return NULL;
	}

	gchar **lines = metautils_decode_lines((gchar*)gba->data,
			(gchar*)(gba->data + gba->len));
	if (!lines && err)
		*err = NEWERROR(CODE_BAD_REQUEST, "Invalid buffer content");
	g_byte_array_free(gba, TRUE);
	return lines;
}

gboolean 
meta1v2_remote_create_reference (const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid, const gchar *refname,
		gdouble to_step, gdouble to_overall, gchar **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_CREATE,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_CONTAINERNAME, gba_poolify(&pool, metautils_gba_from_string(refname)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean
meta1v2_remote_has_reference (const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid,
		gdouble to_step, gdouble to_overall)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_HAS,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean 
meta1v2_remote_delete_reference (const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid,
		gdouble to_step, gdouble to_overall,
		char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_DESTROY,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gchar** 
meta1v2_remote_link_service(const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refID, const gchar *srvtype,
		gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refID != NULL);
	EXTRA_ASSERT(srvtype != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVAVAIL,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refID)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);
	return result;
}

gchar**
meta1v2_remote_list_reference_services(const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid, const gchar *srvtype,
		gdouble to_step, gdouble to_overall)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	GRID_TRACE2("%s:%d", __FUNCTION__, __LINE__);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVALL,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
	gba_pool_clean(&pool);
	return result;
}

gboolean 
meta1v2_remote_unlink_service(const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid, const gchar *srvtype,
		gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(srvtype != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVDEL,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean
meta1v2_remote_unlink_one_service(const addr_info_t *meta1, GError **err,
		const char *ns, const container_id_t refid, const gchar *srvtype,
		gdouble to_step, gdouble to_overall,
		char **master, gint64 seqid)
{
	GSList *pool = NULL;
	GByteArray *body = NULL;

	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(srvtype != NULL);

	body = gba_poolify(&pool, g_byte_array_new());

	if (seqid <= 0) {
		if (err)
			*err = NEWERROR(CODE_BAD_REQUEST, "Invalid sequence number [%"G_GINT64_FORMAT"]", seqid);
		gba_pool_clean(&pool);
		return FALSE;
	}
	else {
		gchar str[128];
		g_snprintf(str, sizeof(str), "%"G_GINT64_FORMAT"\n", seqid);
		g_byte_array_append(body, (guint8*)str, strlen(str));
		GRID_DEBUG("About to delete seqid=%s", str);
	}

	if (body->len <= 0) {
		if (err)
			*err = NEWERROR(CODE_BAD_REQUEST, "No sequence number provided");
		gba_pool_clean(&pool);
		return FALSE;
	}

	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVDEL,
			body,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gchar **
meta1v2_remote_poll_reference_service(const addr_info_t *meta1,
		GError **err, const char *ns, const container_id_t refid,
		const gchar *srvtype, gdouble to_step, gdouble to_overall,
		char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(srvtype != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVNEW,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);
	return result;
}

gchar **
meta1v2_remote_update_m1_policy(const addr_info_t *meta1, GError **err,
		const char *ns,  const container_id_t prefix, const container_id_t refid,
		const gchar *srvtype, const gchar* action, gboolean checkonly,
		const gchar *excludeurl, gdouble to_step, gdouble to_overall)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(srvtype != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_UPDATEM1POLICY,
			NULL,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NAME_MSGKEY_ACTION, gba_poolify(&pool, metautils_gba_from_string(action)),
			NULL);

	if (prefix)
		message_add_fields_gba(req,NAME_MSGKEY_PREFIX,gba_poolify(&pool,metautils_gba_from_cid(prefix)),NULL);
	if (refid)
		message_add_fields_gba(req,NAME_MSGKEY_CONTAINERID,gba_poolify(&pool,metautils_gba_from_cid(refid)),NULL);
	if (checkonly)
		message_add_field( req, NAME_MSGKEY_CHECKONLY, "true", sizeof("true")-1);
	if( excludeurl )
		message_add_fields_gba(req,
				"EXCLUDEURL", gba_poolify(&pool,metautils_gba_from_string(excludeurl)),
				NULL);

	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
	gba_pool_clean(&pool);
	return result;
}

gboolean
meta1v2_remote_force_reference_service(const addr_info_t *meta1,
		GError **err, const char *ns, const container_id_t refid,
		const gchar *url, gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(url != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVSET,
			gba_poolify(&pool, metautils_gba_from_string(url)),
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean
meta1v2_remote_configure_reference_service(const addr_info_t *meta1,
		GError **err, const char *ns, const container_id_t refid,
		const gchar *url, gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(meta1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(url != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_SRVSETARG,
			gba_poolify(&pool, metautils_gba_from_string(url)),
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(meta1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean
meta1v2_remote_reference_set_property(const addr_info_t *m1, GError **err,
		const gchar *ns, const container_id_t refid, gchar **pairs,
		gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(m1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_CID_PROPSET,
			gba_poolify(&pool, metautils_encode_lines(pairs)),
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(m1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

gboolean
meta1v2_remote_reference_get_property(const addr_info_t *m1, GError **err,
		const gchar *ns, const container_id_t refid,
		gchar **keys, gchar ***result, gdouble to_step, gdouble to_overall)
{
	EXTRA_ASSERT(m1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);
	EXTRA_ASSERT(result != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_CID_PROPGET,
			gba_poolify(&pool, metautils_encode_lines(keys)),
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	*result = list_request(m1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
	gba_pool_clean(&pool);
	return *result != NULL;
}

gboolean
meta1v2_remote_reference_del_property(const addr_info_t *m1, GError **err,
		const gchar *ns, const container_id_t refid,
		gchar **keys, gdouble to_step, gdouble to_overall, char **master)
{
	EXTRA_ASSERT(m1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL, NAME_MSGNAME_M1V2_CID_PROPDEL,
			gba_poolify(&pool, metautils_encode_lines(keys)),
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_CONTAINERID, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);
	gchar **result = list_request(m1, to_step, to_overall, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), master);
	gba_pool_clean(&pool);

	if (!result)
		return FALSE;
	g_strfreev(result);
	return TRUE;
}

static GError *
gba_request(const addr_info_t *a, gdouble timeout, GByteArray **result, GByteArray *req)
{
	gchar str[STRLEN_ADDRINFO];
	addr_info_to_string (a, str, sizeof(str));
	return gridd_client_exec_and_concat (str, timeout>0.0 ? timeout : M1V2_CLIENT_TIMEOUT, req, result);
}

gchar**
meta1v2_remote_list_services(const addr_info_t *m1, GError **err,
        const gchar *ns, const container_id_t refid)
{
    EXTRA_ASSERT(m1 != NULL);
    EXTRA_ASSERT(ns != NULL);
    EXTRA_ASSERT(refid != NULL);

    GSList *pool = NULL;
    MESSAGE req = message_create_request(NULL, NULL,
            NAME_MSGNAME_M1V2_SRVALLONM1, NULL /* no body */,
            NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
            NAME_MSGKEY_PREFIX, gba_poolify(&pool, metautils_gba_from_cid(refid)),
            NULL);

    gchar** result = list_request(m1,  60000, 60000, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
    gba_pool_clean(&pool);
    return result;
}

GError *
meta1v2_remote_list_references(const addr_info_t *m1, const gchar *ns,
		const container_id_t refid, GByteArray **result)
{
	EXTRA_ASSERT(m1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL,
			NAME_MSGNAME_M1V2_LISTBYPREF, NULL /* no body */,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_PREFIX, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NULL);

	GError *err = gba_request(m1, M1V2_CLIENT_TIMEOUT, result,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)));
	gba_pool_clean(&pool);
	return err;
}

GError *
meta1v2_remote_list_references_by_service(const addr_info_t *m1,
		const gchar *ns, const container_id_t refid,
		const gchar *srvtype, const gchar *url,
		GByteArray **result)
{
	EXTRA_ASSERT(m1 != NULL);
	EXTRA_ASSERT(ns != NULL);
	EXTRA_ASSERT(refid != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_request(NULL, NULL,
			NAME_MSGNAME_M1V2_LISTBYSERV, NULL /* no body */,
			NAME_MSGKEY_NAMESPACE, gba_poolify(&pool, metautils_gba_from_string(ns)),
			NAME_MSGKEY_PREFIX, gba_poolify(&pool, metautils_gba_from_cid(refid)),
			NAME_MSGKEY_SRVTYPE, gba_poolify(&pool, metautils_gba_from_string(srvtype)),
			NAME_MSGKEY_URL, gba_poolify(&pool, metautils_gba_from_string(url)),
			NULL);

	GError *err = gba_request(m1, M1V2_CLIENT_TIMEOUT, result,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)));
	gba_pool_clean(&pool);
	return err;
}

gboolean
meta1v2_remote_get_prefixes(const addr_info_t *m1, GError **err, gchar *** result)
{
	EXTRA_ASSERT(m1 != NULL);

	GSList *pool = NULL;
	MESSAGE req = message_create_named(NAME_MSGNAME_M1V2_GETPREFIX);
	*result = list_request(m1, 60000, 60000, err,
			gba_poolify(&pool, message_marshall_gba_and_clean(req)), NULL);
	gba_pool_clean(&pool);
	return *result != NULL;
}

