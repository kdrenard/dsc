#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_QNAME_SZ 512

typedef struct _dns_message dns_message;
struct _dns_message {
    struct timeval ts;
    struct in_addr client_ipv4_addr;
    unsigned short src_port;
    unsigned short qtype;
    unsigned short qclass;
    unsigned short msglen;
    char qname[MAX_QNAME_SZ];
    const char *tld;
    unsigned char rcode;
    unsigned int qr:1;
    unsigned int rd:1;		/* set if RECUSION DESIRED bit is set */
    struct {
	unsigned int found:1;	/* set if we found an OPT RR */
	unsigned int d0:1;	/* set if DNSSEC D0 bit is set */
	unsigned char version;	/* version field from OPT RR */
    } edns;
    /* ... */
};

typedef void (DMC) (dns_message *);

void dns_message_report(void);
int dns_message_add_array(const char *, const char *,const char *,const char *,const char *,const char *, int);
const char * dns_message_tld(dns_message * m);

