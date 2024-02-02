#include "game.h"
#include "graphics.h"
#include "menu.h"
#include "file_render.h"
#include "exit_cause.h"

int main()
{
    //init all modules
    graphics_init();
    file_render_init();
    menu_init();

    MenuCode menu_code;
    do
    {
        menu_code = menu_update();
        //update menu until game start code is returned
        if(menu_code != MENU_CODE_GAME_START) 
            continue;

        game_init();

        //force render first game frame
        force_render_next_frame = true;

        while(!game_update())
            graphics_update();

        game_free();
    }
    //repeat until exit code is returned
    while(menu_code != MENU_CODE_EXIT);
    
    //free all modules
    menu_free();
    file_render_free();
    graphics_free();
    
    return CAUSE_OK;
}