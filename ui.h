#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#include "stdafx.h"

extern HWND UI_MW;

#define UI_TRAY_MSG (WM_APP + 1)

#define UI_STR_MAXLEN 128
typedef char UI_STR[UI_STR_MAXLEN];

bool UI_init();
void UI_spawn();
void UI_quit(int);

void UI_TR_delete();
bool UI_TR_update();

#endif // UI_H_INCLUDED