#ifndef JBUF_H
#define JBUF_H

enum {
    /** 20 ms data at 16ksps */
    JBUF_FRAME_SIZE = 320   /* samples */
};

/** \return non-zero on overflow */
int jbuf_put(short sample);
/** \brief Mark End-Of-Packet */
void jbuf_eop(void);
short* jbuf_get(void);
unsigned int jbuf_available(void);

#endif
