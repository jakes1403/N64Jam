#include <libdragon.h>
#include "../../core.h"
#include "../../minigame.h"
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>

#include <t3d/t3dmodel.h>

#define FONT_TEXT           1

#define COUNTDOWN_DELAY     3.0f
#define GO_DELAY            1.0f
#define WIN_DELAY           5.0f
#define WIN_SHOW_DELAY      2.0f

// TODO: Flip false when releasing
const bool DEBUG_CONTROLLER_PORT_MODE = true;

const MinigameDef minigame_def = {
    .gamename = "Jakes Game",
    .developername = "jakes1403",
    .description = "Test Game",
    .instructions = "Press A to win."
};

T3DMat4 modelMat;

T3DMat4FP* modelMatFP = NULL;

struct Box {
    T3DVec3 position;

    T3DMat4 matrix;
    T3DMat4FP* matrixFP;

    T3DVec3 velocity;
};

void InitBox(struct Box* box)
{
    box->position.v[0] = 0;
    box->position.v[1] = 0;
    box->position.v[2] = 0;

    box->velocity.v[0] = 0;
    box->velocity.v[1] = 0;
    box->velocity.v[2] = 0;

    box->matrixFP = malloc_uncached(sizeof(T3DMat4FP));
}

void mutateBoxPostion(struct Box* box, float scale)
{
    t3d_mat4_identity(&box->matrix);
    t3d_mat4_scale(&box->matrix, scale, scale, scale);
    t3d_mat4_translate(&box->matrix, box->position.v[0], box->position.v[1], box->position.v[2]);
    t3d_mat4_to_fixed(box->matrixFP, &box->matrix);
}

const float move_speed = 30.0f;

T3DVec3 camPos = {{0,44,-7}};
const T3DVec3 camTarget = {{0,0,0}};

uint8_t colorAmbient[4] = {80, 80, 100, 0xFF};
uint8_t colorDir[4]     = {0xEE, 0xAA, 0xAA, 0xFF};

T3DVec3 lightDirVec = {{-1.0f, 1.0f, 1.0f}};

float rotAngle = 0.0f;

T3DVec3 rotAxis = {{0, 0, 0}};

T3DVec3 boxPosition = {{0, 0, 0}};

T3DViewport viewport;

rspq_block_t *dplDraw = NULL;

rdpq_font_t *font;

float countdown_timer;
bool is_ending;
float end_timer;

wav64_t sfx_start;
wav64_t sfx_countdown;
wav64_t sfx_stop;
wav64_t sfx_winner;

bool wasCountdown = true;

bool is_countdown()
{
    return countdown_timer > 0.0f;
}

int winningPlayer = 0;

T3DModel *mapModel = NULL;
T3DModel *boxModel = NULL;

struct Box box1;

struct Box box2;

struct Box box3;

struct Box box4;

struct Box box5;

struct Box box6;

struct Box box7;

struct Box box8;

struct Box box9;

struct Box box10;

struct Box box11;

struct Box box12;

struct Box players[4];

bool p1_dead = false;
bool p2_dead = false;
bool p3_dead = false;
bool p4_dead = false;

bool in_bounds = false;

bool test_in_bounds(float x, float y, float x_left, float x_right, float y_up, float y_down)
{
    return x_left < x && x < x_right &&
        y_down < y && y < y_up;
}

bool in_square_bounds(float x, float y, float square_x, float square_y, float square_width)
{
    float dia = square_width / 2;
    return test_in_bounds(x, y, square_x - dia, square_x + dia, square_y + dia, square_y - dia);
}

const int box_size = 10;
const int box_max_push_distance = 3;

void processBox(float player_x, float player_y, float player2_x, float player2_y, float deltatime, struct Box* box, bool *p1_pushback, bool *p2_pushback)
{
    bool p1_in_bounds = in_square_bounds(player_x, player_y,box->position.v[0], box->position.v[2], box_size );

    if (p1_in_bounds)
    {
        box->position.v[2] += deltatime * move_speed;
    }

    bool p2_in_bounds = in_square_bounds(player2_x, player2_y,box->position.v[0], box->position.v[2], box_size );

    if (p2_in_bounds)
    {
        box->position.v[2] -= deltatime * move_speed;
    }

    box->position.v[2] = fmin(box->position.v[2], box_max_push_distance);
    box->position.v[2] = fmax(box->position.v[2], -box_max_push_distance);

    *p1_pushback = (box->position.v[2] == box_max_push_distance && p1_in_bounds) || (p2_in_bounds && p1_in_bounds);
    *p2_pushback = (box->position.v[2] == -box_max_push_distance && p2_in_bounds) || (p1_in_bounds && p2_in_bounds);
}

void processBox_h(float player_x, float player_y, float player2_x, float player2_y, float deltatime, struct Box* box, bool *p1_pushback, bool *p2_pushback)
{
    bool p1_in_bounds = in_square_bounds(player_x, player_y,box->position.v[0], box->position.v[2], box_size );

    if (p1_in_bounds)
    {
        box->position.v[0] -= deltatime * move_speed;
    }

    bool p2_in_bounds = in_square_bounds(player2_x, player2_y,box->position.v[0], box->position.v[2], box_size );

    if (p2_in_bounds)
    {
        box->position.v[0] += deltatime * move_speed;
    }

    box->position.v[0] = fmax(box->position.v[0], -box_max_push_distance);
    box->position.v[0] = fmin(box->position.v[0], box_max_push_distance);

    *p1_pushback = (box->position.v[0] == -box_max_push_distance && p1_in_bounds) || (p2_in_bounds && p1_in_bounds);
    *p2_pushback = (box->position.v[0] == box_max_push_distance && p2_in_bounds) || (p1_in_bounds && p2_in_bounds);
}

/*==============================
    minigame_init
    The minigame initialization function
==============================*/
void minigame_init()
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    rdpq_init();

    debug_init_isviewer();
    debug_init_usblog();

    rdpq_debug_start();

    font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    rdpq_text_register_font(FONT_TEXT, font);

    t3d_init((T3DInitParams){}); // Init library itself, use empty params for default settings

    t3d_mat4_identity(&modelMat);

    t3d_vec3_norm(&lightDirVec);

    t3d_vec3_norm(&rotAxis);

    // create a viewport, this defines the section to draw to (by default the whole screen)
    // and contains the projection & view (camera) matrices
    viewport = t3d_viewport_create();

    countdown_timer = COUNTDOWN_DELAY;
    wav64_open(&sfx_start, "rom:/core/Start.wav64");
    wav64_open(&sfx_countdown, "rom:/core/Countdown.wav64");
    wav64_open(&sfx_stop, "rom:/core/Stop.wav64");
    wav64_open(&sfx_winner, "rom:/core/Winner.wav64");

    modelMatFP = malloc_uncached(sizeof(T3DMat4FP));

    mapModel = t3d_model_load("rom:/jake_game/map.t3dm");

    boxModel = t3d_model_load("rom:/jake_game/box.t3dm");

    InitBox(&box1);

    InitBox(&box2);

    InitBox(&box3);

    InitBox(&box4);

    InitBox(&box5);

    InitBox(&box6);

    InitBox(&box7);

    InitBox(&box8);

    InitBox(&box9);

    InitBox(&box10);

    InitBox(&box11);

    InitBox(&box12);

    box1.position.v[0] = 10;

    box2.position.v[0] = 20;

    box3.position.v[0] = 30;

    box4.position.v[2] = -10;

    box5.position.v[2] = -20;

    box6.position.v[2] = -30;

    box7.position.v[2] = 10;

    box8.position.v[2] = 20;

    box9.position.v[2] = 30;

    box10.position.v[0] = -10;

    box11.position.v[0] = -20;

    box12.position.v[0] = -30;

    boxPosition.v[0] = 20;

    boxPosition.v[2] = -20;

    for (int i = 0; i < 4; i++)
    {
        InitBox(&players[i]);
    }

    players[0].position.v[0] = 20;
    players[0].position.v[2] = -20;

    players[1].position.v[0] = 20;
    players[1].position.v[2] = 20;

    players[2].position.v[0] = -20;
    players[2].position.v[2] = 20;

    players[3].position.v[0] = -20;
    players[3].position.v[2] = -20;
    
}

/*==============================
    minigame_fixedloop
    Code that is called every loop, at a fixed delta time.
    Use this function for stuff where a fixed delta time is 
    important, like physics.
    @param  The fixed delta time for this tick
==============================*/
void minigame_fixedloop(float deltatime)
{
    if (countdown_timer > -GO_DELAY)
    {
        float prevtime = countdown_timer;
        countdown_timer -= deltatime;
        if ((int)prevtime != (int)countdown_timer && countdown_timer >= 0)
            wav64_play(&sfx_countdown, 31);
    }

    if (is_ending) {
        float prevendtime = end_timer;
        end_timer += deltatime;
        if ((int)prevendtime != (int)end_timer && (int)end_timer == WIN_SHOW_DELAY)
            wav64_play(&sfx_winner, 31);
        if (end_timer > WIN_DELAY) minigame_end();
    }

    if (!is_countdown() && wasCountdown)
    {
        wasCountdown = false;
        wav64_play(&sfx_start, 31);
    }
}

/*==============================
    minigame_loop
    Code that is called every loop.
    @param  The delta time for this tick
==============================*/

void minigame_loop(float deltatime)
{
    size_t playerCount = core_get_playercount();
    if (DEBUG_CONTROLLER_PORT_MODE)
    {
        playerCount = 4;
    }
    for (size_t i = 0; i < playerCount; i++)
    {
        // For human players, check if the physical A button on the controller was pressed
        joypad_port_t controller_port = core_get_playercontroller(i);

        if (DEBUG_CONTROLLER_PORT_MODE)
        {
            controller_port = i;
        }

        joypad_inputs_t inputs = joypad_get_inputs(controller_port);

        joypad_buttons_t btn = inputs.btn;

        float x = (inputs.stick_x * -1) / 255.0f;
        float y = inputs.stick_y  / 255.0f;
        float mag = sqrt(x*x + y*y);

        float x_norm = 0.0f;
        float y_norm = 0.0f;
        if (mag > 0.01f)
        {
            x_norm = x/mag;
            y_norm = y/mag;
        }


        bool can_move = true;

        if (p1_dead && i == 0)
        {
            can_move = false;
        }
        if (p2_dead && i == 1)
        {
            can_move = false;
        }
        if (p3_dead && i == 2)
        {
            can_move = false;
        }
        if (p4_dead && i == 3)
        {
            can_move = false;
        }

        if (can_move)
        {
            players[i].velocity.v[0] = x_norm * deltatime * move_speed;
            players[i].velocity.v[2] = y_norm * deltatime * move_speed;
        }
        

        // if (btn.c_left)
        // {
        //     camPos.y -= 1;
        // }
        // if (btn.c_right)
        // {
        //     camPos.y += 1;
        // }

        // if (btn.c_down)
        // {
        //     camPos.z -= 1;
        // }
        // if (btn.c_up)
        // {
        //     camPos.z += 1;
        // }


        if (btn.a && !is_countdown() && !is_ending)
        {
            core_set_winner(i);
            is_ending = true;
            wav64_play(&sfx_stop, 31);
            winningPlayer = i;
        }
    }


    // we can set up our viewport settings beforehand here
    t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(85.0f), 10.0f, 100.0f);
    t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(T3DVec3){{0,1,0}});

    // Model-Matrix, t3d offers some basic matrix functions
    t3d_mat4_identity(&modelMat);
    t3d_mat4_scale(&modelMat, 0.02f, 0.02f, 0.02f);
    t3d_mat4_to_fixed(modelMatFP, &modelMat);

    // ======== Draw (3D) ======== //
    rdpq_attach(display_get(), display_get_zbuf()); // set the target to draw to
    t3d_frame_start(); // call this once per frame at the beginning of your draw function

    t3d_viewport_attach(&viewport); // now use the viewport, this applies proj/view matrices and sets scissoring

    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    // this cleans the entire screen (even if out viewport is smaller)
    t3d_screen_clear_color(RGBA32(252, 135, 126, 0));
    t3d_screen_clear_depth();

    t3d_light_set_ambient(colorAmbient); // one global ambient light, always active
    t3d_light_set_directional(0, colorDir, &lightDirVec); // optional directional light, can be disabled
    t3d_light_set_count(1);

    t3d_state_set_drawflags(T3D_FLAG_SHADED | T3D_FLAG_DEPTH);

    bool p1_moveback1 = false;
    bool p2_moveback1 = false;
    processBox(players[0].position.v[0], players[0].position.v[2], players[1].position.v[0], players[1].position.v[2], deltatime, &box1, &p1_moveback1, &p2_moveback1);
    bool p1_moveback2 = false;
    bool p2_moveback2 = false;
    processBox(players[0].position.v[0], players[0].position.v[2], players[1].position.v[0], players[1].position.v[2], deltatime, &box2, &p1_moveback2, &p2_moveback2);
    bool p1_moveback3 = false;
    bool p2_moveback3 = false;
    processBox(players[0].position.v[0], players[0].position.v[2],players[1].position.v[0], players[1].position.v[2], deltatime, &box3, &p1_moveback3, &p2_moveback3);

    bool p1_moveBack = p1_moveback1 || p1_moveback2 || p1_moveback3;
    bool p2_moveBack = p2_moveback1 || p2_moveback2 || p2_moveback3;

    if (p1_moveBack)
    {
        if (players[0].velocity.v[2] > 0)
        {
            players[0].velocity.v[2] = 0;
        }
    }

    if (p2_moveBack)
    {
        if (players[1].velocity.v[2] < 0)
        {
            players[1].velocity.v[2] = 0;
        }
    }

    p1_moveback1 = false;
    p2_moveback1 = false;
    processBox_h(players[0].position.v[0], players[0].position.v[2], players[3].position.v[0], players[3].position.v[2], deltatime, &box4, &p1_moveback1, &p2_moveback1);
    p1_moveback2 = false;
    p2_moveback2 = false;
    processBox_h(players[0].position.v[0], players[0].position.v[2], players[3].position.v[0], players[3].position.v[2], deltatime, &box5, &p1_moveback2, &p2_moveback2);
    p1_moveback3 = false;
    p2_moveback3 = false;
    processBox_h(players[0].position.v[0], players[0].position.v[2],players[3].position.v[0], players[3].position.v[2], deltatime, &box6, &p1_moveback3, &p2_moveback3);

    p1_moveBack = p1_moveback1 || p1_moveback2 || p1_moveback3;
    p2_moveBack = p2_moveback1 || p2_moveback2 || p2_moveback3;

    if (p1_moveBack)
    {
        if (players[0].velocity.v[0] < 0)
        {
            players[0].velocity.v[0] = 0;
        }
    }

    if (p2_moveBack)
    {
        if (players[3].velocity.v[0] > 0)
        {
            players[3].velocity.v[0] = 0;
        }
    }

    p1_moveback1 = false;
    p2_moveback1 = false;
    processBox_h(players[1].position.v[0], players[1].position.v[2], players[2].position.v[0], players[2].position.v[2], deltatime, &box7, &p1_moveback1, &p2_moveback1);
    p1_moveback2 = false;
    p2_moveback2 = false;
    processBox_h(players[1].position.v[0], players[1].position.v[2], players[2].position.v[0], players[2].position.v[2], deltatime, &box8, &p1_moveback2, &p2_moveback2);
    p1_moveback3 = false;
    p2_moveback3 = false;
    processBox_h(players[1].position.v[0], players[1].position.v[2],players[2].position.v[0], players[2].position.v[2], deltatime, &box9, &p1_moveback3, &p2_moveback3);

    p1_moveBack = p1_moveback1 || p1_moveback2 || p1_moveback3;
    p2_moveBack = p2_moveback1 || p2_moveback2 || p2_moveback3;

    if (p1_moveBack)
    {
        if (players[1].velocity.v[0] < 0)
        {
            players[1].velocity.v[0] = 0;
        }
    }

    if (p2_moveBack)
    {
        if (players[2].velocity.v[0] > 0)
        {
            players[2].velocity.v[0] = 0;
        }
    }

    p1_moveback1 = false;
    p2_moveback1 = false;
    processBox(players[3].position.v[0], players[3].position.v[2], players[2].position.v[0], players[2].position.v[2], deltatime, &box10, &p1_moveback1, &p2_moveback1);
    p1_moveback2 = false;
    p2_moveback2 = false;
    processBox(players[3].position.v[0], players[3].position.v[2], players[2].position.v[0], players[2].position.v[2], deltatime, &box11, &p1_moveback2, &p2_moveback2);
    p1_moveback3 = false;
    p2_moveback3 = false;
    processBox(players[3].position.v[0], players[3].position.v[2],players[2].position.v[0], players[2].position.v[2], deltatime, &box12, &p1_moveback3, &p2_moveback3);

    p1_moveBack = p1_moveback1 || p1_moveback2 || p1_moveback3;
    p2_moveBack = p2_moveback1 || p2_moveback2 || p2_moveback3;

    if (p1_moveBack)
    {
        if (players[3].velocity.v[2] > 0)
        {
            players[3].velocity.v[2] = 0;
        }
    }

    if (p2_moveBack)
    {
        if (players[2].velocity.v[2] < 0)
        {
            players[2].velocity.v[2] = 0;
        }
    }

    mutateBoxPostion(&box1, 0.05f);
    mutateBoxPostion(&box2, 0.05f);
    mutateBoxPostion(&box3, 0.05f);

    mutateBoxPostion(&box4, 0.05f);
    mutateBoxPostion(&box5, 0.05f);
    mutateBoxPostion(&box6, 0.05f);

    mutateBoxPostion(&box7, 0.05f);
    mutateBoxPostion(&box8, 0.05f);
    mutateBoxPostion(&box9, 0.05f);

    mutateBoxPostion(&box10, 0.05f);
    mutateBoxPostion(&box11, 0.05f);
    mutateBoxPostion(&box12, 0.05f);

    for (int i = 0; i < 4; i++)
    {
        bool can_move = true;

        if (p1_dead && i == 0)
        {
            can_move = false;
        }
        if (p2_dead && i == 1)
        {
            can_move = false;
        }
        if (p3_dead && i == 2)
        {
            can_move = false;
        }
        if (p4_dead && i == 3)
        {
            can_move = false;
        }

        if (can_move)
        {
            players[i].position.v[0] += players[i].velocity.v[0];
            players[i].position.v[2] += players[i].velocity.v[2];
            mutateBoxPostion(&players[i], 0.02f);
        }
    }

    // t3d functions can be recorded into a display list:
    if(!dplDraw) {
      rspq_block_begin();

    //   t3d_matrix_push(modelMatFP);
    //   t3d_model_draw(mapModel);
    //   t3d_matrix_pop(1);

      t3d_matrix_push(box1.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box2.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box3.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box4.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box5.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box6.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box7.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box8.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box9.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box10.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box11.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      t3d_matrix_push(box12.matrixFP);
      t3d_model_draw(boxModel);
      t3d_matrix_pop(1);

      for (int i = 0; i < 4; i++)
      {
        t3d_matrix_push(players[i].matrixFP);
        t3d_model_draw(boxModel);
        t3d_matrix_pop(1);
      }
      

      dplDraw = rspq_block_end();
    }

    rspq_block_run(dplDraw);

    rdpq_sync_pipe();

    rdpq_set_mode_standard();

    in_bounds = in_square_bounds(boxPosition.v[0], boxPosition.v[2],box1.position.v[0], box1.position.v[2], 10 );

    // if (in_bounds)
    // {
    //     rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 155, 100, "IN BOUNDS!");
    // }
    // else
    // {
    //     rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 155, 100, "NOT IN BOUNDS");
    // }

    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 155, 200, "CamPos (%f, %f)", camPos.y, camPos.z);
    

    if (is_countdown()) {
        // Draw countdown
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 155, 100, "%d", (int)ceilf(countdown_timer));
    } else if (countdown_timer > -GO_DELAY) {
        // For a short time after countdown is over, draw "GO!"
        rdpq_text_print(NULL, FONT_BUILTIN_DEBUG_MONO, 150, 100, "GO!");
    } else if (is_ending && end_timer >= WIN_SHOW_DELAY) {
        // Draw winner announcement (There might be multiple winners)
        int ycur = 100;
        int i = winningPlayer;
        ycur += rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 120, ycur, "Player %d wins!\n", i+1).advance_y;
    }

    rdpq_detach_show();
}

/*==============================
    minigame_cleanup
    Clean up any memory used by your game just before it ends.
==============================*/
void minigame_cleanup()
{
    wav64_close(&sfx_start);
    wav64_close(&sfx_countdown);
    wav64_close(&sfx_stop);
    wav64_close(&sfx_winner);

    t3d_destroy();
    display_close();

    rdpq_text_unregister_font(FONT_TEXT);
    rdpq_font_free(font);
}