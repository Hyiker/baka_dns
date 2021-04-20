#include "server/message.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logging.h"

int domain_len(const char* domain) {
    int result = strnlen(domain, DOMAIN_BUF_LEN) + 1;
    if (result > DOMAIN_LEN_MAX) {
        LOG_ERR("domain len too loooooong\n");
        return -1;
    }
    return result;
}
// copy the network byte ordered u8 array into dest with _size_
static uint32_t u8n_to_u32h(const uint8_t* ptr) {
    uint32_t dest;
    memcpy(&dest, ptr, sizeof(uint32_t));
    dest = ntohl(dest);
    return dest;
}
static uint16_t u8n_to_u16h(const uint8_t* ptr) {
    uint16_t dest;
    memcpy(&dest, ptr, sizeof(uint16_t));
    dest = ntohs(dest);
    return dest;
}
static uint8_t u8n_to_u8h(const uint8_t* ptr) {
    uint8_t dest;
    memcpy(&dest, ptr, sizeof(uint8_t));
    return dest;
}
static void u32h_to_u8n(uint32_t h, uint8_t* ptr) {
    uint32_t dest = htonl(h);
    memcpy(ptr, &dest, sizeof(h));
}
static void u16h_to_u8n(uint16_t h, uint8_t* ptr) {
    uint16_t dest = htons(h);
    memcpy(ptr, &dest, sizeof(h));
}
static void msg_header_from_buf(const uint8_t* buf,
                                struct message_header* msg_header) {
    uint16_t id, qdcount, ancount, nscount, arcount;
    uint8_t misc1, misc2;
    id = u8n_to_u16h(buf);
    misc1 = u8n_to_u8h(buf += sizeof(id));
    misc2 = u8n_to_u8h(buf += sizeof(misc1));
    qdcount = u8n_to_u16h(buf += sizeof(misc2));
    ancount = u8n_to_u16h(buf += sizeof(qdcount));
    nscount = u8n_to_u16h(buf += sizeof(ancount));
    arcount = u8n_to_u16h(buf += sizeof(nscount));
    msg_header->id = id;
    msg_header->misc1 = misc1;
    msg_header->misc2 = misc2;
    msg_header->qdcount = qdcount;
    msg_header->ancount = ancount;
    msg_header->nscount = nscount;
    msg_header->arcount = arcount;
    print_msg_header(msg_header);
}
// store domain from buf into dest
// returns the length of domain name(includes the '\0')
// -1 if err
static int read_domain(const uint8_t* buf, char* dest) {
    int buf_domain_len = domain_len(buf);
    if (buf_domain_len == -1 || buf_domain_len == 0u) {
        LOG_ERR("bad domain length");
        return -1;
    }

    memcpy(dest, buf, buf_domain_len * sizeof(uint8_t));
    return buf_domain_len;
}
// fetch message question info from buffer
// this function allocates _qname_ on the heap!
// returns n bytes it reads in _buf_
// err if -1
static int msg_question_from_buf(const uint8_t* buf,
                                 struct message_question* msg_qd) {
    // with the last '\0'
    uint8_t domainbuf[DOMAIN_BUF_LEN];
    int qname_len = read_domain(buf, domainbuf);
    if (qname_len < 0) {
        return -1;
    }

    uint8_t* qname = malloc(qname_len * sizeof(uint8_t));
    memcpy(qname, domainbuf, qname_len * sizeof(uint8_t));
    uint16_t qtype = u8n_to_u16h(buf + qname_len);
    uint16_t qclass = u8n_to_u16h(buf + qname_len + sizeof(qtype));
    msg_qd->qname = qname;
    msg_qd->qtype = qtype;
    msg_qd->qclass = qclass;
    return qname_len + sizeof(qtype) + sizeof(qclass);
}

static int msg_rr_from_buf(const uint8_t* buf, struct resource_record* rr) {
    uint8_t domainbuf[DOMAIN_BUF_LEN];
    int name_len = read_domain(buf, domainbuf);
    if (name_len < 0) {
        return -1;
    }

    uint8_t* name = malloc(name_len * sizeof(uint8_t));
    memcpy(name, domainbuf, name_len * sizeof(uint8_t));
    uint16_t type, _class, ttl, rdlength;
    type = u8n_to_u16h(buf += name_len);
    _class = u8n_to_u16h(buf += sizeof(type));
    ttl = u8n_to_u16h(buf += sizeof(_class));
    rdlength = u8n_to_u16h(buf += sizeof(ttl));
    buf += rdlength;
    uint8_t* rdata = malloc(rdlength * sizeof(uint8_t));
    memcpy(rdata, buf, rdlength * sizeof(uint8_t));
    rr->name = name;
    rr->_class = _class;
    rr->rdata = rdata;
    rr->type = type;
    rr->ttl = ttl;
    rr->rdlength = rdlength;
    LOG_INFO(
        "name = %s, type = %x, class = %x, ttl = %u, rdlength = %u, rdata = "
        "%s\n",
        name, type, _class, ttl, rdlength, rdata);
}
int message_from_buf(const uint8_t* buf, uint32_t size, struct message* msg) {
    if (size < sizeof(struct message_header)) {
        LOG_ERR("size too small");
        return -1;
    }
    const uint8_t* bufptr = buf;
    msg_header_from_buf(bufptr, &msg->header);
    bufptr += sizeof(msg->header);

    if (msg->header.qdcount) {
        msg->question =
            malloc(sizeof(struct message_question*) * msg->header.qdcount);
    }
    for (size_t i = 0; i < msg->header.qdcount; i++) {
        msg->question[i] = malloc(sizeof(struct message_question));
        int n = msg_question_from_buf(bufptr, msg->question[i]);
        if (n < 0) {
            return -1;
        }
        bufptr += n;
    }

    if (msg->header.ancount) {
        msg->answer =
            malloc(sizeof(struct resource_record*) * msg->header.ancount);
    }
    for (size_t i = 0; i < msg->header.ancount; i++) {
        msg->answer[i] = malloc(sizeof(struct resource_record));
        int n = msg_rr_from_buf(bufptr, msg->answer[i]);
        if (n < 0) {
            return -1;
        }
        bufptr += n;
    }

    if (msg->header.nscount) {
        msg->authority =
            malloc(sizeof(struct resource_record*) * msg->header.nscount);
    }
    for (size_t i = 0; i < msg->header.nscount; i++) {
        msg->authority[i] = malloc(sizeof(struct resource_record));
        int n = msg_rr_from_buf(bufptr, msg->authority[i]);
        if (n < 0) {
            return -1;
        }
        bufptr += n;
    }
    if (msg->header.arcount) {
        msg->addition =
            malloc(sizeof(struct resource_record*) * msg->header.arcount);
    }
    for (size_t i = 0; i < msg->header.arcount; i++) {
        msg->addition[i] = malloc(sizeof(struct resource_record));
        int n = msg_rr_from_buf(bufptr, msg->addition[i]);
        if (n < 0) {
            return -1;
        }
        bufptr += n;
    }
    return 1;
}

static int msg_header_to_u8(const struct message_header* header,
                            uint8_t* dest) {
    u16h_to_u8n(header->id, dest);
    dest += sizeof(uint16_t);
    *(dest++) = header->misc1;
    *(dest++) = header->misc2;
    u16h_to_u8n(header->qdcount, dest);
    u16h_to_u8n(header->ancount, dest += sizeof(uint16_t));
    u16h_to_u8n(header->nscount, dest += sizeof(uint16_t));
    u16h_to_u8n(header->arcount, dest += sizeof(uint16_t));
    return sizeof(struct message_header);
}
static int msg_question_to_u8(const struct message_question* q, uint8_t* dest) {
    int len_qname = domain_len(q->qname);
    if (len_qname == -1) {
        LOG_ERR("bad qname length");
        return -1;
    }
    memcpy(dest, q->qname, len_qname);
    u16h_to_u8n(q->qtype, dest += len_qname);
    u16h_to_u8n(q->qclass, dest += sizeof(q->qtype));
    return len_qname + sizeof(q->qtype) + sizeof(q->qclass);
}
static int msg_rr_to_u8(const struct resource_record* rr, uint8_t* dest) {
    int len_name = domain_len(rr->name);
    if (len_name == -1) {
        LOG_ERR("bad name length");
        return -1;
    }
    memcpy(dest, rr->name, len_name);
    u16h_to_u8n(rr->type, dest += len_name);
    u16h_to_u8n(rr->_class, dest += sizeof(rr->type));
    u32h_to_u8n(rr->ttl, dest += sizeof(rr->_class));
    u16h_to_u8n(rr->rdlength, dest += sizeof(rr->ttl));
    memcpy(dest += sizeof(rr->rdlength), rr->rdata, rr->rdlength);
    return len_name + sizeof(rr->type) + sizeof(rr->_class) + sizeof(rr->ttl) +
           sizeof(rr->rdlength) + rr->rdlength;
}
int message_to_u8(const struct message* msg, uint8_t* dest) {
    uint32_t hsize = 0, qsize = 0, ansize = 0, nssize = 0, arsize = 0;
    // header
    dest += (hsize = msg_header_to_u8(&msg->header, dest));
    // question
    if (msg->header.qdcount) {
        if (!msg->question) {
            LOG_ERR("invalid question**");
            return -1;
        }

        for (size_t i = 0; i < msg->header.qdcount; i++) {
            if (!msg->question[i]) {
                LOG_ERR("invalid question*");
                return -1;
            }
            dest += (qsize = msg_question_to_u8(msg->question[i], dest));
        }
    }
    // rr answer
    if (msg->header.ancount) {
        if (!msg->answer) {
            LOG_ERR("invalid answer**");
            return -1;
        }

        for (size_t i = 0; i < msg->header.ancount; i++) {
            if (!msg->answer[i]) {
                LOG_ERR("invalid answer*");
                return -1;
            }
            dest += (ansize = msg_rr_to_u8(msg->answer[i], dest));
        }
    }
    return hsize + qsize + ansize + nssize + arsize;
}

static void free_heap_message_question(struct message_question* mqptr) {
    if (mqptr->qname) {
        free(mqptr->qname);
    }
    free(mqptr);
}

static void free_heap_resource_record(struct resource_record* rrptr) {
    if (rrptr->name) {
        free(rrptr->name);
    }
    if (rrptr->rdata) {
        free(rrptr->rdata);
    }

    free(rrptr);
}
int free_heap_message(struct message* msgptr) {
    for (size_t i = 0; i < msgptr->header.qdcount; i++) {
        if (!msgptr->question || !msgptr->question[i]) {
            LOG_ERR("invalid qdcount");
            return -1;
        }
        free_heap_message_question(msgptr->question[i]);
    }
    if (msgptr->question) {
        free(msgptr->question);
    }
    for (size_t i = 0; i < msgptr->header.ancount; i++) {
        if (!msgptr->answer || !msgptr->answer[i]) {
            LOG_ERR("invalid ancount");
            return -1;
        }
        free_heap_resource_record(msgptr->answer[i]);
    }
    if (msgptr->answer) {
        free(msgptr->answer);
    }

    for (size_t i = 0; i < msgptr->header.nscount; i++) {
        if (!msgptr->authority || !msgptr->authority[i]) {
            LOG_ERR("invalid nscount");
            return -1;
        }
        free_heap_resource_record(msgptr->authority[i]);
    }
    if (msgptr->authority) {
        free(msgptr->authority);
    }

    for (size_t i = 0; i < msgptr->header.arcount; i++) {
        if (!msgptr->addition || !msgptr->addition[i]) {
            LOG_ERR("invalid arcount");
            return -1;
        }
        free_heap_resource_record(msgptr->addition[i]);
    }
    if (msgptr->addition) {
        free(msgptr->addition);
    }
}

void print_msg_header(struct message_header* msg_header) {
    LOG_INFO(
        "id = %u, misc1 = 0x%02x, misc2 = 0x%02x, qdcount = %u, ancount = %u, "
        "nscount "
        "= %u, arcount = %u\n",
        msg_header->id, msg_header->misc1, msg_header->misc2,
        msg_header->qdcount, msg_header->ancount, msg_header->nscount,
        msg_header->arcount);
}
inline uint8_t create_misc1(uint8_t qr, uint8_t opcode, uint8_t aa, uint8_t tc,
                            uint8_t rd) {
    return (qr << 7) & QR_MASK | (opcode << 3) & OPCODE_MASK |
           (aa << 2) & AA_MASK | (tc << 1) & TC_MASK | rd & RD_MASK;
}

inline uint8_t create_misc2(uint8_t ra, uint8_t z, uint8_t rcode) {
    return (ra << 7) & RA_MASK | (z << 4) & Z_MASK | rcode & RCODE_MASK;
}
struct resource_record* create_resource_record(uint8_t* name, uint16_t type,
                                               uint16_t _class, uint32_t ttl,
                                               uint16_t rdlength,
                                               uint8_t* rdata) {
    struct resource_record* rr = malloc(sizeof(struct resource_record));
    int dlen = domain_len(name);
    if (dlen == -1) {
        LOG_ERR("err creating an rr");
        exit(EXIT_FAILURE);
    }
    rr->name = malloc(dlen * sizeof(uint8_t));
    rr->rdata = malloc(rdlength * sizeof(uint8_t));
    memcpy(rr->name, name, dlen);
    memcpy(rr->rdata, rdata, rdlength);
    rr->type = type;
    rr->_class = _class;
    rr->ttl = ttl;
    rr->rdlength = rdlength;
    return rr;
}

void domain_rev(uint8_t* dest, uint8_t* src) {
    char stack[DOMAIN_BUF_LEN] = {0};
    int sptr = 0;
    uint8_t n;
    while ((n = *src)) {
        for (size_t i = 1; i <= n; i++) {
            stack[sptr++] = src[i];
        }
        src += n + 1;
        stack[sptr++] = n;
    }
    sptr--;
    uint32_t desti = 0;
    while (sptr > 0 && (n = stack[sptr])) {
        for (size_t i = 1; i <= n; i++) {
            dest[i] = stack[sptr - n + i - 1];
        }
        dest[0] = n;
        dest += n + 1;
        sptr -= n + 1;
    }
    dest[0] = 0;
}