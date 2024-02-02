#define _POSIX_C_SOURCE 200201L
#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <time.h>
#include <math.h>
#include "graphics.h"
#include "game.h"
#include "color_pair.h"
#include "file_render.h"

#define WORLD_SCALE_X 4
#define WORLD_SCALE_Y 2
//offsets to render at the middle of the screen
#define WORLD_OFFSET_X ((COLS - world.width * 2 * WORLD_SCALE_X) / 2)
#define WORLD_OFFSET_Y ((LINES - world.height * 2 * WORLD_SCALE_Y) / 2)

//render passes count
#define ENTITY_RENDER_PASSES WORLD_SCALE_X

//if frightened mode left time < this, blink hunter
#define HUNTER_FRIGHTENED_BLINK_TIME 8

//scheduled render system info
bool has_scheduled_render;
ScheduledRenderType scheduled_render;
int scheduled_render_data;

//player & hunters state when last rendered on screen
EntityState last_frame_player_state;
EntityState last_frame_hunters_state[HUNTERS_COUNT];

struct timespec sleep_ts = {
    .tv_sec = 0,
    .tv_nsec = FRAME_TIME / (double) ENTITY_RENDER_PASSES * 1000000000L
};

void scene_render(int pass);
void world_render();
void player_render(int pass);
void hunter_render(int type, int pass);
void wall_render(int x, int y, Direction dir);
void cell_render(int x, int y);
void ui_render();
bool render_scheduled();

int get_frightened_hunter_render_pass(int pass, int timer);

int cell_pos_x(int x);
int cell_pos_y(int y);

int entity_render_pos_x(EntityState *entity, int pass);
int entity_render_pos_y(EntityState *entity, int pass);

void graphics_init()
{
    //ncurses init
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(FALSE);
    nodelay(stdscr, TRUE);

    start_color();
    init_pair(PAIR_GENERAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_WALLS, COLOR_BLUE, COLOR_BLACK);
    init_pair(PAIR_MENU_NAMES, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_PLAYER, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_GAME_OVER, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_HUNTERS, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_HUNTERS + 1, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_HUNTERS + 2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_HUNTERS + 3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_HUNTERS + 4, COLOR_BLUE, COLOR_BLACK);
}

void graphics_update()
{
    for(int pass = 0; pass < ENTITY_RENDER_PASSES; ++pass)
    {
        if(!is_game_freeze_timer_active(session.timer_storage) || force_render_next_frame)
            scene_render(pass);
        
        nanosleep(&sleep_ts, NULL);
    }
    force_render_next_frame = false;

    //scheduled render system
    if(render_scheduled())
        refresh();
}

void graphics_free()
{
    endwin();
}

void scene_render(int pass)
{
    erase();

    world_render();
    
    for(int i = 0; i < HUNTERS_COUNT; ++i)
    {
        int hunters_pass = pass;

        if(hunter_get_mode(i) == MODE_FRIGHTENED)
            //slow down hunter movement when rendered between cells (render passes)
            hunters_pass = get_frightened_hunter_render_pass(pass, frame_count);
        
        hunter_render(i, hunters_pass);
    }

    player_render(pass);

    ui_render();

    refresh();

    last_frame_player_state = world.player;
    for(int i = 0; i < HUNTERS_COUNT; ++i)
        last_frame_hunters_state[i] = world.hunters[i];
}

void world_render()
{
    int width = world.width;
    int height = world.height;
    for(int i = 0; i < width; ++i)
        for(int j = 0; j < height; ++j)
            cell_render(i, j);
}

void player_render(int pass)
{
    //get screen pos
    int pos_x = entity_render_pos_x(&world.player, pass);
    int pos_y = entity_render_pos_y(&world.player, pass);

    attron(COLOR_PAIR(PAIR_PLAYER));
    file_render(pos_x - 2, pos_y - 1, RESOURCE_PLAYER_NORTH + world.player.dir);
}

void hunter_render(int type, int pass)
{
    EntityState *hunter = world.hunters + type;

    int pos_x = entity_render_pos_x(hunter, pass);
    int pos_y = entity_render_pos_y(hunter, pass);
    bool frightened = hunter_get_mode(type) == MODE_FRIGHTENED;

    if(frightened)
    {
        if(session.timer_storage[TIMER_HUNTER_MODE_FRIGHTENED] <= HUNTER_FRIGHTENED_BLINK_TIME && session.timer_storage[TIMER_HUNTER_MODE_FRIGHTENED] % 2)
            attron(COLOR_PAIR(PAIR_GENERAL));
        else
            attron(COLOR_PAIR(PAIR_HUNTERS + HUNTERS_COUNT));
    }
    else
        attron(COLOR_PAIR(PAIR_HUNTERS + type));

    if(frightened)
        file_render(pos_x - 2, pos_y - 1, RESOURCE_HUNTER_FRIGHTENED);
    else
        file_render(pos_x - 2, pos_y - 1, RESOURCE_HUNTER_NORTH + hunter->dir);
}

void wall_render(int x, int y, Direction dir)
{
    if(!world_get_wall_coords(&world, &x, &y, dir))
        return;

    WallState* wall = world_get_wall(&world, x, y);
    if(!wall || !wall->exists)
        return;

    //render walls white on victory scheduled render
    if(has_scheduled_render && scheduled_render == SCHEDULED_RENDER_VICTORY)
        attron(COLOR_PAIR(PAIR_GENERAL));
    else
        attron(COLOR_PAIR(PAIR_WALLS));
        
    if(dir % 2)
    {
        for(int j = -1; j <= 1; ++j)
            mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y + j, WORLD_OFFSET_X + x * WORLD_SCALE_X, "|");

        if(((wall = world_get_wall(&world, x + 1, y - 1)) && wall->exists) ||
                ((wall = world_get_wall(&world, x - 1, y - 1)) && wall->exists))
            mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y - 2, WORLD_OFFSET_X + x * WORLD_SCALE_X, "+");
        else if((wall = world_get_wall(&world, x, y - 2)) && wall->exists)
            mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y - 2, WORLD_OFFSET_X + x * WORLD_SCALE_X, "|");

        if(((wall = world_get_wall(&world, x + 1, y + 1)) && wall->exists) ||
                ((wall = world_get_wall(&world, x - 1, y + 1)) && wall->exists))
            mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y + 2, WORLD_OFFSET_X + x * WORLD_SCALE_X, "+");
    }
    else
    {
        mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y, WORLD_OFFSET_X + x * WORLD_SCALE_X - 3, "-------");

        if((wall = world_get_wall(&world, x - 2, y)) && wall->exists)
        {
            mvprintw(WORLD_OFFSET_Y + y * WORLD_SCALE_Y, WORLD_OFFSET_X + x * WORLD_SCALE_X - 4, "-");
            return;
        }
    }
}

void cell_render(int x, int y)
{
    attron(COLOR_PAIR(PAIR_GENERAL));
    CellType *cell = world_get_cell(&world, x, y);
    
    switch (*cell)
    {
    case CELL_BEEPER:
        mvprintw(cell_pos_y(y), cell_pos_x(x), ".");
        break;

    case CELL_ENERGIZER:
        if(frame_count % 2) //blink render
            mvprintw(cell_pos_y(y), cell_pos_x(x), "O");
        break;

    default:
        break;
    }

    wall_render(x, y, DIR_NORTH);
    wall_render(x, y, DIR_WEST);

    //render walls at east & south world borders
    if(x == world.width - 1)
        wall_render(x, y, DIR_EAST);
    if(y == world.height - 1)
        wall_render(x, y, DIR_SOUTH);

    if(world.label_pos_x == x && world.label_pos_y == y)
    {
        if(timer_is_active(session.timer_storage, TIMER_GAME_FREEZE_STARTUP))
        {
            attron(COLOR_PAIR(PAIR_PLAYER));
            mvprintw(cell_pos_y(y), cell_pos_x(x) - 2, "READY!");
        }
    }
}

void ui_render()
{
    attron(COLOR_PAIR(PAIR_GENERAL));
    mvprintw(0, 0, " %d | ", session.score);

    attron(COLOR_PAIR(PAIR_PLAYER));
    for(int i = 1; i < session.lives; ++i)
        printw("> ");
}

bool render_scheduled()
{
    if(!has_scheduled_render)
        return false;
    
    switch (scheduled_render)
    {
    case SCHEDULED_RENDER_HUNTER_DEAD:
        {
            //render score got from this hunter death

            int pass = get_frightened_hunter_render_pass(ENTITY_RENDER_PASSES - 1, frame_count - 1);
            int x = entity_render_pos_x(last_frame_hunters_state + scheduled_render_data, pass);
            int y = entity_render_pos_y(last_frame_hunters_state + scheduled_render_data, pass);

            attron(COLOR_PAIR(PAIR_GENERAL));
            mvprintw(y, x - 1, "%d", 100 * (int)pow(2, session.frightened_kill_count));
        }
        break;

    case SCHEDULED_RENDER_GAME_OVER:
        {
            attron(COLOR_PAIR(PAIR_GAME_OVER));
            mvprintw(cell_pos_y(world.label_pos_y), cell_pos_x(world.label_pos_x) - 4, "GAME OVER");

            //no break: render game over label & dead player
        }

    case SCHEDULED_RENDER_PLAYER_DEAD:
        {
            int x = entity_render_pos_x(&last_frame_player_state, ENTITY_RENDER_PASSES - 1);
            int y = entity_render_pos_y(&last_frame_player_state, ENTITY_RENDER_PASSES - 1);

            attron(COLOR_PAIR(PAIR_PLAYER));
            file_render(x - 2, y - 1, RESOURCE_PLAYER_DEAD);
        }
        break;
    
    case SCHEDULED_RENDER_VICTORY:
        {
            //re-render all walls & ui with white color

            int width = world.width;
            int height = world.height;
            for(int i = 0; i < width; ++i)
                for(int j = 0; j < height; ++j)
                {
                    wall_render(i, j, DIR_NORTH);
                    wall_render(i, j, DIR_WEST);

                    if(i == world.width - 1)
                        wall_render(i, j, DIR_EAST);
                    if(j == world.height - 1)
                        wall_render(i, j, DIR_SOUTH);
                }

            ui_render();
        }
        break;
    }

    has_scheduled_render = false;

    return true;
}

void schedule_render(ScheduledRenderType type, int data)
{
    has_scheduled_render = true;
    scheduled_render = type;
    scheduled_render_data = data;
}

int get_frightened_hunter_render_pass(int pass, int timer)
{
    //get render pass (pos between cells) for frightened (slowed down) hunter
    int slowdown_stage = hunters_get_slowdown_stage(timer);
    return (int)roundf((pass + slowdown_stage * ENTITY_RENDER_PASSES) / (float)HUNTER_FRIGHTENED_SLOWDOWN_MODIFIER);
}

int cell_pos_x(int x)
{
    return WORLD_OFFSET_X + (x * 2 + 1) * WORLD_SCALE_X;
}

int cell_pos_y(int y)
{
    return WORLD_OFFSET_Y + (y * 2 + 1) * WORLD_SCALE_Y;
}

int entity_render_pos_x(EntityState *entity, int pass)
{
    return cell_pos_x(entity->prev_pos_x) + (entity->pos_x - entity->prev_pos_x) * pass * 2;
}

int entity_render_pos_y(EntityState *entity, int pass)
{
    return cell_pos_y(entity->prev_pos_y) + (entity->pos_y - entity->prev_pos_y) * (int)roundf(pass * 2 * WORLD_SCALE_Y / (float) WORLD_SCALE_X);
}