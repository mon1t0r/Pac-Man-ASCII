#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curses.h>
#include <math.h>
#include <limits.h>
#include "game.h"
#include "graphics.h"

#define PLAYER_LIVES_COUNT 3
//max scatter/chase mode switch count before infinite chase
#define HUNTER_MAX_MODE_SWITCH_COUNT 6

World world;
SessionInfo session;

bool force_render_next_frame;
bool exit_next_frame;
unsigned int frame_count;

char *world_path;

void game_restart(bool restore_level, bool reset_score);

void timer_handle_end(FrameTimerType type);
bool timer_should_decrement(FrameTimerType type);

void player_move();
void player_handle_movement();
void player_handle_cell(int x, int y);

void hunter_handle_update(int type);
void hunter_handle_movement(int type);
void hunter_handle_collision(int type);
void hunter_choose_direction(int type, Direction dirs_clear[3], bool can_turn_north);
void hunter_restore_state(int type, const EntityState *state, bool save_prev_pos);
bool hunter_is_active(int type);

void hunters_set_mode(HunterMode mode);

void game_init()
{
    srand(time(NULL));

    game_restart(true, true);
    frame_count = 1;
}

void game_restart(bool restore_level, bool reset_score)
{
    if(reset_score)
    {
        //reset whole session
        memset(&session, 0, sizeof(session));
        session.lives = PLAYER_LIVES_COUNT;
    }
    else
        //do not clear first 3 int variables (score, lives, beepers_collected)
        memset(((int *)&session) + 3, 0, sizeof(session) - sizeof(int) * 3);

    if(restore_level)
    {
        //reload world from file
        world_free(&world);
        world_init(&world, world_path);
        session.beepers_collected = 0;
    }
    
    for(int i = 0; i < HUNTERS_COUNT; ++i)
        hunter_restore_state(i, world.hunters_home_states + i, false);

    timer_reset_all(session.timer_storage);

    //activate scatter mode
    timer_set_active(session.timer_storage, TIMER_HUNTER_MODE_SCATTER);

    timer_set_active(session.timer_storage, TIMER_HUNTERS_FREEZE);
    timer_set_active(session.timer_storage, TIMER_GAME_FREEZE_STARTUP);

    world.player = world.player_home_state;

    frame_count = 0;
}

int game_update()
{
    for(int i = 0; i < TIMER_TYPES_COUNT; ++i)
        //decrement timer only if it should decrement & is active
        if(timer_is_active(session.timer_storage, i) && timer_should_decrement(i))
        {
            session.timer_storage[i]--;
            if(session.timer_storage[i] <= 0)
                timer_handle_end(i);
        }

    frame_count++;

    //handle player input
    player_handle_movement();

    //exit to menu if needed
    if(exit_next_frame)
    {
        exit_next_frame = false;
        return 1;
    }

    //do not update game if game is freezed
    if(is_game_freeze_timer_active(session.timer_storage))
        return 0;
    
    //release hunters one by one
    if(timer_is_active(session.timer_storage, TIMER_HUNTERS_FREEZE))
    {
        //get current hunter index that should be released based on time passed (first one is released immidiately)
        int hunter_index = 1 + (int)roundf((session.timer_storage[TIMER_HUNTERS_FREEZE] / (float)timer_get_full_duration(TIMER_HUNTERS_FREEZE)) * (float)(HUNTERS_COUNT - 1));
        if(!hunter_is_active(hunter_index))
            //restore first (default) hunter state - hunter spawn point
            hunter_restore_state(hunter_index, world.hunters_home_states, true);
    }

    entity_reset_prev_pos(&world.player);
    
    player_move();
    for(int i = 0; i < HUNTERS_COUNT; ++i)
    {
        if(!hunter_is_active(i))
            continue;
        
        //update and move hunter only if should not be freezed this frame
        if(!session.hunters_info[i].freeze_next_frame)
        {
            hunter_handle_update(i);
            hunter_handle_movement(i);
        }
        else
            session.hunters_info[i].freeze_next_frame = false;
        
        //update collision only if should not be collidable this frame
        if(!session.hunters_info[i].incollidable_next_frame)
            hunter_handle_collision(i);
        else
            session.hunters_info[i].incollidable_next_frame = false;
    }

    return 0;
}

void game_free()
{
    world_free(&world);
}

void set_world_path(char *path)
{
    world_path = path;
}

void timer_handle_end(FrameTimerType type)
{
    switch (type)
    {
    case TIMER_GAME_FREEZE_PLAYER_DEATH: //restart game: save score & save world state
        force_render_next_frame = true;
        game_restart(false, false);
        break;

    case TIMER_GAME_FREEZE_VICTORY: //restart game: save score & reset world state
        force_render_next_frame = true;
        game_restart(true, false);
        break;

    case TIMER_GAME_FREEZE_GAME_OVER: //restart game: reset score & reset world state
        force_render_next_frame = true;
        game_restart(true, true);
        break;
    
    case TIMER_HUNTER_MODE_SCATTER: //if mode was scatter, change to chase
        session.mode_change_count++;
        hunters_set_mode(MODE_CHASE);
        break;
    
    case TIMER_HUNTER_MODE_CHASE: //if mode was chase, change to scatter
        session.mode_change_count++;
        hunters_set_mode(MODE_SCATTER);
        break;

    case TIMER_HUNTER_MODE_FRIGHTENED: //if mode was frightened, every hunter become unfrightened
        for(int i = 0; i < HUNTERS_COUNT; ++i)
            session.hunters_info[i].is_frightened = false;
        break;

    default:
        break;
    }

    if(session.mode_change_count >= HUNTER_MAX_MODE_SWITCH_COUNT) //if too much mode changes, enable infinite chase mode
    {
        hunters_set_mode(MODE_CHASE);
        session.timer_storage[TIMER_HUNTER_MODE_CHASE] = -1;
    }
}

bool timer_should_decrement(FrameTimerType type)
{
    if(type == TIMER_HUNTER_MODE_SCATTER || type == TIMER_HUNTER_MODE_CHASE) //if is scatter/chase mode timer and if frightened timer is active, do not decrement
        return !timer_is_active(session.timer_storage, TIMER_HUNTER_MODE_FRIGHTENED);
    return true;
}

void player_move()
{
    if(entity_front_is_clear(&world.player, &world))
        entity_step(&world.player, &world);
    
    player_handle_cell(world.player.prev_pos_x, world.player.prev_pos_y);
}

void player_handle_movement()
{
    int dir = -1;
    int key = -1;
    int key_queue = -1;
    while(dir == -1) //handle all pressed keys one by one
    {
        key = getch();
        switch (key)
        {
        case -1: //if nothing pressed
            dir = -2;
            break;
        case KEY_DOWN:
            dir = DIR_SOUTH;
            break;
        case KEY_UP:
            dir = DIR_NORTH;
            break;
        case KEY_LEFT:
            dir = DIR_WEST;
            break;
        case KEY_RIGHT:
            dir = DIR_EAST;
            break;
        case 27: //if ESC
            exit_next_frame = true;
            break;
        }

        if(dir >= 0 && !entity_dir_is_clear(&world.player, &world, (Direction) dir)) //if can not turn into direction, put key into queue
        {
            key_queue = key;
            dir = -1;
        }
    }

    if(dir < 0)
    {
        if(key_queue != -1) //if has key in queue, ungetch
            ungetch(key_queue);
        return;
    }

    world.player.dir = (Direction) dir;
    flushinp();
}

void player_handle_cell(int x, int y)
{
    CellType *cell = world_get_cell(&world, x, y);
    if(cell && *cell != CELL_EMPTY)
    {
        if(*cell == CELL_ENERGIZER)
        {
            session.score += 50;
            hunters_set_mode(MODE_FRIGHTENED);
        }
        else
            session.score += 10;
        *cell = CELL_EMPTY;
        session.beepers_collected++;
        if(session.beepers_collected >= world.beepers_count) //if collected all beepers
        {
            session.beepers_collected = 0;
            world.player.pos_x = world.player.prev_pos_x;
            world.player.pos_y = world.player.prev_pos_y;
            schedule_render(SCHEDULED_RENDER_VICTORY, 0);
            timer_set_active(session.timer_storage, TIMER_GAME_FREEZE_VICTORY);
        }
    }
}

void hunter_handle_update(int type)
{
    if(hunter_get_mode(type) == MODE_FRIGHTENED)
    {
        if(!hunters_get_slowdown_stage(frame_count))
            entity_reset_prev_pos(world.hunters + type); //do not reset prev pos in certain frames in frightened mode
        return;
    }

    entity_reset_prev_pos(world.hunters + type);
}

void hunter_handle_movement(int type)
{
    EntityState *hunter = world.hunters + type;
    bool front_is_clear = entity_front_is_clear(hunter, &world);

    Direction dirs_clear[4];
    int clear_count = 0;
    for(int i = 0; i < 4; ++i)
        if(entity_dir_is_clear(hunter, &world, i))
            dirs_clear[clear_count++] = i; //get all avaliable directions

    switch (hunter_get_mode(type))
    {
    case MODE_SCATTER: //move 
        {
            if(front_is_clear && rand() % 2) //step forward with 50% probability, if avaliable
            {
                entity_step(hunter, &world);
                return;
            }

            if(hunter->dir % 2) //turn north/south if was moving west/east
            {
                bool north_clear = entity_dir_is_clear(hunter, &world, DIR_NORTH);
                bool south_clear = entity_dir_is_clear(hunter, &world, DIR_SOUTH);

                if(north_clear && south_clear)
                    hunter->dir = rand() % 2 ? DIR_NORTH : DIR_SOUTH;
                else if(north_clear)
                    hunter->dir = DIR_NORTH;
                else if(south_clear)
                    hunter->dir = DIR_SOUTH;
                else if(!front_is_clear)
                    hunter->dir = direction_opposite(hunter->dir);
            }
            else //turn west/east if was moving north/south
            {
                bool east_clear = entity_dir_is_clear(hunter, &world, DIR_EAST);
                bool west_clear = entity_dir_is_clear(hunter, &world, DIR_WEST);

                if(east_clear && west_clear)
                    hunter->dir = rand() % 2 ? DIR_EAST : DIR_WEST;
                else if(east_clear)
                    hunter->dir = DIR_EAST;
                else if(west_clear)
                    hunter->dir = DIR_WEST;
                else if(!front_is_clear)
                    hunter->dir = direction_opposite(hunter->dir);
            }

            entity_step(hunter, &world);
        }
        break;
    
    case MODE_CHASE:
        {
            switch (clear_count)
            {
            case 1:
                hunter->dir = dirs_clear[0]; //if only 1 avaliable direction, turn into it
                break;
            case 2: //if 2 avaliable directions, do not change direstion; if front is blocked, turn into not the opposite one
                if(!front_is_clear)
                    hunter->dir = dirs_clear[0] != direction_opposite(hunter->dir) ? dirs_clear[0] : dirs_clear[1];
                break;
            case 3: //if 3 avaliable directions, run direction choose algorithm, with can turn north option
                hunter_choose_direction(type, dirs_clear, true);
                break;
            case 4: //if 4 avaliable directions, keep moving in old direction; if old direction is north, run direction choose algorithm, without can turn north option
                if(hunter->dir == DIR_NORTH)
                    hunter_choose_direction(type, dirs_clear + 1, false);
                break;
            
            default:
                break;
            }

            entity_step(hunter, &world);
        }
        break;

    case MODE_FRIGHTENED:
        {
            if(hunters_get_slowdown_stage(frame_count)) //do not move in certain frames in frightened mode
                break;
            
            //choose random direction from avaliale (but not opposite)
            if(clear_count > 2 || (clear_count == 2 && dirs_clear[0] != direction_opposite(dirs_clear[1])))
                hunter->dir = dirs_clear[rand() % clear_count];
            else if(clear_count == 1)
                hunter->dir = dirs_clear[0];

            entity_step(hunter, &world);
        }
        break;
    }
}

void hunter_handle_collision(int type)
{
    EntityState *hunter = world.hunters + type;
    HunterBehaviourInfo *hunter_info = session.hunters_info + type;

    //chech for collision with player
    if((hunter->prev_pos_x != world.player.prev_pos_x || hunter->prev_pos_y != world.player.prev_pos_y) &&
        (hunter->pos_x != world.player.prev_pos_x || hunter->pos_y != world.player.prev_pos_y))
        return;

    if(hunter_get_mode(type) == MODE_FRIGHTENED)
    {
        session.frightened_kill_count++;
        session.score += 100 * (int)pow(2, session.frightened_kill_count);

        hunter_restore_state(type, world.hunters_home_states, true);
        hunter_info->is_frightened = false;
        hunter_info->incollidable_next_frame = true;
        
        schedule_render(SCHEDULED_RENDER_HUNTER_DEAD, type);
        timer_set_active(session.timer_storage, TIMER_GAME_FREEZE_HUNTER_DEATH);
        return;
    }
    
    session.lives--;
    if(session.lives <= 0)
    {
        schedule_render(SCHEDULED_RENDER_GAME_OVER, 0);
        timer_set_active(session.timer_storage, TIMER_GAME_FREEZE_GAME_OVER);
    }
    else
    {
        schedule_render(SCHEDULED_RENDER_PLAYER_DEAD, 0);
        timer_set_active(session.timer_storage, TIMER_GAME_FREEZE_PLAYER_DEATH);
    }
}

void hunter_choose_direction(int type, Direction dirs_clear[3], bool can_turn_north)
{
    const Direction turn_order[] = { DIR_NORTH, DIR_WEST, DIR_SOUTH };

    EntityState *hunter = world.hunters + type;

    int compare_count = 0;
    float distances[2];
    Direction directions[2];
    int neighbour_x, neighbour_y;

    for(int i = 0; i < 3; ++i)
    {
        //exclude north & opposite directions
        if((!can_turn_north && dirs_clear[i] == DIR_NORTH) || direction_opposite(hunter->dir) == dirs_clear[i])
            continue;
        neighbour_x = hunter->pos_x;
        neighbour_y = hunter->pos_y;
        //get neighbour cell to current pos
        world_get_cell_neighbour(&world, &neighbour_x, &neighbour_y, dirs_clear[i]);
        //get distance from neighbour cell to player position
        distances[compare_count] = world_cell_distance(neighbour_x, neighbour_y, world.player.pos_x, world.player.pos_y);
        directions[compare_count++] = dirs_clear[i];
    }

    //choose cell, which has the least distance to player; if equal, use turn_order
    if(distances[0] < distances[1])
        hunter->dir = directions[0];
    else if(distances[0] > distances[1])
        hunter->dir = directions[1];
    else
    {
        for(int i = 0; i < 3; ++i)
            for(int j = 0; j < 2; ++j)
                if(turn_order[i] == directions[j])
                {
                    hunter->dir = directions[j];
                    break;
                }
    }
}

void hunter_restore_state(int type, const EntityState *state, bool save_prev_pos)
{
    EntityState *hunter = world.hunters + type;

    int prev_x = hunter->pos_x;
    int prev_y = hunter->pos_y;
    *hunter = *state;
    if(save_prev_pos) //save prev pos to play move anim
    {
        hunter->prev_pos_x = prev_x;
        hunter->prev_pos_y = prev_y;
        session.hunters_info[type].freeze_next_frame = true;
    }
}

int hunters_get_slowdown_stage(int time)
{
    return time % HUNTER_FRIGHTENED_SLOWDOWN_MODIFIER;
}

bool hunter_is_active(int type)
{
    //hunter is active if has zero index (first hunter, always active) or is not at home position
    return type == 0 ||
        world.hunters[type].pos_x != world.hunters_home_states[type].pos_x ||
        world.hunters[type].pos_y != world.hunters_home_states[type].pos_y;
}

HunterMode hunter_get_mode(int type)
{
    //get mode for an individual hunter

    if(timer_is_active(session.timer_storage, TIMER_HUNTER_MODE_FRIGHTENED) && session.hunters_info[type].is_frightened)
        return MODE_FRIGHTENED;

    if(timer_is_active(session.timer_storage, TIMER_HUNTER_MODE_SCATTER))
        return MODE_SCATTER;
    
    if(timer_is_active(session.timer_storage, TIMER_HUNTER_MODE_CHASE))
        return MODE_CHASE;

    return -1;
}

void hunters_set_mode(HunterMode mode)
{
    //set mode for all hunters

    timer_set_active(session.timer_storage, TIMER_HUNTER_MODE_SCATTER + mode);
    if(mode == MODE_FRIGHTENED)
    {
        session.frightened_kill_count = 0;
        for(int i = 0; i < HUNTERS_COUNT; ++i)
            session.hunters_info[i].is_frightened = true;
    }
}