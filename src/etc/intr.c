#include "../types.h"
#include "etc.h"

typedef struct intrEnv_t intrEnv_t;
static Callback setIntr(s32 arg0, Callback arg1);
static void* startIntr();
static void* stopIntr();
static void* restartIntr();
static void memclr(void* ptr, s32 size);
static void trapIntr();
s32 setjmp(s32*);
void* startIntrVSync(); /* extern */
long long startIntrDMA();

static struct intrEnv_t intrEnv = {
    0, // interruptsInitialized
    0, // inInterrupt
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0}, // handlers (explicit zeros for each element)
    0,   // enabledInterruptsMask
    0,   // savedMask
    0,
};

static struct Callbacks callbacks = {
    "$Id: intr.c,v 1.73 1995/11/10 05:29:40 suzu Exp $",
    0,
    setIntr,
    startIntr,
    stopIntr,
    0,
    restartIntr,
    &intrEnv};

static struct Callbacks* pCallbacks = &callbacks;

void* ResetCallback(void) { return pCallbacks->ResetCallback(); }

void InterruptCallback(int irq, void (*f)()) {
    pCallbacks->InterruptCallback(irq, f);
}

void* DMACallback(int dma, void (*func)()) {
    return pCallbacks->DMACallback(dma, func);
}

void VSyncCallback(void (*f)()) { pCallbacks->VSyncCallbacks(0, f); }

int VSyncCallbacks(int ch, void (*f)()) {
    return pCallbacks->VSyncCallbacks(ch, f);
}

void* StopCallback(void) { return pCallbacks->StopCallback(); }

void* RestartCallback(void) { return pCallbacks->RestartCallback(); }

int CheckCallback(void) { return intrEnv.inInterrupt; }

static volatile u16* i_mask = (u16*)0x1F801070;
static volatile u16* g_InterruptMask = (u16*)0x1F801074;
static volatile s32* d_pcr = (s32*)0x1F8010F0;

u16 GetIntrMask(void) { return *g_InterruptMask; }

u16 SetIntrMask(u16 arg0) {
    u16 mask;

    mask = *g_InterruptMask;
    *g_InterruptMask = arg0;
    return mask;
}

void* startIntr() {
    if (intrEnv.interruptsInitialized != 0) {
        return NULL;
    }
    *i_mask = *g_InterruptMask = 0;
    *d_pcr = 0x33333333;
    memclr(&intrEnv, sizeof(intrEnv) / sizeof(s32));
    if (setjmp(intrEnv.buf) != 0) {
        trapIntr();
    }
    intrEnv.buf[JB_SP] = (s32)&intrEnv.stack[1004];
    HookEntryInt((u16*)intrEnv.buf);
    intrEnv.interruptsInitialized = 1;
    pCallbacks->VSyncCallbacks = startIntrVSync();
    pCallbacks->DMACallback = startIntrDMA();
    _96_remove();
    ExitCriticalSection();
    return &intrEnv;
}

static s32 D_8002D350 = 0;

void trapIntr() {
    s32 i;
    u16 mask;

    if (intrEnv.interruptsInitialized == 0) {
        printf("unexpected interrupt(%04x)\n", *i_mask);
        ReturnFromException();
    }
    intrEnv.inInterrupt = 1;
    mask = (intrEnv.enabledInterruptsMask & *i_mask) & *g_InterruptMask;
    while (mask != 0) {
        for (i = 0; mask && i < 11; ++i, mask >>= 1) {
            if (mask & 1) {
                *i_mask = ~(1 << i);
                if (intrEnv.handlers[i] != NULL) {
                    intrEnv.handlers[i]();
                }
            }
        }
        mask = (intrEnv.enabledInterruptsMask & *i_mask) & *g_InterruptMask;
    }
    if (*i_mask & *g_InterruptMask) {
        if (D_8002D350++ > 0x800) {
            printf("intr timeout(%04x:%04x)\n", *i_mask, *g_InterruptMask);
            D_8002D350 = 0;
            *i_mask = 0;
        }
    } else {
        D_8002D350 = 0;
    }
    intrEnv.inInterrupt = 0;
    ReturnFromException();
}

Callback setIntr(s32 arg0, Callback arg1) {
    Callback temp_s4;
    u16 temp_v1;
    s32 var_s3;

    temp_s4 = intrEnv.handlers[arg0];
    if ((arg1 != temp_s4) && (intrEnv.interruptsInitialized != 0)) {
        temp_v1 = *g_InterruptMask;
        *g_InterruptMask = 0;
        var_s3 = temp_v1 & 0xFFFF;
        if (arg1 != 0) {
            intrEnv.handlers[arg0] = arg1;
            var_s3 = var_s3 | (1 << arg0);
            intrEnv.enabledInterruptsMask |= (1 << arg0);
        } else {
            intrEnv.handlers[arg0] = 0;
            var_s3 = var_s3 & ~(1 << arg0);
            intrEnv.enabledInterruptsMask &= ~(1 << arg0);
        }
        if (arg0 == 0) {
            ChangeClearPAD(arg1 == 0);
            ChangeClearRCnt(3, arg1 == 0);
        }
        if (arg0 == 4) {
            ChangeClearRCnt(0, arg1 == 0);
        }
        if (arg0 == 5) {
            ChangeClearRCnt(1, arg1 == 0);
        }
        if (arg0 == 6) {
            ChangeClearRCnt(2, arg1 == 0);
        }
        *g_InterruptMask = var_s3;
    }
    return temp_s4;
}

void* stopIntr() {
    if (intrEnv.interruptsInitialized == 0) {
        return NULL;
    }
    EnterCriticalSection();
    intrEnv.savedMask = *g_InterruptMask;
    intrEnv.savedPcr = *d_pcr;
    *i_mask = *g_InterruptMask = 0;
    *d_pcr &= 0x77777777;
    ResetEntryInt();
    intrEnv.interruptsInitialized = 0;
    return &intrEnv;
}

void* restartIntr() {
    if (intrEnv.interruptsInitialized != 0) {
        return 0;
    }

    HookEntryInt((u16*)intrEnv.buf);
    intrEnv.interruptsInitialized = 1;
    *g_InterruptMask = intrEnv.savedMask;
    *d_pcr = intrEnv.savedPcr;
    ExitCriticalSection();
    return &intrEnv;
}

void memclr(void* ptr, s32 size) {
    s32 i;
    s32* e = (s32*)ptr;

    for (i = size - 1; i != -1; i--) {
        *e = 0;
        e++;
    }
}
