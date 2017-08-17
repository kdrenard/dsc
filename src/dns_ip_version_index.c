/*
 * Copyright (c) 2016-2017, OARC, Inc.
 * Copyright (c) 2007, The Measurement Factory, Inc.
 * Copyright (c) 2007, Internet Systems Consortium, Inc.
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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <netdb.h>

#include "dns_message.h"
#include "md_array.h"

/* This indexer is the same as ip_version_indexer but
   applies only to DNS messages. */

static int largest = 0;

int dns_ip_version_indexer(const void* vp)
{
    const dns_message* m = vp;
    int                i = (int)inXaddr_version(&m->tm->src_ip_addr);
    if (i > largest)
        largest = i;
    return i;
}

static int next_iter = 0;

int dns_ip_version_iterator(char** label)
{
    static char label_buf[20];
    if (NULL == label) {
        next_iter = 0;
        return largest + 1;
    }
    if (next_iter > largest)
        return -1;
    snprintf(*label = label_buf, 20, "IPv%d", next_iter);
    return next_iter++;
}

void dns_ip_version_reset(void)
{
    largest = 0;
}
