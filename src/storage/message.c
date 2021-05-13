#include "storage/message.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logging.h"

// deprecated
int domain_len(const uint8_t* domain) {
    // FIXME: remove this func
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
    uint16_t dest = *(uint16_t*)ptr;
    // memcpy(&dest, ptr, sizeof(uint16_t));
    dest = ntohs(dest);
    return dest;
}
static uint8_t u8n_to_u8h(const uint8_t* ptr) {
    uint8_t dest = *(uint8_t*)ptr;
    // memcpy(&dest, ptr, sizeof(uint8_t));
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
int mq_copy(struct message_question* dest, const struct message_question* src) {
    memcpy(dest, src, sizeof(struct message_question));
    int dlen = domain_len(src->qname);
    if (dlen < 0) {
        return -1;
    }
    dest->qname = malloc(sizeof(uint8_t) * dlen);
    memcpy(dest->qname, src->qname, dlen);
    return 1;
}
int rr_copy(struct resource_record* dest, const struct resource_record* src) {
    memcpy(dest, src, sizeof(struct resource_record));
    int dlen = domain_len(src->name);
    if (dlen < 0) {
        return -1;
    }
    dest->name = malloc(sizeof(uint8_t) * dlen);
    if (src->rdlength) {
        dest->rdata = malloc(sizeof(uint8_t) * src->rdlength);
        memcpy(dest->rdata, src->rdata, sizeof(uint8_t) * src->rdlength);
    } else {
        dest->rdata = NULL;
    }

    memcpy(dest->name, src->name, sizeof(uint8_t) * dlen);
    return 1;
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
// returns the actual length of domain name(includes the '\0')
// -1 if err
static int read_domain(const uint8_t* base, const uint8_t* src, char* dest,
                       int32_t* _offset) {
    int buf_domain_len = 1;
    // we manually copy domain to support pointer
    while (*src) {
        if ((((*src) >> 6) & 0x3) == 0x3) {
            uint8_t sr = ((*src) >> 6) & 0x3;
            // pointer detected, make a recursive query
            uint16_t offset =
                (((uint16_t)*src) << 8 | ((uint16_t)(*(src + 1)))) & 0x3fff;
            int _ = 0;
            int dlen = read_domain(base, base + offset, dest, &_) - 1;
            dest += dlen;
            // the pointer actually provides 2 offsets
            *_offset += 1;
            src += 2;
            buf_domain_len += dlen;
            break;
        } else {
            *dest = *src;
            dest++;
            src++;
            buf_domain_len++;
            *_offset += 1;
        }
    }
    *_offset += 1;
    return buf_domain_len;
}
// fetch message question info from buffer
// this function allocates _qname_ on the heap!
// returns n bytes it reads in _buf_
// err if -1
static int msg_question_from_buf(const uint8_t* base, const uint8_t* buf,
                                 struct message_question* msg_qd) {
    // with the last '\0'
    uint8_t domainbuf[DOMAIN_BUF_LEN] = {0};
    uint32_t offset = 0;
    int qname_len = read_domain(base, buf, domainbuf, &offset);
    if (qname_len < 0) {
        return -1;
    }

    uint8_t* qname = malloc(qname_len * sizeof(uint8_t));
    memcpy(qname, domainbuf, qname_len * sizeof(uint8_t));
    uint16_t qtype = u8n_to_u16h(buf + offset);
    uint16_t qclass = u8n_to_u16h(buf + offset + sizeof(qtype));
    msg_qd->qname = qname;
    msg_qd->qtype = qtype;
    msg_qd->qclass = qclass;
    return offset + sizeof(qtype) + sizeof(qclass);
}

static int msg_rr_from_buf(const uint8_t* base, const uint8_t* buf,
                           struct resource_record* rr) {
    uint8_t domainbuf[DOMAIN_BUF_LEN] = {0};
    int offset = 0;
    int name_len = read_domain(base, buf, domainbuf, &offset);
    if (name_len < 0) {
        return -1;
    }

    uint8_t* name = malloc(name_len * sizeof(uint8_t));
    memcpy(name, domainbuf, name_len * sizeof(uint8_t));
    uint32_t ttl;
    uint16_t type, _class, rdlength, real_rdlength;
    type = u8n_to_u16h(buf += offset);
    _class = u8n_to_u16h(buf += sizeof(type));
    ttl = u8n_to_u32h(buf += sizeof(_class));
    rdlength = u8n_to_u16h(buf += sizeof(ttl));
    buf += sizeof(rdlength);
    uint8_t* rdata = NULL;

    uint8_t rrbuf[RR_BUF_SIZE] = {0};
    switch (type) {
        case RRTYPE_SOA:;
            int mname_offset = 0, rname_offset = 0;
            int mname_len = read_domain(base, buf, rrbuf, &mname_offset);
            int rname_len = read_domain(base, buf += mname_offset,
                                        rrbuf + mname_len, &rname_offset);
            memcpy(rrbuf + mname_len + rname_len,
                   buf + mname_offset + rname_offset, sizeof(uint32_t) * 5);
            real_rdlength = mname_len + rname_len + sizeof(uint32_t) * 5;
            LOG_INFO("real: %u, rdlen: %u\n", real_rdlength, rdlength);
            break;
        case RRTYPE_MX:;
            memcpy(rrbuf, buf, sizeof(uint16_t));
            buf += sizeof(uint16_t);
            int exchange_offset = 0;
            int exchange_len = read_domain(base, buf, rrbuf + sizeof(uint16_t),
                                           &exchange_offset);
            real_rdlength = exchange_len + sizeof(uint16_t);
            rdata = malloc(real_rdlength);
            break;
        case RRTYPE_NS:
        // slide through
        case RRTYPE_PTR:
        // slide through
        case RRTYPE_TXT:
        // slide through
        case RRTYPE_CNAME:;
            int _offset = 0;
            int _len = read_domain(base, buf, rrbuf, &_offset);
            real_rdlength = _len;
            LOG_INFO("real: %u, rdlen: %u\n", real_rdlength, rdlength);
            break;
        default:
            real_rdlength = rdlength;
            memcpy(rrbuf, buf, rdlength);
            break;
    }
    if (real_rdlength) {
        rdata = malloc(real_rdlength);
        memcpy(rdata, rrbuf, real_rdlength);
    } else {
        rdata = NULL;
    }

    rr->name = name;
    rr->_class = _class;
    rr->rdata = rdata;
    rr->type = type;
    rr->ttl = ttl;
    rr->rdlength = real_rdlength;
    LOG_INFO(
        "name = %s, type = %u, class = %u, ttl = %u, rdlength = %u, rdata = "
        "%s\n",
        name, type, _class, ttl, rdlength, rdata);
    return offset + sizeof(type) + sizeof(_class) + sizeof(ttl) +
           sizeof(rdlength) + rdlength;
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
        int n = msg_question_from_buf(buf, bufptr, msg->question[i]);
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
        int n = msg_rr_from_buf(buf, bufptr, msg->answer[i]);
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
        int n = msg_rr_from_buf(buf, bufptr, msg->authority[i]);
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
        int n = msg_rr_from_buf(buf, bufptr, msg->addition[i]);
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
            LOG_ERR("invalid question**\n");
            return -1;
        }

        for (size_t i = 0; i < msg->header.qdcount; i++) {
            if (!msg->question[i]) {
                LOG_ERR("invalid question*\n");
                return -1;
            }
            int tmp = 0;
            dest += (tmp = msg_question_to_u8(msg->question[i], dest));
            qsize += tmp;
        }
    }
    // rr answer
    if (msg->header.ancount) {
        if (!msg->answer) {
            LOG_ERR("invalid answer**\n");
            return -1;
        }

        for (size_t i = 0; i < msg->header.ancount; i++) {
            if (!msg->answer[i]) {
                LOG_ERR("invalid answer*\n");
                return -1;
            }
            int tmp = 0;
            dest += (tmp = msg_rr_to_u8(msg->answer[i], dest));
            ansize += tmp;
        }
    }
    if (msg->header.nscount) {
        if (!msg->authority) {
            LOG_ERR("invalid authority**\n");
            return -1;
        }

        for (size_t i = 0; i < msg->header.nscount; i++) {
            if (!msg->authority[i]) {
                LOG_ERR("invalid authority*\n");
                return -1;
            }
            int tmp = 0;
            dest += (tmp = msg_rr_to_u8(msg->authority[i], dest));
            nssize += tmp;
        }
    }
    if (msg->header.arcount) {
        if (!msg->addition) {
            LOG_ERR("invalid addition**\n");
            return -1;
        }

        for (size_t i = 0; i < msg->header.arcount; i++) {
            if (!msg->addition[i]) {
                LOG_ERR("invalid addition*\n");
                return -1;
            }
            int tmp = 0;
            dest += (tmp = msg_rr_to_u8(msg->addition[i], dest));
            arsize += tmp;
        }
    }
    return hsize + qsize + ansize + nssize + arsize;
}

void free_heap_message_question(struct message_question* mqptr) {
    if (!mqptr) {
        LOG_ERR("bad message question pointer\n");
        return;
    }

    if (mqptr->qname) {
        free(mqptr->qname);
        mqptr->qname = NULL;
    }
}

void free_heap_resource_record(struct resource_record* rrptr) {
    if (!rrptr) {
        LOG_ERR("bad resource record pointer\n");
        return;
    }

    if (rrptr->name) {
        free(rrptr->name);
        rrptr->name = NULL;
    }
    if (rrptr->rdata) {
        free(rrptr->rdata);
        rrptr->rdata = NULL;
    }
}
int free_heap_message(struct message* msgptr) {
    for (size_t i = 0; i < msgptr->header.qdcount; i++) {
        if (!msgptr->question || !msgptr->question[i]) {
            LOG_ERR("invalid qdcount");
            break;
        }
        free_heap_message_question(msgptr->question[i]);
        free(msgptr->question[i]);
        msgptr->question[i] = NULL;
    }
    if (msgptr->question) {
        free(msgptr->question);
        msgptr->question = NULL;
    }
    for (size_t i = 0; i < msgptr->header.ancount; i++) {
        if (!msgptr->answer || !msgptr->answer[i]) {
            LOG_ERR("invalid ancount");
            break;
        }
        free_heap_resource_record(msgptr->answer[i]);
        free(msgptr->answer[i]);
        msgptr->answer[i] = NULL;
    }
    if (msgptr->answer) {
        free(msgptr->answer);
        msgptr->answer = NULL;
    }

    for (size_t i = 0; i < msgptr->header.nscount; i++) {
        if (!msgptr->authority || !msgptr->authority[i]) {
            LOG_ERR("invalid nscount");
            break;
        }
        free_heap_resource_record(msgptr->authority[i]);
        free(msgptr->authority[i]);
        msgptr->authority[i] = NULL;
    }
    if (msgptr->authority) {
        free(msgptr->authority);
        msgptr->authority = NULL;
    }

    for (size_t i = 0; i < msgptr->header.arcount; i++) {
        if (!msgptr->addition || !msgptr->addition[i]) {
            LOG_ERR("invalid arcount");
            break;
        }
        free_heap_resource_record(msgptr->addition[i]);
        free(msgptr->addition[i]);
        msgptr->addition[i] = NULL;
    }
    if (msgptr->addition) {
        free(msgptr->addition);
        msgptr->addition = NULL;
    }
    return 1;
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
    return ((qr << 7) & QR_MASK) | ((opcode << 3) & OPCODE_MASK) |
           ((aa << 2) & AA_MASK) | ((tc << 1) & TC_MASK) | (rd & RD_MASK);
}

inline uint8_t create_misc2(uint8_t ra, uint8_t z, uint8_t rcode) {
    return ((ra << 7) & RA_MASK) | ((z << 4) & Z_MASK) | (rcode & RCODE_MASK);
}
struct resource_record* create_resource_record(uint8_t* name, uint16_t type,
                                               uint16_t _class, uint32_t ttl,
                                               uint16_t rdlength,
                                               uint8_t* rdata) {
    struct resource_record* rr = malloc(sizeof(struct resource_record));
    int dlen = domain_len(name);
    if (dlen == -1) {
        LOG_ERR("err creating an rr\n");
        pthread_exit(NULL);
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

uint8_t rr_cmp(struct resource_record* rr1, struct resource_record* rr2) {
    uint8_t e = 1;
    e = e && 0 == strncmp(rr1->name, rr2->name, domain_len(rr1->name));
    e = e && rr1->rdlength == rr2->rdlength;
    e = e && rr1->type == rr2->type;
    e = e && rr1->_class == rr2->_class;
    e = e && 0 == strncmp(rr1->rdata, rr2->rdata, rr1->rdlength);
    return e;
}

uint8_t check_blocked(struct resource_record* rr) {
    return *(uint32_t*)(rr->rdata) == 0;
}