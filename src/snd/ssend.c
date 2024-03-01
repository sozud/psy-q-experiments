#include "libsnd_i.h"

#if VERSION == 35
void SsEnd(void) {
    if (_snd_seq_tick_env.unk4 == 0) {
        _snd_seq_tick_env.unk17 = 0;
        EnterCriticalSection();
        if (_snd_seq_tick_env.unk16 != 0) {
            VSyncCallback(NULL);
            _snd_seq_tick_env.unk16 = 0;
        } else if (_snd_seq_tick_env.unk18 == 0) {
            InterruptCallback(0, _snd_seq_tick_env.unk12);
            _snd_seq_tick_env.unk12 = 0;
        } else {
            InterruptCallback(6, NULL);
        }
        ExitCriticalSection();
        _snd_seq_tick_env.unk18 = 0xFF;
    }
}
#else
void SsEnd(void) {
    if (_snd_seq_tick_env.unk4 == 0) {
        _snd_seq_tick_env.unk17 = 0;
        if (_snd_seq_tick_env.unk18 != 0x7F) {
            EnterCriticalSection();
            if (_snd_seq_tick_env.unk16 != 0) {
                VSyncCallback(NULL);
                _snd_seq_tick_env.unk16 = 0;
            } else if (_snd_seq_tick_env.unk18 == 0) {
                InterruptCallback(0, _snd_seq_tick_env.unk12);
                _snd_seq_tick_env.unk12 = 0;
            } else {
                InterruptCallback(6, NULL);
            }
            ExitCriticalSection();
            _snd_seq_tick_env.unk18 = 0x7F;
        }
    }
}
#endif