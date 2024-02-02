#define _POSIX_C_SOURCE 200201L
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include "menu.h"
#include "game.h"
#include "file_render.h"
#include "color_pair.h"
#include "exit_cause.h"

#define MENU_FRAME_TIME 0.05f
//game frame time / menu frame time coefficient (for entity rendering at the same speed as in-game)
#define MENU_FRAME_TIME_DIFF (int)(FRAME_TIME / MENU_FRAME_TIME)
#define MENU_HEIGHT 19
#define MAX_WORLD_COUNT 5

#define MAX_WORLD_FILE_NAME_LENGHT 50
#define MAX_WORLD_NAME_LENGHT 30

typedef enum {
    MENU_MAIN,
    MENU_WORLD_SELECTION
} MenuType;

typedef struct {
    char filename[MAX_WORLD_FILE_NAME_LENGHT];
    char name[MAX_WORLD_NAME_LENGHT];
    int name_lenght;
} WorldInfo;

struct timespec menu_sleep_ts = {
    .tv_sec = 0,
    .tv_nsec = MENU_FRAME_TIME / 4.0 * 1000000000L
};

unsigned int menu_frame_count = 0;

MenuType menu_type = MENU_MAIN;
int selected_button_index = 0;
int buttons_count = 0;

WorldInfo worlds[MAX_WORLD_COUNT];
int worlds_count = 0;

void menu_render();
void menu_pacman_anim_render(int x, int y, int len);
void menu_frame_render(int x, int y, int lenght, bool vertical);

void buttons_render(int x, int y);
void button_render(int x, int y, int index, const char *label);

int get_animation_offset(int timer);

void menu_change_type(MenuType type);

MenuCode input_handle();
MenuCode button_handle_click();

void menu_init()
{
    const char *worlds_dir = "./worlds/";
    int worlds_dir_len = strlen(worlds_dir);

    //read all world names from directory
    struct dirent *dirent;
    DIR *dir = opendir(worlds_dir);
    if (dir)
    {
        int name_lenght;
        worlds_count = 0;
        while ((dirent = readdir(dir)) != NULL)
        {
            if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
                continue;

            strcpy(worlds[worlds_count].filename, worlds_dir);
            strcpy(worlds[worlds_count].filename + worlds_dir_len, dirent->d_name);

            if((name_lenght = world_read_name(worlds[worlds_count].name, worlds[worlds_count].filename)) <= 0)
                continue;

            worlds[worlds_count].name_lenght = name_lenght;

            if(++worlds_count >= MAX_WORLD_COUNT)
                break;
        }
        closedir(dir);
    }

    menu_change_type(MENU_MAIN);
}

MenuCode menu_update()
{
    menu_render();

    nanosleep(&menu_sleep_ts, NULL);

    menu_frame_count++;

    return input_handle();
}

void menu_free()
{

}

void menu_change_type(MenuType type)
{
    menu_type = type;
    menu_frame_count = 0;
    selected_button_index = 0;

    if(type == MENU_MAIN)
        buttons_count = 2;
    else
        buttons_count = worlds_count + 1;
}

MenuCode input_handle()
{
    bool handle_click = false;
    int key = getch();
    switch (key)
    {
    case KEY_UP:
        selected_button_index--;
        break;

    case KEY_DOWN:
        selected_button_index++;
        break;

    case '\n':
        handle_click = true;
        break;
    }

    if(selected_button_index < 0)
        selected_button_index = buttons_count - 1;
    else if(selected_button_index >= buttons_count)
        selected_button_index = 0;

    if(handle_click)
        return button_handle_click();
    
    return MENU_CODE_UPDATE;
}

MenuCode button_handle_click()
{
    if(menu_type == MENU_MAIN)
    {
        switch (selected_button_index)
        {
        case 0:
            menu_change_type(MENU_WORLD_SELECTION);
            break;

        case 1:
            return MENU_CODE_EXIT;
        }
    }
    else
    {
        if(selected_button_index == 0)
        {
            menu_change_type(MENU_MAIN);
            return MENU_CODE_UPDATE;
        }

        set_world_path(worlds[selected_button_index - 1].filename);
        menu_change_type(MENU_MAIN);
        return MENU_CODE_GAME_START;
    }

    return MENU_CODE_UPDATE;
}

void menu_render()
{
    erase();

    //render menu frame
    attron(COLOR_PAIR(PAIR_GENERAL));
    menu_frame_render(0,        0,         COLS, false);
    menu_frame_render(0,        LINES - 1, COLS, false);
    menu_frame_render(0,        0,         LINES, true);
    menu_frame_render(COLS - 1, 0,         LINES, true);

    if(menu_type == MENU_MAIN)
    {
        int origin_y = (LINES - MENU_HEIGHT) / 2;

        attron(COLOR_PAIR(PAIR_PLAYER));
        file_render((COLS - 40) / 2, origin_y, RESOURCE_LOGO);

        const int len = 70;
        menu_pacman_anim_render((COLS - len) / 2, origin_y + 8, len);

        buttons_render((COLS - 8) / 2, origin_y + 16);

        attron(COLOR_PAIR(PAIR_GENERAL));
        mvprintw(LINES - 2, 2, "Created by mon1tor");
    }
    else
    {
        int origin_y = (LINES - worlds_count * 2 - 4) / 2;

        attron(COLOR_PAIR(PAIR_GENERAL));
        mvprintw(origin_y + 2, COLS / 2 - 13 / 2, "SELECT WORLD:");

        buttons_render(COLS / 2, origin_y);
    }

    refresh();
}

void menu_pacman_anim_render(int x, int y, int len)
{
    attron(COLOR_PAIR(PAIR_WALLS));
    menu_frame_render(x, y, len, false);
    menu_frame_render(x, y + 4, len, false);

    int x_middle = x + len / 2;
    attron(COLOR_PAIR(PAIR_GENERAL));
    for(int i = x_middle; i <= x + len; ++i)
    {
        if(i % 5 == 4 - (menu_frame_count / MENU_FRAME_TIME_DIFF) % 5)
            mvprintw(y + 2, i, ".");
    }

    int animation_offset = get_animation_offset((menu_frame_count / MENU_FRAME_TIME_DIFF) / 2 + 1);
    attron(COLOR_PAIR(PAIR_PLAYER));
    file_render(x_middle - 2 + animation_offset, y + 1, RESOURCE_PLAYER_EAST);

    for(int i = 0; i < 2; ++i)
    {
        animation_offset = get_animation_offset((menu_frame_count / MENU_FRAME_TIME_DIFF) / 2 - i);
        attron(COLOR_PAIR(PAIR_HUNTERS + i));
        file_render(x_middle - 2 - 10 * (i + 1) + animation_offset, y + 1, RESOURCE_HUNTER_EAST);
    }
}

void menu_frame_render(int x, int y, int lenght, bool vertical)
{
    if(vertical)
    {
        for(int j = 1; j < lenght; ++j)
            mvprintw(y + j, x, "|");
        
        mvprintw(y, x, "+");
        mvprintw(y + lenght - 1, x, "+");
    }
    else
    {
        for(int i = 0; i < lenght; ++i)
            mvprintw(y, x + i, "-");
        mvprintw(y, x, "+");
        mvprintw(y, x + lenght - 1, "+");
    }
}

void buttons_render(int x, int y)
{
    if(menu_type == MENU_MAIN)
    {
        attron(COLOR_PAIR(PAIR_GENERAL));
        button_render(x, y, 0, "NEW GAME");
        attron(COLOR_PAIR(PAIR_GENERAL));
        button_render(x, y + 2, 1, "EXIT");
    }
    else
    {
        int i;
        for(i = 1; i <= worlds_count; ++i)
        {
            attron(COLOR_PAIR(PAIR_MENU_NAMES));
            button_render(x - worlds[i - 1].name_lenght / 2, y + i * 2 + 2, i, worlds[i - 1].name);
        } 
        
        attron(COLOR_PAIR(PAIR_GENERAL));
        button_render(x - 2, y + i * 2 + 2, 0, "BACK");
    }
}

void button_render(int x, int y, int index, const char *label)
{
    mvprintw(y, x, label);
    if(selected_button_index == index)
    {
        attron(COLOR_PAIR(PAIR_WALLS));
        mvprintw(y, x - 3, ">>");
    }
}

int get_animation_offset(int timer)
{
    //hunters & player animation stages & offset
    
    int offset_stage = timer % 6;

    int offset;
    if(offset_stage <= 1)
        offset = 0;
    else if(offset_stage >= 3 && offset_stage <= 4)
        offset = 2;
    else
        offset = 1;

    return offset;
}