#ifndef _M5CP2_UI_H_
#define _M5CP2_UI_H_

#include "Minu/minu.hpp"
#include "config.h"

/// Menu and menu pages
extern Minu menu;
extern size_t homePageId;
extern size_t wifiPageId;
extern size_t wifiPromptPageId;
extern size_t scanResultPageId;
extern size_t saveSSIDAsPageId;
extern size_t pingTargetsPageId;
extern size_t routerPageId;
extern size_t routerInfoPageId;
extern size_t routerLocationPageId;
extern size_t routerWANListPageId;
extern size_t routerWANInfoPageId;
extern size_t routerWANSummaryPageId;
extern size_t timePageId;
extern size_t countdownPageId;
extern size_t fobInfoPageId;
extern size_t factoryResetPageId;
extern size_t simListPageId;
extern size_t simInfoPageId;
extern size_t lastVisitedPageId;

/// @brief Initialize the menu system and set up child pages and items
void uiMenuInit(void);

#endif