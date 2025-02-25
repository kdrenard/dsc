/*
 * Copyright (c) 2008-2023, OARC, Inc.
 * Copyright (c) 2007-2008, Internet Systems Consortium, Inc.
 * Copyright (c) 2003-2007, The Measurement Factory, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "dns_message.h"
#include "xmalloc.h"
#include "syslog_debug.h"
#include "tld_list.h"

#include "null_index.h"
#include "qtype_index.h"
#include "qclass_index.h"
#include "country_index.h"
#include "asn_index.h"
#include "tld_index.h"
#include "rcode_index.h"
#include "client_index.h"
#include "client_subnet_index.h"
#include "server_ip_addr_index.h"
#include "qnamelen_index.h"
#include "label_count_index.h"
#include "qname_index.h"
#include "msglen_index.h"
#include "certain_qnames_index.h"
#include "idn_qname_index.h"
#include "query_classification_index.h"
#include "edns_version_index.h"
#include "edns_bufsiz_index.h"
#include "do_bit_index.h"
#include "rd_bit_index.h"
#include "tc_bit_index.h"
#include "qr_aa_bits_index.h"
#include "opcode_index.h"
#include "transport_index.h"
#include "dns_ip_version_index.h"
#include "dns_source_port_index.h"
#include "response_time_index.h"
#include "encryption_index.h"

#include "ip_direction_index.h"
#include "ip_proto_index.h"
#include "ip_version_index.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>

extern int            debug_flag;
static md_array_list* Arrays     = 0;
static filter_list*   DNSFilters = 0;

static indexer indexers[] = {
    { "client", 0, client_indexer, client_iterator, client_reset },
    { "server", 0, sip_indexer, sip_iterator, sip_reset },
    { "country", country_init, country_indexer, country_iterator, country_reset },
    { "asn", asn_init, asn_indexer, asn_iterator, asn_reset },
    { "client_subnet", client_subnet_init, client_subnet_indexer, client_subnet_iterator, client_subnet_reset },
    { "null", 0, null_indexer, null_iterator },
    { "qclass", 0, qclass_indexer, qclass_iterator, qclass_reset },
    { "qnamelen", 0, qnamelen_indexer, qnamelen_iterator, qnamelen_reset },
    { "label_count", 0, label_count_indexer, label_count_iterator, label_count_reset },
    { "qname", 0, qname_indexer, qname_iterator, qname_reset },
    { "second_ld", 0, second_ld_indexer, second_ld_iterator, second_ld_reset },
    { "third_ld", 0, third_ld_indexer, third_ld_iterator, third_ld_reset },
    { "msglen", 0, msglen_indexer, msglen_iterator, msglen_reset },
    { "qtype", 0, qtype_indexer, qtype_iterator, qtype_reset },
    { "rcode", 0, rcode_indexer, rcode_iterator, rcode_reset },
    { "tld", 0, tld_indexer, tld_iterator, tld_reset },
    { "certain_qnames", 0, certain_qnames_indexer, certain_qnames_iterator },
    { "query_classification", 0, query_classification_indexer, query_classification_iterator },
    { "idn_qname", 0, idn_qname_indexer, idn_qname_iterator },
    { "edns_version", 0, edns_version_indexer, edns_version_iterator },
    { "edns_bufsiz", 0, edns_bufsiz_indexer, edns_bufsiz_iterator },
    { "do_bit", 0, do_bit_indexer, do_bit_iterator },
    { "rd_bit", 0, rd_bit_indexer, rd_bit_iterator },
    { "tc_bit", 0, tc_bit_indexer, tc_bit_iterator },
    { "opcode", 0, opcode_indexer, opcode_iterator, opcode_reset },
    { "transport", 0, transport_indexer, transport_iterator },
    { "dns_ip_version", 0, dns_ip_version_indexer, dns_ip_version_iterator, dns_ip_version_reset },
    { "dns_source_port", 0, dns_source_port_indexer, dns_source_port_iterator, dns_source_port_reset },
    { "dns_sport_range", 0, dns_sport_range_indexer, dns_sport_range_iterator, dns_sport_range_reset },
    { "qr_aa_bits", 0, qr_aa_bits_indexer, qr_aa_bits_iterator },
    { "response_time", 0, response_time_indexer, response_time_iterator, response_time_reset, response_time_flush },
    { "ip_direction", 0, ip_direction_indexer, ip_direction_iterator },
    { "ip_proto", 0, ip_proto_indexer, ip_proto_iterator, ip_proto_reset },
    { "ip_version", 0, ip_version_indexer, ip_version_iterator, ip_version_reset },
    { "encryption", 0, encryption_indexer, encryption_iterator },
    { 0 }
};

/*
 * Filters
 */

static int queries_only_filter(const dns_message* m, const void* ctx)
{
    return m->qr ? 0 : 1;
}

static int nxdomains_only_filter(const dns_message* m, const void* ctx)
{
    return m->rcode == 3;
}

static int ad_filter(const dns_message* m, const void* ctx)
{
    return m->ad;
}

static int popular_qtypes_filter(const dns_message* m, const void* ctx)
{
    switch (m->qtype) {
    case 1:
    case 2:
    case 5:
    case 6:
    case 12:
    case 15:
    case 28:
    case 33:
    case 38:
    case 255:
        return 1;
    default:
        break;
    }
    return 0;
}

static int aaaa_or_a6_filter(const dns_message* m, const void* ctx)
{
    switch (m->qtype) {
    case T_AAAA:
    case T_A6:
        return 1;
    default:
        break;
    }
    return 0;
}

static int idn_qname_filter(const dns_message* m, const void* ctx)
{
    return !strncmp(m->qname, "xn--", 4);
}

static int root_servers_net_filter(const dns_message* m, const void* ctx)
{
    return !strcmp(m->qname + 1, ".root-servers.net");
}

static int chaos_class_filter(const dns_message* m, const void* ctx)
{
    return m->qclass == C_CHAOS;
}

static int priming_query_filter(const dns_message* m, const void* ctx)
{
    if (m->qtype != T_NS)
        return 0;
    if (!strcmp(m->qname, "."))
        return 0;
    return 1;
}

static int replies_only_filter(const dns_message* m, const void* ctx)
{
    return m->qr ? 1 : 0;
}

static int qname_filter(const dns_message* m, const void* ctx)
{
    return !regexec((const regex_t*)ctx, m->qname, 0, 0, 0);
}

static int servfail_filter(const dns_message* m, const void* ctx)
{
    return m->rcode == 2;
}

/*
 * Helpers
 */

static const char* printable_dnsname(const char* name)
{
    static char buf[MAX_QNAME_SZ];
    int         i;

    for (i = 0; i < sizeof(buf) - 1;) {
        if (!*name)
            break;
        if (isgraph(*name)) {
            buf[i] = *name;
            i++;
        } else {
            if (i + 3 > MAX_QNAME_SZ - 1)
                break; /* expanded character would overflow buffer */
            snprintf(buf + i, sizeof(buf) - i - 1, "%%%02x", (unsigned char)*name);
            i += 3;
        }
        name++;
    }
    buf[i] = '\0';
    return buf;
}

static void dns_message_print(dns_message* m)
{
    char buf[128];
    inXaddr_ntop(m->qr ? &m->tm->dst_ip_addr : &m->tm->src_ip_addr, buf, 128);
    fprintf(stderr, "%15s:%5d", buf, m->qr ? m->tm->dst_port : m->tm->src_port);
    fprintf(stderr, "\t%s", (m->tm->proto == IPPROTO_UDP) ? "UDP" : (m->tm->proto == IPPROTO_TCP) ? "TCP" : "???");
    fprintf(stderr, "\tQT=%d", m->qtype);
    fprintf(stderr, "\tQC=%d", m->qclass);
    fprintf(stderr, "\tlen=%d", m->msglen);
    fprintf(stderr, "\tqname=%s", printable_dnsname(m->qname));
    fprintf(stderr, "\ttld=%s", printable_dnsname(dns_message_tld(m)));
    fprintf(stderr, "\topcode=%d", m->opcode);
    fprintf(stderr, "\trcode=%d", m->rcode);
    fprintf(stderr, "\tmalformed=%d", m->malformed);
    fprintf(stderr, "\tqr=%d", m->qr);
    fprintf(stderr, "\trd=%d", m->rd);
    fprintf(stderr, "\n");
}

static indexer* dns_message_find_indexer(const char* in)
{
    indexer* indexer;
    for (indexer = indexers; indexer->name; indexer++) {
        if (0 == strcmp(in, indexer->name))
            return indexer;
    }
    dsyslogf(LOG_ERR, "unknown indexer '%s'", in);
    return NULL;
}

static int dns_message_find_filters(const char* fn, filter_list** fl)
{
    char*        tok = 0;
    char*        t;
    char*        copy = xstrdup(fn);
    filter_list* f;
    if (NULL == copy)
        return 0;
    for (t = strtok_r(copy, ",", &tok); t; t = strtok_r(NULL, ",", &tok)) {
        if (0 == strcmp(t, "any"))
            continue;
        for (f = DNSFilters; f; f = f->next) {
            if (0 == strcmp(t, f->filter->name))
                break;
        }
        if (f) {
            fl = md_array_filter_list_append(fl, f->filter);
            continue;
        }
        dsyslogf(LOG_ERR, "unknown filter '%s'", t);
        xfree(copy);
        return 0;
    }
    xfree(copy);
    return 1;
}

/*
 * Public
 */

void dns_message_handle(dns_message* m)
{
    md_array_list* a;
    if (debug_flag > 1)
        dns_message_print(m);
    for (a = Arrays; a; a = a->next)
        md_array_count(a->theArray, m);
}

int dns_message_add_array(const char* name, const char* fn, const char* fi, const char* sn, const char* si, const char* f, dataset_opt opts)
{
    filter_list*   filters = NULL;
    indexer *      indexer1, *indexer2;
    md_array_list* a;

    if (NULL == (indexer1 = dns_message_find_indexer(fi)))
        return 0;
    if (NULL == (indexer2 = dns_message_find_indexer(si)))
        return 0;
    if (0 == dns_message_find_filters(f, &filters))
        return 0;

    a = xcalloc(1, sizeof(*a));
    if (a == NULL) {
        dsyslogf(LOG_ERR, "Cant allocate memory for '%s' DNS message array", name);
        return 0;
    }
    a->theArray = md_array_create(name, filters, fn, indexer1, sn, indexer2);
    if (NULL == a->theArray) {
        dsyslogf(LOG_ERR, "Cant allocate memory for '%s' DNS message array", name);
        xfree(a);
        return 0;
    }
    a->theArray->opts = opts;
    assert(a->theArray);
    a->next = Arrays;
    Arrays  = a;
    return 1;
}

void dns_message_flush_arrays(void)
{
    md_array_list* a;
    for (a = Arrays; a; a = a->next) {
        if (a->theArray->d1.indexer->flush_fn || a->theArray->d2.indexer->flush_fn)
            md_array_flush(a->theArray);
    }
}

void dns_message_report(FILE* fp, md_array_printer* printer)
{
    md_array_list* a;
    for (a = Arrays; a; a = a->next) {
        md_array_print(a->theArray, printer, fp);
    }
}

void dns_message_clear_arrays(void)
{
    md_array_list* a;
    for (a = Arrays; a; a = a->next)
        md_array_clear(a->theArray);
}

/*
 * QnameToNld
 *
 * qname is a 0-terminated string containing a DNS name
 * nld is the domain level to find
 *
 * return value is a pointer into the qname string.
 *
 * Handles the following cases:
 *    qname is empty ("")
 *    qname ends with one or more dots
 *    qname begins with one or more dots
 *    multiple consequtive dots in qname
 *
 * TESTS
 *        assert(0 == strcmp(QnameToNld("a.b.c.d", 1), "d"));
 *        assert(0 == strcmp(QnameToNld("a.b.c.d", 2), "c.d"));
 *        assert(0 == strcmp(QnameToNld("a.b.c.d.", 2), "c.d."));
 *        assert(0 == strcmp(QnameToNld("a.b.c.d....", 2), "c.d...."));
 *        assert(0 == strcmp(QnameToNld("c.d", 5), "c.d"));
 *        assert(0 == strcmp(QnameToNld(".c.d", 5), "c.d"));
 *        assert(0 == strcmp(QnameToNld(".......c.d", 5), "c.d"));
 *        assert(0 == strcmp(QnameToNld("", 1), ""));
 *        assert(0 == strcmp(QnameToNld(".", 1), "."));
 *        assert(0 == strcmp(QnameToNld("a.b..c..d", 2), "c..d"));
 *        assert(0 == strcmp(QnameToNld("a.b................c..d", 3), "b................c..d"));
 */
const char* dns_message_QnameToNld(const char* qname, int nld)
{
    const char* e = qname + strlen(qname) - 1;
    const char* t;
    int         dotcount = 0;
    int         state    = 0; /* 0 = not in dots, 1 = in dots */
    while (*e == '.' && e > qname)
        e--;
    t = e;
    if (0 == strcmp(t, ".arpa"))
        dotcount--;
    if (have_tld_list) {
        // Use TLD list to find labels that are the "TLD"
        const char *lt = 0, *ot = t;
        int done = 0;
        while (t > qname) {
            t--;
            if ('.' == *t) {
                if (0 == state) {
                    int r = tld_list_find(t + 1);
                    if (r & 1) {
                        // this is a tld
                        lt = t;
                    }
                    if (!r || !(r & 2)) {
                        // no more children
                        if (lt) {
                            // reset to what we last found
                            t = lt;
                            dotcount++;
                            state = 1;
                        } else {
                            // or reset
                            t     = ot;
                            state = 0;
                        }
                        done = 1;
                        break;
                    }
                }
                state = 1;
            } else {
                state = 0;
            }
        }
        if (!done) {
            // nothing found, reset t
            t = e;
        }
    }
    while (t > qname && dotcount < nld) {
        t--;
        if ('.' == *t) {
            if (0 == state)
                dotcount++;
            state = 1;
        } else {
            state = 0;
        }
    }
    while (*t == '.' && t < e)
        t++;
    return t;
}

const char* dns_message_tld(dns_message* m)
{
    if (NULL == m->tld)
        m->tld = dns_message_QnameToNld(m->qname, 1);
    return m->tld;
}

void dns_message_filters_init(void)
{
    filter_list** fl = &DNSFilters;

    fl = md_array_filter_list_append(fl, md_array_create_filter("queries-only", queries_only_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("replies-only", replies_only_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("nxdomains-only", nxdomains_only_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("popular-qtypes", popular_qtypes_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("idn-only", idn_qname_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("aaaa-or-a6-only", aaaa_or_a6_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("root-servers-net-only", root_servers_net_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("chaos-class", chaos_class_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("priming-query", priming_query_filter, 0));
    fl = md_array_filter_list_append(fl, md_array_create_filter("servfail-only", servfail_filter, 0));
    (void)md_array_filter_list_append(fl, md_array_create_filter("authentic-data-only", ad_filter, 0));
}

void dns_message_indexers_init(void)
{
    indexer* indexer;

    for (indexer = indexers; indexer->name; indexer++) {
        if (indexer->init_fn)
            indexer->init_fn();
    }
}

int add_qname_filter(const char* name, const char* pat)
{
    filter_list** fl = &DNSFilters;
    regex_t*      r;
    int           x;
    while ((*fl))
        fl = &((*fl)->next);
    r = xcalloc(1, sizeof(*r));
    if (NULL == r) {
        dsyslogf(LOG_ERR, "Cant allocate memory for '%s' qname filter", name);
        return 0;
    }
    if (0 != (x = regcomp(r, pat, REG_EXTENDED | REG_ICASE))) {
        char errbuf[512];
        regerror(x, r, errbuf, 512);
        dsyslogf(LOG_ERR, "regcomp: %s", errbuf);
    }
    (void)md_array_filter_list_append(fl, md_array_create_filter(name, qname_filter, r));
    return 1;
}
