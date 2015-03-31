/*
OpenIO SDS cluster
Copyright (C) 2014 Worldine, original work as part of Redcurrant
Copyright (C) 2015 OpenIO, modified as part of OpenIO Software Defined Storage

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

#ifndef OIO_SDS__cluster__remote__gridcluster_remote_h
# define OIO_SDS__cluster__remote__gridcluster_remote_h 1

/**
 * @addtogroup gridcluster_remote
 * @{
 */

#include <metautils/lib/metacomm.h>

/** Get infos about namespace */
namespace_info_t *gcluster_get_namespace_info_full(addr_info_t *addr,
		long to, GError **error);

meta0_info_t* gcluster_get_meta0(addr_info_t *addr,
		long to, GError **error);

meta0_info_t* gcluster_get_meta0_2timeouts(addr_info_t * addr,
		long to_cnx, long to_req, GError ** error);

/** Push a list of broken containers to the conscience */
gint gcluster_push_broken_container(addr_info_t *addr, long to,
		GSList *container_list, GError **error);

gint gcluster_v2_push_broken_container(struct metacnx_ctx_s *cnx,
		const gchar *ns_name, const container_id_t cid, GError **error);

gint gcluster_v2_push_broken_content(struct metacnx_ctx_s *cnx,
		const gchar *ns_name, const container_id_t cid, const gchar *path,
		GError **error);

/** Push a list of virtual namespace space used to the conscience */
gint gcluster_push_virtual_ns_space_used(addr_info_t * addr,
		long to, GHashTable *space_used, GError ** error);

/** Tell the conscience that a rawx was fully scaned to repair these containers */
gint gcluster_fix_broken_container(addr_info_t *addr, long to,
		GSList *container_list, GError **error);

/** Remove a list of broken containers from the conscience */
gint gcluster_rm_broken_container(addr_info_t *addr, long to,
		GSList *container_list, GError **error);

/** Get the full broken container list from the conscience */
GSList *gcluster_get_broken_container(addr_info_t *addr, long to,
		GError **error);

/** Remove a list of broken containers from the conscience */
GSList *gcluster_get_services(const char *target, gdouble timeout,
		const gchar *type, gboolean full, GError **error);

/** Get the list of service types from conscience. */
GSList *gcluster_get_service_types(addr_info_t *addr, long timeout,
		GError **error);

gint gcluster_push_services(addr_info_t *addr, long to,
		GSList *services_list, gboolean lock_action, GError **error);

GByteArray* gcluster_get_srvtype_event_config(addr_info_t *addr, long to,
		gchar *name, GError **error);

/** @} */

#endif /*OIO_SDS__cluster__remote__gridcluster_remote_h*/
