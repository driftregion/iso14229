#ifndef SHARED_H
#define SHARED_H

/**
 * | Addr  | Description                        |
 * |-------|------------------------------------|
 * | 0x7E0 | Server 1 Physical Receive Address  |
 * | 0x7E1 | Server 2 Physical Receive Address  |
 * | 0x7E8 | Server 1 Physical Transmit Address |
 * | 0x7E9 | Server 2 Physical Transmit Address |
 * | 0x7DF | Functional Receive Address         |
 *
 *          |--0x7DF->|
 *         -|--0x7E0->| Server 1
 *        / |<-0x7E8--|
 *  Client
 *        \ |--0x7DF->|
 *         -|--0x7E1->| Server 2
 *          |<-0x7E9--|
 *
 *  Client has one link that strictly sends functional requests to 0x7DF.
 *  Functional responses are received on the other links
 *
 */

// 服务器响应地址
#define SERVER_SEND_ID (0x7E8) /* server sends */
// 服务器物理接收地址 (1:1)
#define SERVER_PHYS_RECV_ID (0x7E0) /* server listens for physically (1:1) addressed messages */
// 服务器功能接收地址 (1:n)
#define SERVER_FUNC_RECV_ID (0x7DF) /* server listens for functionally (1:n) addressed messages */

#define CLIENT_DEFAULT_P2_MS 150
#define CLIENT_DEFAULT_P2_STAR_MS 1500
#define SERVER_DEFAULT_P2_MS 50
#define SERVER_DEFAULT_P2_STAR_MS 2000
#define SERVER_DEFAULT_S3_MS 5000

#define DID_0x0001_LEN 1U
#define DID_0x0008_LEN 21U

#define RID_TERMINATE_PROCESS 0x1234

#endif
