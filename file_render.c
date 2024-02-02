#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>
#include "file_render.h"
#include "exit_cause.h"

//max line count for resource
#define MAX_LINE_COUNT 30

typedef char * FileRenderLine;

FileRenderLine *file_render_storage[RESOURCE_COUNT];

void file_init(FileRenderType type, const char *resource_path);
void file_free(FileRenderLine *file_render);

void file_render_init()
{
    //resources init
    file_init(RESOURCE_LOGO, "res/logo.res");

    file_init(RESOURCE_PLAYER_NORTH, "res/player/player_north.res");
    file_init(RESOURCE_PLAYER_EAST, "res/player/player_east.res");
    file_init(RESOURCE_PLAYER_SOUTH, "res/player/player_south.res");
    file_init(RESOURCE_PLAYER_WEST, "res/player/player_west.res");
    file_init(RESOURCE_PLAYER_DEAD, "res/player/player_dead.res");

    file_init(RESOURCE_HUNTER_NORTH, "res/hunter/hunter_north.res");
    file_init(RESOURCE_HUNTER_EAST, "res/hunter/hunter_east.res");
    file_init(RESOURCE_HUNTER_SOUTH, "res/hunter/hunter_south.res");
    file_init(RESOURCE_HUNTER_WEST, "res/hunter/hunter_west.res");
    file_init(RESOURCE_HUNTER_FRIGHTENED, "res/hunter/hunter_frightened.res");
}

void file_render_free()
{
    for(int i = 0; i < RESOURCE_COUNT; ++i)
        file_free(file_render_storage[i]);
}

void file_render(int x, int y, FileRenderType type)
{
    for(int i = 0; i < MAX_LINE_COUNT; ++i)
    {
        //if line is NULL, break
        if(!file_render_storage[type][i])
            break;
        mvprintw(y + i, x, file_render_storage[type][i]);
    }
}

void file_init(FileRenderType type, const char *resource_path)
{
    //line array (single file) memory allocation
    file_render_storage[type] = (FileRenderLine *) calloc(MAX_LINE_COUNT, sizeof(FileRenderLine));

    FILE *file = fopen(resource_path, "r");
    if(file == NULL)
        exit(CAUSE_RESOURCE_DOES_NOT_EXIST);
    
    size_t lenght = 0;
    size_t str_lenght = 0;
    int lines_count = 0;

    while (getline(&file_render_storage[type][lines_count], &lenght, file) != -1)
    {
        str_lenght = strlen(file_render_storage[type][lines_count]);

        //line separation & terminators
        if(file_render_storage[type][lines_count][str_lenght - 1] == '\n')
            file_render_storage[type][lines_count][str_lenght - 1] = 0;
        
        if(++lines_count >= MAX_LINE_COUNT)
            break;
    }

    //set last + 1 line to NULL
    if(lines_count < MAX_LINE_COUNT)
        file_render_storage[type][lines_count] = NULL;

    fclose(file);
}

void file_free(FileRenderLine *file_render)
{
    for(int i = 0; i < MAX_LINE_COUNT; ++i)
        free(file_render[i]);
    free(file_render);
}