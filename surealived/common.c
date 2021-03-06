/*
 * Copyright 2009-2011 DreamLab Onet.pl Sp. z o.o.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *   See the GNU General Public License for more details.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
*/

#include <common.h>
#include <sd_maincfg.h>
#include <sd_ipvssync.h>

static gchar *protostr[] = { "tcp", "udp", "fwmark", NULL };
static gchar *rtstr[] = { "dr", "masq", "tun", NULL };
static gchar *rstatestr[] = { "ONLINE", "OFFLINE", "DOWN", NULL };
static gchar *nstatestr[] = { "UNKNOWN", "UP", "DOWN", NULL };
static GHashTable *VCfgHash = NULL;

inline void time_inc(struct timeval *t, guint s, guint ms) {
    guint usd;

    usd = t->tv_usec + ms*1000;

    t->tv_sec += s + usd / 1000000;
    t->tv_usec = usd % 1000000;
}

/*! \brief Set the communication log filename for a specified real 
  \param real CfgReal 
  \param fname A buffer at least BUFSIZ length to store a filename
*/
static gchar *sd_commlog_fname(CfgReal *real, gchar *fname) {
    snprintf(fname, BUFSIZ, "%s/%s_%s", G_debug_comm_path, real->virt->name, real->name);
    return fname;
}

/*! \brief Unlink the communication log based on a filename constructed from a \a real
  \param real CfgReal
  \return \a TRUE if unlink succeed, otherwise \a FALSE
*/
gboolean sd_unlink_commlog(CfgReal *real) {
    gchar fname[BUFSIZ];
    sd_commlog_fname(real, fname);
    if (unlink(fname))
        return FALSE;
    return TRUE;
}

/*! \brief Append bufsiz length message from buf to \a fname file 
  \param fname A filename to append to
  \param buf A buffer to write
  \param bufsiz A buffer message len
  \return \a TRUE if \a bufsiz length write succeed, otherwise \a FALSE
*/
gboolean sd_append_to_file(gchar *fname, gchar *buf, gint bufsiz, gchar *usertxt) {
    struct timeval tv;
    gchar buft[32];
    gchar micro[16];
    FILE *out;
    int w, fdno;

    out = fopen(fname, "a");
    if (!out)
        LOGERROR("Can't append to file %s", fname);
    assert(out);

    fdno = fileno(out);
    gettimeofday(&tv, NULL);
    strftime(buft, 32, "[%F %T.", localtime(&tv.tv_sec));
    
    flock(fdno, LOCK_EX);
    fprintf(out, "%s", buft);
    snprintf(micro, 16, "%06d] ", (int) tv.tv_usec);
    fprintf(out, "%s", micro);

    if (usertxt) 
        fprintf(out, "%s", usertxt);

    w = fwrite(buf, bufsiz, 1, out);
    fprintf(out, "\n");
    fflush(out);
    flock(fdno, LOCK_UN);
    fclose(out);

    if (!w)
        return FALSE;

    return TRUE;
}

/*! \brief Append to a real based filename communication log
  \param real CfgReal used to build a filename
  \param buf A buffer to write to the communication log
  \param bufsiz A buffer length
*/
gboolean sd_append_to_commlog(CfgReal *real, gchar *buf, gint bufsiz, gchar *usertxt) {
    gchar fname[BUFSIZ];

    sd_commlog_fname(real, fname);

    return sd_append_to_file(fname, buf, bufsiz, usertxt);
}

gchar *sd_proto_str(SDIPVSProtocol proto) {
    return protostr[proto];
}

SDIPVSProtocol sd_proto_no(gchar *str) {
    gint i = 0;

    while (1) {
        if (!protostr[i])
            return -1;

        if (!strcmp(protostr[i], str))
            return i;

        i++;
    }
    return -1;
}

gchar *sd_rt_str(SDIPVSRt rt) {
    return rtstr[rt];
}

gchar *sd_rstate_str(RState st) {
    return rstatestr[st];
}

RState sd_rstate_no(gchar *str) {
    gint i = 0;

    while (1) {
        if (!rstatestr[i])
            return -1;

        if (!strcmp(rstatestr[i], str))
            return i;

        i++;
    }
    return -1;
}

gchar *sd_nstate_str(NotifyState st) {
    return nstatestr[st];
}

NotifyState sd_nstate_no(gchar *str) {
    gint i = 0;

    while (1) {
        if (!nstatestr[i])
            return -1;

        if (!strcmp(nstatestr[i], str))
            return i;

        i++;
    }
    return -1;
}

/* ---------------------------------------------------------------------- */
/* Split line into hash table, returns GHashTable */
GHashTable *sd_parse_line(gchar *line) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_assert(ht);

    gchar **strv = g_strsplit_set(line, " \t", 0);
    gchar **v;
	
    v = strv;
    while ( *v ) {
        gchar **kvstrv = g_strsplit_set(*v, "=", 0);
        if (kvstrv[0] && kvstrv[1]) {
            LOGDETAIL("* key = %s, val = %s", kvstrv[0], kvstrv[1]);
            g_hash_table_insert(ht, g_strdup(kvstrv[0]), g_strdup(kvstrv[1]));
        }
        g_strfreev(kvstrv);
        v++;
    }

    g_strfreev(strv);

    return ht;
}

/* ---------------------------------------------------------------------- */
/* create hash table which contains mapping:
   "VIP:VPORT:VPROTO:VFWMARK" -> CfgVirtual *,
   "VIP:VPORT:VPROTO:VFWMARK RIP:RPORT" -> CfgReal *, for ex.
   "192.168.0.1:80:0:0" -> 0x081a1234
   "192.168.0.1:80:0:0 10.0.0.1:80" -> 0x08ab1234
   Very useful when trying to find out CfgVirtual* and CfgReal*
*/

GHashTable *sd_vcfg_hashmap_new(GPtrArray *VCfgArr) {
    CfgVirtual *virt;
    CfgReal    *real;
    gint        vi, ri;
    gchar      *str;

    if (VCfgHash)
        return VCfgHash;

    VCfgHash = g_hash_table_new(g_str_hash, g_str_equal);
    assert(VCfgHash);

    LOGINFO("Building virtual/real hashmap...");
    for (vi = 0; vi < VCfgArr->len; vi++) {
        virt = g_ptr_array_index(VCfgArr, vi);
        
        str = g_strdup_printf("%s:%d:%d:%d", 
                              virt->addrtxt, ntohs(virt->port), virt->ipvs_proto, virt->ipvs_fwmark);
        g_hash_table_insert(VCfgHash, str, virt);
        LOGDEBUG("\tHASHMAP VIRT '%s' -> %p", str, virt);

        if (!virt->realArr) /* ignore empty virtuals */
            continue;
            
        for (ri = 0; ri < virt->realArr->len; ri++) {
            real = g_ptr_array_index(virt->realArr, ri);

            str = g_strdup_printf("%s:%d:%d:%d %s:%d", 
                                  virt->addrtxt, ntohs(virt->port), virt->ipvs_proto, virt->ipvs_fwmark,
                                  real->addrtxt, ntohs(real->port));
            g_hash_table_insert(VCfgHash, str, real);
            LOGDEBUG("\tHASHMAP REAL '%s' -> %p", str, real);
        }
    }

    return VCfgHash;
}

CfgVirtual *sd_vcfg_hashmap_lookup_virtual(GHashTable *VCfgHash, gchar *vkey) {
    CfgVirtual *virt = g_hash_table_lookup(VCfgHash, vkey);
    LOGDEBUG("hashmap lookup virtual [%s]: %p", vkey, virt);    
    return virt;
}

CfgReal *sd_vcfg_hashmap_lookup_real(GHashTable *VCfgHash, gchar *rkey) {
    CfgReal *real = g_hash_table_lookup(VCfgHash, rkey);
    LOGDEBUG("hashmap lookup real [%s]: %p", rkey, real);    
    return real;
}

/* This function returns expected weight sum for all reals
   - online/offline/down - if apply_online_state == FALSE
   - online              - if apply_online_state == TRUE
 */
gint sd_vcfg_reals_weight_sum(CfgVirtual *virt, gboolean apply_online_state) {
    CfgReal *real;
    gint sum = 0, ri;
    
    if (!virt || !virt->realArr)
        return 0;

    for (ri = 0; ri < virt->realArr->len; ri++) {
        real = g_ptr_array_index(virt->realArr, ri);
        sum += sd_ipvssync_calculate_real_weight(real, apply_online_state);
    }

    LOGDEBUG("Virtual [%s], reals_weight_sum = %d, apply_online_state = %s", virt->name, sum, GBOOLSTR(apply_online_state));
    return sum;
}

/* This function returns sum of all online reals */
gint sd_vcfg_reals_online_sum(CfgVirtual *virt) {
    CfgReal *real;
    gint sum = 0, ri;
    
    if (!virt || !virt->realArr)
        return 0;

    for (ri = 0; ri < virt->realArr->len; ri++) {
        real = g_ptr_array_index(virt->realArr, ri);
        if (real->rstate == REAL_ONLINE && real->online)
            sum++;
    }

    LOGDEBUG("Virtual [%s], reals_online_sum = %d/%d", virt->name, sum, virt->realArr->len);
    return sum;
}
