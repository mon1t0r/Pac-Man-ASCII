#pragma once

typedef enum {
    MENU_CODE_UPDATE,
    MENU_CODE_EXIT,
    MENU_CODE_GAME_START
} MenuCode;

void menu_init();
MenuCode menu_update();
void menu_free();