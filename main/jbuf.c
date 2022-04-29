#include "jbuf.h"
#include "utils.h"

//#define LOCAL_DEBUG
#ifdef LOCAL_DEBUG
#	define TRACE(args) (ets_uart_printf("JB: "), ets_uart_printf args)
#else
#	define TRACE(args)
#endif
#	define MSG(args) (ets_uart_printf("JB: "), printf args)

static short buffer[JBUF_FRAME_SIZE*20];
/** Number of samples to collect before starting playing (on start on buffer underrun).
    You can reduce it to reduce delay on streaming if your WiFi is less crappy than mine
    (UPC Horizon DMC7002KLG: ping to router or device jumps randomly to 200 ms and more,
     high packet loss when communicating with internet).
 */
static const int PREFETCH_SIZE = JBUF_FRAME_SIZE*16;
static short zero_frame[JBUF_FRAME_SIZE] = {0};
static volatile int buffer_wr_pos = 0;
static volatile int buffer_rd_pos = 0;
static int overflow_cnt = 0;
static int underflow_cnt = 0;
static volatile int reset_req = 0;
enum JbufState {
    JbufIdle = 0,
    JbufBuffering,
    JbufPlaying
} jbufState = JbufIdle;

int jbuf_put(short sample) {
    if (reset_req) {
        return 0;
    }
    int new_wr_pos = buffer_wr_pos + 1;
    if (new_wr_pos >= sizeof(buffer)/sizeof(buffer[0])) {
        new_wr_pos = 0;
    }
    if (new_wr_pos == buffer_rd_pos) {
        overflow_cnt++;
        return -1;
    } else {
        buffer[buffer_wr_pos] = sample;
        buffer_wr_pos = new_wr_pos;
        if (jbufState == JbufIdle) {
            jbufState = JbufBuffering;
            TRACE(("Buffering\n"));
        }
        return 0;
    }
}

void jbuf_eop(void) {
    if (reset_req) {
        reset_req = 0;
        buffer_wr_pos = 0;
    }
}

short* jbuf_get(void) {
    int available;
    if (buffer_wr_pos >= buffer_rd_pos) {
        available = buffer_wr_pos - buffer_rd_pos;
    } else {
        available = sizeof(buffer)/sizeof(buffer[0]) - (buffer_rd_pos - buffer_wr_pos);
    }
    switch (jbufState)
    {
    case JbufIdle:
        break;
    case JbufBuffering:
        if (available >= PREFETCH_SIZE) {
            jbufState = JbufPlaying;
            TRACE(("Playing\n"));
        } else {
            underflow_cnt++;
            if (underflow_cnt > 200) {
                // reset state and jitter buffer completely
                TRACE(("Idle\n"));
                jbufState = JbufIdle;
                underflow_cnt = 0;
                buffer_rd_pos = 0;
                reset_req = 1;
            }
        }
        break;
    case JbufPlaying:
        if (available < JBUF_FRAME_SIZE) {
            jbufState = JbufBuffering;
            TRACE(("Underflow, buffering\n"));
            underflow_cnt++;
        } else {
            underflow_cnt = 0;
        }
        break;
    default:
        break;
    }

    if (jbufState == JbufPlaying) {
        TRACE(("g, av %d, t %u, pos %d\n", available, system_get_time(), buffer_rd_pos));
        int old_pos = buffer_rd_pos;
        int new_rd_pos = buffer_rd_pos + JBUF_FRAME_SIZE;
        if (new_rd_pos >= sizeof(buffer)/sizeof(buffer[0])) {
            buffer_rd_pos = 0;
        } else {
            buffer_rd_pos = new_rd_pos;
        }
        if (old_pos < 0 || old_pos > sizeof(buffer)/sizeof(buffer[0]) - JBUF_FRAME_SIZE) {
            TRACE(("Unexpected, pos = %d\n", old_pos));
        }
        return buffer + old_pos;
    } else {
        //ets_uart_printf("z");
    #if 0
        if (zero_frame[0] != 0 || zero_frame[1] != 0) {
            int i;
            for (i=0; i<ARRAY_SIZE(zero_frame); i++) {
                if (zero_frame[i] != 0) {
                    ets_uart_printf("WTF?\n");
                    break;
                }
            }
        }
    #endif // LOCAL_DEBUG
        return zero_frame;
    }
}

unsigned int jbuf_available(void)
{
    if (buffer_wr_pos >= buffer_rd_pos) {
        return buffer_wr_pos - buffer_rd_pos;
    } else {
        return sizeof(buffer)/sizeof(buffer[0]) - (buffer_rd_pos - buffer_wr_pos);
    }
}
