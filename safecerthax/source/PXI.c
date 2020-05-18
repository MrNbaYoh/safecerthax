/*
PXI.c:
    PXI I/O functions.

(c) TuxSH, 2016-2017
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#include "PXI.h"

void PXIReset(void)
{
    REG_PXI_SYNC = 0;
    REG_PXI_CNT = CNT_CLEAR_SEND_FIFO;

    for(u32 i = 0; i < 16; i += 2)
    {
        REG_PXI_RECV;
        REG_PXI_RECV;
    }

    REG_PXI_CNT = 0;
    REG_PXI_CNT = CNT_ENABLE_FIFOs | CNT_ACKNOWLEDGE_FIFO_ERROR | CNT_CLEAR_SEND_FIFO;
}

void PXITriggerSync11IRQ(void)
{
    REG_PXI_INTERRUPT_CNT |= SYNC_TRIGGER_SYNC11_IRQ;
}

bool PXIIsSendFIFOEmpty(void)
{
    return (REG_PXI_CNT & CNT_SEND_FIFO_EMPTY_STATUS) != 0;
}

bool PXIIsSendFIFOFull(void)
{
    return (REG_PXI_CNT & CNT_SEND_FIFO_FULL_STATUS) != 0;
}

void PXISendByte(u8 byte)
{
    REG_PXI_BYTE_SENT_TO_REMOTE = byte;
}

void PXISendWord(u32 word)
{
    while(REG_PXI_CNT & CNT_SEND_FIFO_FULL_STATUS);
    REG_PXI_SEND = word;
}

void PXISendBuffer(const u32 *buffer, u32 nbWords)
{
    for(; nbWords > 0; nbWords--)
    {
        while(REG_PXI_CNT & CNT_SEND_FIFO_FULL_STATUS);
        REG_PXI_SEND = *buffer++;
    }
}

bool PXIIsReceiveFIFOEmpty(void)
{
    return (REG_PXI_CNT & CNT_RECEIVE_FIFO_EMPTY_STATUS) != 0;
}

u8 PXIReceiveByte(void)
{
    return REG_PXI_BYTE_RECEIVED_FROM_REMOTE;
}

u32 PXIReceiveWord(void)
{
    while(REG_PXI_CNT & CNT_RECEIVE_FIFO_EMPTY_STATUS);
    return REG_PXI_RECV;
}

void PXIReceiveBuffer(u32 *buffer, u32 nbWords)
{
    for(; nbWords > 0; nbWords--)
    {
        while(REG_PXI_CNT & CNT_RECEIVE_FIFO_EMPTY_STATUS);
        *buffer++ = REG_PXI_RECV;
    }
}
