#include "kl.h"
#include "ka.h"
#include "km.h"
#ifndef NOGUI
# include "ui.h"
#endif // NOGUI

bool KL_active = false;
HHOOK KL_handle;

KM KL_km_shift;
KM KL_km_control;
KM KL_km_alt;
KM KL_km_l3;
KM KL_km_l5;

KLY KL_kly;

UCHAR KL_phys[255];
UCHAR KL_phys_mods[255];

void KL_activate() {
    KL_handle = SetWindowsHookEx(WH_KEYBOARD_LL, KL_proc, nil, 0);
    KL_active = true;
#ifndef NOGUI
    UI_TR_update();
#endif // NOGUI
}

void KL_deactivate() {
    UnhookWindowsHookEx(KL_handle);
    KL_active = false;
#ifndef NOGUI
    UI_TR_update();
#endif // NOGUI
}

void KL_toggle() {
    if (KL_active) {
        KL_deactivate();
    } else {
        KL_activate();
    }
}

#define RawThisEvent() 0
#define StopThisEvent() 1
#define PassThisEvent() CallNextHookEx(NULL, aCode, wParam, lParam)
LRESULT CALLBACK KL_proc(int aCode, WPARAM wParam, LPARAM lParam) {
    if (aCode != HC_ACTION)
        return PassThisEvent();
    PKBDLLHOOKSTRUCT ev = (PKBDLLHOOKSTRUCT) lParam;
    DWORD flags = ev->flags;
    SC sc = (SC) ev->scanCode;
    //VK vk = (VK) ev->vkCode;
    bool down = (wParam != WM_KEYUP && wParam != WM_SYSKEYUP);
    // non-physical key events:
    //   injected key presses (generated by programs - keybd_event(), SendInput()),
    //   their (non-injected) release counterparts,
    //   fake shift presses and releases by driver accompanying numpad keys
    //     (so that numlock'ed keys are independent of shift state yet have the same 2 levels),
    //   LControl press/release by OS (the window system) triggered by AltGr RAlt event
    // Only check here for injected presses and corresponding releases.
    bool faked;
    faked = (flags & LLKHF_INJECTED || (!(KL_phys[sc]) && !down));
    if (flags & LLKHF_EXTENDED)
        sc |= 0x100;
    if (faked || sc >= KPN) {
        return PassThisEvent();
    }

    KL_phys[sc] = down;
    {
        BYTE mod = KL_phys_mods[sc];
        if (mod) {
            switch (mod) {
            case MOD_SHIFT:
                KM_shift_event(&KL_km_shift, down, sc);
                break;
            case MOD_CONTROL:
                KM_shift_event(&KL_km_control, down, sc);
                break;
            case MOD_ALT:
                KM_shift_event(&KL_km_alt, down, sc);
                break;
            }
            return PassThisEvent();
        } else {
            KM_nonmod_event(&KL_km_shift, down, sc);
            KM_nonmod_event(&KL_km_l3, down, sc);
        }
    }

    unsigned char lv = 0;
    if (KL_km_l3.in_effect) {
        lv = 2;
    } else if (KL_km_l5.in_effect) {
        lv = 4;
    }
    if (KL_km_shift.in_effect) {
        lv += 1;
    }

    LK lk = KL_kly[lv][sc];
    dput(" l%db%x", lv, lk.binding);

    if (!lk.active) {
        dput(" na%s", (down ? "_ " : "^\n"));
        return PassThisEvent();
    }

    UCHAR mods = lk.mods;
    if (mods == KLM_WCHAR) {
        INPUT inp;
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = 0;
        inp.ki.dwFlags = KEYEVENTF_UNICODE;
        inp.ki.dwExtraInfo = 0;
        inp.ki.wScan = sc;
        inp.ki.time = GetTickCount();
        SendInput(1, &inp, sizeof(INPUT));
    } else if (mods & KLM_KA) {
        dput(" ka_call ka%d(%d){", lk.binding, down);
        KA_call(lk.binding, down, sc);
        dput("}%s", (down ? "_" : "^\n"));
    } else {
        char mod_shift = (mods & MOD_SHIFT) ? (KL_km_shift.in_effect ? 0 : 1) : (KL_km_shift.in_effect ? -1 : 0), mod_shift0 = mod_shift;
        char mod_control = (((mods & MOD_CONTROL) && !KL_km_control.in_effect) ? 1 : 0), mod_control0 = mod_control;
        char mod_alt = (((mods & MOD_ALT) && !KL_km_alt.in_effect) ? 1 : 0), mod_alt0 = mod_alt;
        int mods_count = (mod_shift & 1) + mod_control + mod_alt;
        dput(" send vk%02x[%d%d%d]%s", lk.binding, mod_shift, mod_control, mod_alt, (down ? "_" : "^\n"));
        if (mods_count) {
            INPUT inps[7];
            int tick_count = GetTickCount();
            int i;
            int inps_count = 1 + mods_count * 2;
            fori (i, 0, inps_count) {
                VK vk1 = lk.binding;
                DWORD flags = 0;
                if (mod_shift) {
                    dput("+%d-", mod_shift);
                    flags = (mod_shift > 0 ? 0 : KEYEVENTF_KEYUP);
                    mod_shift = 0;
                    vk1 = VK_LSHIFT;
                } else if (mod_control) {
                    dput("^");
                    flags = (mod_control > 0 ? 0 : KEYEVENTF_KEYUP);
                    mod_control = 0;
                    vk1 = VK_LCONTROL;
                } else if (mod_alt) {
                    dput("!");
                    flags = (mod_alt > 0 ? 0 : KEYEVENTF_KEYUP);
                    mod_alt = 0;
                    vk1 = VK_RMENU;
                } else {
                    dput("-");
                    mod_shift = -mod_shift0;
                    mod_control = -mod_control0;
                    mod_alt = -mod_alt0;
                    vk1 = lk.binding;
                    flags = (down ? 0 : KEYEVENTF_KEYUP);
                }
                INPUT *inp = &(inps[i]);
                inp->type = INPUT_KEYBOARD;
                inp->ki.wVk = vk1;
                inp->ki.dwFlags = flags;
                inp->ki.dwExtraInfo = 0;
                inp->ki.wScan = sc;
                inp->ki.time = tick_count;
            }

            SendInput(inps_count, inps, sizeof(INPUT));
        } else {
            keybd_event(lk.binding, sc, (down ? 0 : KEYEVENTF_KEYUP), 0);
        }
    }

    return StopThisEvent();
}
#undef RawThisEvent
#undef StopThisEvent
#undef PassThisEvent

KLY *KL_bind_kly = &KL_kly;
bool KL_bind_lvls[KLVN];

void KL_bind(SC sc, UINT mods, SC binding) {
    LK lk;
    UINT lv;
    dput("bind sc%03x ", sc);
    fori (lv, 0, len(KL_bind_lvls)) {
        int lv1 = lv+1;
        if (!KL_bind_lvls[lv]) {
            dput(":%d - ", lv1);
            continue;
        }
        SC binding1 = binding;
        lk.active = true;
        lk.mods = mods | KLM_SC;
        if (mods & KLM_SC) {
            binding1 = OS_sc_to_vk(binding);
        }
        lk.binding = binding1;
        if (lv % 2) {
            UINT mods0 = mods;
            if (KL_bind_lvls[lv - 1]) {
                mods |= MOD_SHIFT;
            }
            dput("+(%d:%d)", mods0, mods);
        }
        (*KL_bind_kly)[lv][sc] = lk;
        if (mods & KLM_WCHAR) {
            dput(":%d u%04x ", lv1, binding);
        } else if (mods & KLM_SC) {
            dput(":%d sc%03x=>vk%02x ", lv1, binding, binding1);
        } else if (mods & KLM_KA) {
            dput(":%d ka%d ", lv1, binding);
        } else {
            dput(":%d vk%02x ", lv1, binding);
        }
    }
    dputs("");
}

typedef struct {
    bool compiled;
    // Primary Language ID
    LANGID lang;
    // Bindings
    KLY kly;
} KLC;

size_t KL_klcs_size = 0;
size_t KL_klcs_count = 0;
KLC *KL_klcs = (KLC*)calloc((KL_klcs_size = 4), sizeof(KLC));

void KL_add_lang(LANGID lang) {
    if ((KL_klcs_count+=1) > KL_klcs_size) {
        KL_klcs = (KLC*)realloc(KL_klcs, (KL_klcs_size *= 1.5) * sizeof(KLC));
    }
    KLC *klc = KL_klcs + KL_klcs_count - 1;
    klc->compiled = false;
    klc->lang = lang;
}

KLY *KL_lang_to_kly(LANGID lang) {
    UINT i;
    fori(i, 0, KL_klcs_count) {
        if (KL_klcs[i].lang == lang)
            return &(KL_klcs[i].kly);
    }
    return nil;
}

KLC *KL_lang_to_klc(LANGID lang) {
    UINT i;
    fori(i, 0, KL_klcs_count) {
        if (KL_klcs[i].lang == lang)
            return &(KL_klcs[i]);
    }
    return nil;
}


void KL_compile_klc(KLC *klc) {
    if (klc->lang == LANG_NEUTRAL)
        return;
    KLY *kly = &(klc->kly);
    CopyMemory(KL_kly, KL_lang_to_kly(LANG_NEUTRAL), sizeof(KLY));
    int lv, sc;
    fori(lv, 0, KLVN) {
        fori(sc, 0, KPN) {
            LK lk = (*kly)[lv][sc];
            if (lk.active) {
                dput("sc%03x:%d ", sc, lv+1);
                KL_kly[lv][sc] = lk;
            }
        }
    }
    fori (lv, 0, KLVN) {
        fori (sc, 0, KPN) {
            LK *p_lk = &(KL_kly[lv][sc]), lk = *p_lk;
            if (lk.active) {
                //dput("a(%03x:%d:%x/%x)", sc, lv+1, lk.binding, lk.mods);
                if (lk.mods & KLM_WCHAR) {
                    WCHAR w = lk.binding;
                    dput("sc%03x:%d->", sc, lv+1);
                    KP kp = OS_wchar_to_vk(w);
                    if (kp.vk) {
                        dput("vk%02x/%d", kp.vk, kp.mods);
                        lk.mods = kp.mods;
                        lk.binding = kp.vk;
                    }
                    dput(";");
                }
                *p_lk = lk;
            }
        }
    }
    CopyMemory(kly, KL_kly, sizeof(KLY));
    klc->compiled = true;
}

void KL_activate_lang(LANGID lang) {
    dput("lang %04d ", lang);
    KLC *lang_klc = KL_lang_to_klc(lang);
    if (lang_klc->compiled) {
        CopyMemory(KL_kly, lang_klc->kly, sizeof(KLY));
    } else {
        dput("compile ");
        KL_compile_klc(lang_klc);
    }
    dputs("");
}

void KL_set_bind_lang(LANGID lang) {
    KLY *kly = KL_lang_to_kly(lang);
    if (kly == nil) {
        KL_add_lang(lang);
        kly = KL_lang_to_kly(lang);
    }
    KL_bind_kly = kly;
}

void KL_bind_lvls_zero() {
    UINT i;
    fori (i, 0, len(KL_bind_lvls)) {
        KL_bind_lvls[i] = false;
    }
}

void KL_bind_lvls_init() {
    KL_bind_lvls_zero();
    KL_bind_lvls[0] = true;
    KL_bind_lvls[1] = true;
}

void KL_init() {
    ZeroMemory(KL_kly, sizeof(KL_kly));
    ZeroMemory(KL_phys, sizeof(KL_phys));

    KM_init(&KL_km_shift);
    KM_init(&KL_km_control);
    KM_init(&KL_km_alt);
    KM_init(&KL_km_l3);
    KM_init(&KL_km_l5);

    KL_add_lang(LANG_NEUTRAL);
    KL_bind_kly = KL_lang_to_kly(LANG_NEUTRAL);
    UINT sc;
    fori (sc, 0, len(KL_phys_mods)) {
        VK vk = OS_sc_to_vk(sc);
        UCHAR mod = 0;
        switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
            mod = MOD_SHIFT;
            break;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
            mod = MOD_CONTROL;
            break;
        case VK_MENU: case VK_LMENU: case VK_RMENU:
            mod = MOD_ALT;
            break;
        case VK_LWIN: case VK_RWIN:
            mod = MOD_WIN;
            break;
        }
        KL_phys_mods[sc] = mod;
    }
}
