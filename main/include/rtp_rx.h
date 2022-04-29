#ifndef RTP_RX_H
#define RTP_RX_H

struct sockaddr_in;

void rtp_rx_init(void);
void on_udpserver_recv(struct sockaddr_in *addr, char *pusrdata, unsigned short len);

#endif
