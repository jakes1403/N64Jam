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

const MinigameDef minigame_def = {
    .gamename = "Jakes Game",
    .developername = "jakes1403",
    .description = "Test Game",
    .instructions = "Press A to win."
};

T3DMat4 modelMat; // matrix for our model, this is a "normal" float matrix

T3DMat4FP* modelMatFP = NULL;

const T3DVec3 camPos = {{0,0,-18}};
const T3DVec3 camTarget = {{0,0,0}};

uint8_t colorAmbient[4] = {80, 80, 100, 0xFF};
uint8_t colorDir[4]     = {0xEE, 0xAA, 0xAA, 0xFF};

T3DVec3 lightDirVec = {{-1.0f, 1.0f, 1.0f}};

float rotAngle = 0.0f;

T3DVec3 rotAxis = {{-1.0f, 2.5f, 0.25f}};

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

T3DModel *model = NULL;

/*==============================
    minigame_init
    The minigame initialization function
==============================*/
void minigame_init()
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    rdpq_init();

    font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    rdpq_text_register_font(FONT_TEXT, font);

    t3d_init((T3DInitParams){}); // Init library itself, use empty params for default settings

    t3d_mat4_identity(&modelMat);
    // Now allocate a fixed-point matrix, this is what t3d uses internally.
    modelMatFP = malloc_uncached(sizeof(T3DMat4FP));


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

    model = t3d_model_load("rom:/jake_game/model.t3dm");
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
    for (size_t i = 0; i < core_get_playercount(); i++)
    {
        // For human players, check if the physical A button on the controller was pressed
        joypad_buttons_t btn = joypad_get_buttons_pressed(core_get_playercontroller(i));
        if (btn.a)
        {
            core_set_winner(i);
            is_ending = true;
            wav64_play(&sfx_stop, 31);
            winningPlayer = i;
        }
    }

    // ======== Update ======== //
    rotAngle += 0.03f;

    // we can set up our viewport settings beforehand here
    t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(85.0f), 10.0f, 100.0f);
    t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(T3DVec3){{0,1,0}});

    // Model-Matrix, t3d offers some basic matrix functions
    t3d_mat4_identity(&modelMat);
    t3d_mat4_rotate(&modelMat, &rotAxis, rotAngle);
    t3d_mat4_scale(&modelMat, 0.02f, 0.02f, 0.02f);
    t3d_mat4_to_fixed(modelMatFP, &modelMat);

    // ======== Draw (3D) ======== //
    rdpq_attach(display_get(), display_get_zbuf()); // set the target to draw to
    t3d_frame_start(); // call this once per frame at the beginning of your draw function

    t3d_viewport_attach(&viewport); // now use the viewport, this applies proj/view matrices and sets scissoring

    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    // this cleans the entire screen (even if out viewport is smaller)
    t3d_screen_clear_color(RGBA32(100, 0, 100, 0));
    t3d_screen_clear_depth();

    t3d_light_set_ambient(colorAmbient); // one global ambient light, always active
    t3d_light_set_directional(0, colorDir, &lightDirVec); // optional directional light, can be disabled
    t3d_light_set_count(1);

    t3d_state_set_drawflags(T3D_FLAG_SHADED | T3D_FLAG_DEPTH);

    // t3d functions can be recorded into a display list:
    if(!dplDraw) {
      rspq_block_begin();

      t3d_matrix_push(modelMatFP);
      // Draw the model, material settings (e.g. textures, color-combiner) are handled internally
      t3d_model_draw(model);
      t3d_matrix_pop(1);

      dplDraw = rspq_block_end();
    }

    rspq_block_run(dplDraw);

    rdpq_set_mode_standard();

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