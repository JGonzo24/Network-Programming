#ifndef __SREJ_H__
#define __SREJ_H__

#include "networks.h"
#include "cpe464.h"



#define MAX_FILE_LEN 100
#define RR_PDU_LEN 11
#define SREJ_PDU_LEN 11

#define START_SEQ_NUM 1
typedef struct header Header;

struct header
{
    uint32_t seq_num;
    uint16_t checksum;
    uint8_t flag;
};

enum FLAG
{
    RR=5, SREJ=6, FNAME=8, FNAME_OK=9, EOF_FLAG=10, DATA=16, RESENT_DATA=17, RESEND_TIMEOUT=18, FNAME_NOT_OK=37, FILE_OK_ACK=36, EOF_ACK=38

};

#endif