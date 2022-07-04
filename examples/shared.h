#ifndef SHARED_H
#define SHARED_H

// 服务器物理接收地址 (1:1)
// server listens for physical (1:1) messages on this CAN ID
#define SRV_PHYS_RECV_ID 0x701

// 服务器功能接收地址 (1:n)
// server listens for functional (1:n) messages on this CAN ID
#define SRV_FUNC_RECV_ID 0x702

// 服务器响应地址
// server responds on this CAN ID
#define SRV_SEND_ID 0x700

#define CLIENT_PHYS_SEND_ID SRV_PHYS_RECV_ID
#define CLIENT_FUNC_SEND_ID SRV_FUNC_RECV_ID
#define CLIENT_RECV_ID SRV_SEND_ID

#define CLIENT_DEFAULT_P2_MS 150
#define CLIENT_DEFAULT_P2_STAR_MS 1500
#define SERVER_DEFAULT_P2_MS 50
#define SERVER_DEFAULT_P2_STAR_MS 2000
#define SERVER_DEFAULT_S3_MS 5000

#define DID_0x0001_LEN 1U
#define DID_0x0008_LEN 21U

#define RESET_TYPE_EXIT 4U

#endif
