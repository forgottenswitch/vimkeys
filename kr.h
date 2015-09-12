#ifndef KR_H_INCLUDED
#define KR_H_INCLUDED

#include "stdafx.h"

#define KR_MAXTITLE 64
#define KR_MAXCLASS 32

extern bool KR_active;
extern size_t KR_id;

void KR_activate();
void KR_deactivate();
void KR_toggle();
void KR_clear();
void KR_resume(bool on_pt_only);
void KR_on_task_switch(HWND hwnd, char *wndclass, bool on_pt_only);

void KR_add_app();
void KR_set_bind_title(char *title);
void KR_set_bind_class(char *wndclass);
void KR_bind(SC sc, SC binding, USHORT mods);
void KR_add_res(USHORT x, USHORT y);

void KR_init();

#endif // KR_H_INCLUDED
