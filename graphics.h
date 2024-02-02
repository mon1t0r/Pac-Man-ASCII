#pragma once

typedef enum {
    SCHEDULED_RENDER_PLAYER_DEAD,
    SCHEDULED_RENDER_HUNTER_DEAD,
    SCHEDULED_RENDER_GAME_OVER,
    SCHEDULED_RENDER_VICTORY
} ScheduledRenderType;

void graphics_init();
void graphics_update();
void graphics_free();

void schedule_render(ScheduledRenderType type, int data);