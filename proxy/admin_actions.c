/*
OpenIO SDS proxy
Copyright (C) 2016-2019 OpenIO SAS, as part of OpenIO SDS

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <meta0v2/meta0_remote.h>
#include <meta0v2/meta0_utils.h>

#include "common.h"
#include "actions.h"

static GString* _m0_mapping_from_m1_list(GSList *m1_list) {
	GString *out = g_string_sized_new(65536 * 20);

	g_string_append_c(out, '{');
	if (m1_list) {
		gboolean first = TRUE;
		GPtrArray *array = meta0_utils_list_to_array(m1_list);
		for (guint i = 0; i < array->len; i++) {
			gchar **v;
			if ((v = array->pdata[i]) != NULL) {
				if (!first)
					g_string_append_c(out, ',');
				guint16 p = i;
				gboolean first2 = TRUE;
				GByteArray *gba = g_byte_array_new();
				g_byte_array_append(gba, (guint8 *) &p, 2);
				g_string_append(out, "\"");
				metautils_gba_to_hexgstr(out, gba);
				g_string_append(out, "\":[");
				g_byte_array_free(gba, TRUE);
				for (gchar **m1 = v; v && *m1; m1++) {
					if (!first2)
						g_string_append_c(out, ',');
					first2 = FALSE;
					g_string_append_printf(out, "\"%s\"", *m1);
				}
				g_string_append_c(out, ']');
				first = FALSE;
			}
		}
		meta0_utils_array_clean(array);
	}
	g_string_append_c(out, '}');

	return out;
}

typedef GError*(*meta0_func)(const char *m0_url, gpointer udata);

static GError*
_action_admin_meta0_common(meta0_func func, gpointer udata)
{
	gchar **urlv = NULL;
	GError *err = proxy_locate_meta0(ns_name, &urlv, oio_ext_get_deadline());
	if (err)
		return err;

	for (gchar **pu = urlv; *pu; ++pu) {
		if (err) g_clear_error(&err);
		meta1_url_shift_addr(*pu);
		err = func(*pu, udata);
		if (!err || !CODE_IS_NETWORK_ERROR(err->code))
			break;
	}
	g_strfreev(urlv);
	return err;
}

static GError*
_wrap_meta0_remote_get_meta1_all(const char *m0_url, GSList **out)
{
	const gint64 deadline = oio_ext_get_deadline();
	return meta0_remote_get_meta1_all(m0_url, out, deadline);
}

enum http_rc_e
action_admin_meta0_list(struct req_args_s *args) {
	GSList *m1_lst = NULL;
	GString *json = NULL;
	GError *err = _action_admin_meta0_common(
			(meta0_func)_wrap_meta0_remote_get_meta1_all, &m1_lst);

	if (!err) {
		json = _m0_mapping_from_m1_list(m1_lst);
		meta0_utils_list_clean(m1_lst);
		return _reply_json(args, HTTP_CODE_OK, "OK", json);
	}
	return _reply_common_error(args, err);
}

static GError*
_wrap_meta0_remote_force(const char *m0_url, GByteArray *udata)
{
	const gint64 deadline = oio_ext_get_deadline();
	GError *err = meta0_remote_force(m0_url, udata->data, udata->len, deadline);
	if (!err)
		err = meta0_remote_cache_refresh(m0_url, deadline);
	return err;
}

enum http_rc_e
action_admin_meta0_force(struct req_args_s *args) {
	GError *err = _action_admin_meta0_common(
			(meta0_func)_wrap_meta0_remote_force, args->rq->body);

	if (!err)
		return _reply_nocontent(args);
	return _reply_common_error(args, err);
}
