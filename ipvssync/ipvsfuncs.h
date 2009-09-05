/*
 * Copyright 2009 DreamLab Onet.pl Sp. z o.o.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#if !defined __IPVSFUNCS_H
#define __IPVSFUNCS_H

#include <linux/types.h>
#include <glib.h>
#include <libipvs.h>
#include <net/ip_vs.h> 

int ipvsfuncs_modprobe_ipvs(void);
void ipvsfuncs_initialize(void);

ipvs_service_t *ipvsfuncs_set_svc(u_int16_t protocol, char *taddr, char *tport,
                                  u_int32_t fwmark, char *sched_name, unsigned  flags,
                                  unsigned  timeout, __be32 netmask, ipvs_service_t *svc);

ipvs_dest_t *ipvsfuncs_set_dest(char *taddr, char *tport, 
                                unsigned conn_flags, int weight, 
                                u_int32_t u_threshold, u_int32_t l_threshold, 
                                ipvs_dest_t *dest);

int ipvsfuncs_add_service(ipvs_service_t *svc);
int ipvsfuncs_del_service(ipvs_service_t *svc);
int ipvsfuncs_update_service(ipvs_service_t *svc);

int ipvsfuncs_add_dest(ipvs_service_t *svc, ipvs_dest_t *dest);
int ipvsfuncs_del_dest(ipvs_service_t *svc, ipvs_dest_t *dest);
int ipvsfuncs_update_dest(ipvs_service_t *svc, ipvs_dest_t *dest);

int ipvsfuncs_fprintf_services(FILE *f);
int ipvsfuncs_del_unmanaged_services(ipvs_service_t **managed_svc, gint *is_in_ipvs);
int ipvsfuncs_del_unmanaged_dests(ipvs_service_t *svc,
                                  ipvs_dest_t **managed_dest, gint *is_in_ipvs);

int ipvsfuncs_fprintf_ipvs(FILE *f);

gboolean ipvsfuncs_set_svc_from_ht(ipvs_service_t *svc, GHashTable *ht);
gboolean ipvsfuncs_set_dest_from_ht(ipvs_dest_t *dest, GHashTable *ht, unsigned vconn_flags);
#endif
