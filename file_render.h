#pragma once

//used for array indexing
typedef enum {
    RESOURCE_LOGO,

    RESOURCE_PLAYER_NORTH,
    RESOURCE_PLAYER_EAST,
    RESOURCE_PLAYER_SOUTH,
    RESOURCE_PLAYER_WEST,
    RESOURCE_PLAYER_DEAD,
    
    RESOURCE_HUNTER_NORTH,
    RESOURCE_HUNTER_EAST,
    RESOURCE_HUNTER_SOUTH,
    RESOURCE_HUNTER_WEST,
    RESOURCE_HUNTER_FRIGHTENED,

    RESOURCE_COUNT
} FileRenderType;

void file_render_init();
void file_render_free();

void file_render(int x, int y, FileRenderType type);