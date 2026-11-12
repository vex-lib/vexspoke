#ifndef NET_FRAME_INBOX_H
#define NET_FRAME_INBOX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vex.h"
#include "re/type/re_type.h"

// 0x0036 is the next free Vexspoke class after ID_PROCESS_SPAWN (0x0035).
#define TYPE_FRAME_INBOX_SINGLETON Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, 0x0036u)

#define FRAME_INBOX_RX_CAP 4096u
#define FRAME_INBOX_POLL_MAX_NS 100000000ULL

typedef enum FrameInboxState {
    FRAME_INBOX_IDLE = 0,
    FRAME_INBOX_CONNECTING = 1,
    FRAME_INBOX_OPEN = 2,
    FRAME_INBOX_CLOSING = 3,
    FRAME_INBOX_CLOSED = 4
} FrameInboxState;

;;CLASSNAME("FrameInbox")
typedef struct FrameInbox {
    FrameInboxState state;
    u8 rxBuf[FRAME_INBOX_RX_CAP];
    u32 rxLen;
    u64 timeoutNs;
    bool cancelled;
} FrameInbox;

FrameInbox *FrameInbox_0();
FrameInbox *FrameInbox_1(u64 timeoutNs);
#define FrameInbox(...) CONSTRUCTOR_DISPATCH(FrameInbox, __VA_ARGS__)

void FrameInbox_free(FrameInbox *self);
bool FrameInbox_feed(FrameInbox *self, const u8 *Bytes, u32 len);
bool FrameInbox_poll(FrameInbox *self, u64 timeoutNs, u8 *dest,
                     u32 destCap, u32 *outLen);
void FrameInbox_cancel(FrameInbox *self);
void FrameInbox_clear(FrameInbox *self);

void FrameInbox_setState(FrameInbox *self, FrameInboxState state);
FrameInboxState FrameInbox_getState(const FrameInbox *self);
void FrameInbox_setTimeoutNs(FrameInbox *self, u64 timeoutNs);
u64 FrameInbox_getTimeoutNs(const FrameInbox *self);
void FrameInbox_setCancelled(FrameInbox *self, bool cancelled);
bool FrameInbox_isCancelled(const FrameInbox *self);
u32 FrameInbox_getPending(const FrameInbox *self);

#endif
