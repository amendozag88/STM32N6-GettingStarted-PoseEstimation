#include "simon_says_game.h"
#include "gesture_detection.h"
#include "utils.h"
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_audio.h"
#include "stm32n6570_discovery_sd.h"
#include "ff.h"
#include "stm32_lcd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RESPONSE_TIMEOUT_MS 2000U
#define RESULT_DISPLAY_MS   1000U

static const char *pose_names[POSE_COUNT] = {
    "Hands Up",
    "Hands On Head",
    "Hands On Hips",
    "Left Arm Out",
    "Right Arm Out"
};

static const char *pose_audio_files[POSE_COUNT] = {
    "hands_up.wav",
    "hands_on_head.wav",
    "hands_on_hips.wav",
    "left_arm_out.wav",
    "right_arm_out.wav"
};

static FATFS sd_fs;
static uint8_t audio_ready = 0;

static int PoseMatched(SimonPose_t pose, spe_pp_outBuffer_t *k)
{
    float32_t lx = k[KEYPOINT_LEFT_WRIST].x_center;
    float32_t ly = k[KEYPOINT_LEFT_WRIST].y_center;
    float32_t rx = k[KEYPOINT_RIGHT_WRIST].x_center;
    float32_t ry = k[KEYPOINT_RIGHT_WRIST].y_center;
    switch(pose)
    {
    case POSE_HANDS_UP:
        return (ly < k[KEYPOINT_LEFT_SHOULDER].y_center &&
                ry < k[KEYPOINT_RIGHT_SHOULDER].y_center);
    case POSE_HANDS_ON_HEAD:
        return (ly < k[KEYPOINT_NOSE].y_center &&
                ry < k[KEYPOINT_NOSE].y_center &&
                fabsf(lx - k[KEYPOINT_NOSE].x_center) < 0.15f &&
                fabsf(rx - k[KEYPOINT_NOSE].x_center) < 0.15f);
    case POSE_HANDS_ON_HIPS:
        return (fabsf(lx - k[KEYPOINT_LEFT_HIP].x_center) < 0.1f &&
                fabsf(ly - k[KEYPOINT_LEFT_HIP].y_center) < 0.1f &&
                fabsf(rx - k[KEYPOINT_RIGHT_HIP].x_center) < 0.1f &&
                fabsf(ry - k[KEYPOINT_RIGHT_HIP].y_center) < 0.1f);
    case POSE_LEFT_ARM_OUT:
        return (lx < k[KEYPOINT_LEFT_SHOULDER].x_center - 0.25f &&
                fabsf(ly - k[KEYPOINT_LEFT_SHOULDER].y_center) < 0.15f);
    case POSE_RIGHT_ARM_OUT:
        return (rx > k[KEYPOINT_RIGHT_SHOULDER].x_center + 0.25f &&
                fabsf(ry - k[KEYPOINT_RIGHT_SHOULDER].y_center) < 0.15f);
    default:
        return 0;
    }
}

static void Audio_Init(uint32_t sample_rate, uint32_t bits, uint32_t channels)
{
    if (audio_ready)
        return;
    BSP_AUDIO_Init_t init;
    init.Device = AUDIO_OUT_DEVICE_HEADPHONE;
    init.SampleRate = sample_rate;
    init.BitsPerSample = bits;
    init.ChannelsNbr = channels;
    init.Volume = 70;
    if (BSP_AUDIO_OUT_Init(0, &init) == BSP_ERROR_NONE)
    {
        audio_ready = 1;
    }
}

static void PlayWav(const char *name)
{
    FIL file;
    if (f_open(&file, name, FA_READ) != FR_OK)
        return;
    UINT br;
    uint8_t header[44];
    f_read(&file, header, sizeof(header), &br);
    uint32_t sample_rate = *(uint32_t*)&header[24];
    uint16_t channels = *(uint16_t*)&header[22];
    uint16_t bits = *(uint16_t*)&header[34];
    uint32_t data_size = *(uint32_t*)&header[40];
    uint8_t *buffer = malloc(data_size);
    if (!buffer)
    {
        f_close(&file);
        return;
    }
    f_read(&file, buffer, data_size, &br);
    f_close(&file);
    Audio_Init(sample_rate, bits, channels);
    BSP_AUDIO_OUT_Play(0, buffer, data_size);
    uint32_t state;
    do {
        BSP_AUDIO_OUT_GetState(0, &state);
    } while(state == AUDIO_OUT_STATE_PLAYING);
    BSP_AUDIO_OUT_Stop(0);
    free(buffer);
}

static void NewInstruction(SimonSaysGame_t *g)
{
    g->current_pose = rand() % POSE_COUNT;
    g->require_simon = rand() & 1U;
    g->instruction_time = HAL_GetTick();
    g->waiting_for_player = 1;
    g->result = 0;
    if (!audio_ready)
    {
        BSP_SD_Init(0);
        f_mount(&sd_fs, "", 1);
    }
    if (g->require_simon)
    {
        PlayWav("simon_says.wav");
    }
    PlayWav(pose_audio_files[g->current_pose]);
}

void SimonSays_Init(SimonSaysGame_t *g)
{
    memset(g, 0, sizeof(*g));
    NewInstruction(g);
}

void SimonSays_Update(SimonSaysGame_t *g, spe_pp_outBuffer_t *k)
{
    uint32_t now = HAL_GetTick();
    if (g->waiting_for_player)
    {
        if (now - g->instruction_time > RESPONSE_TIMEOUT_MS)
        {
            int matched = PoseMatched(g->current_pose, k);
            if ((matched && g->require_simon) || (!matched && !g->require_simon))
            {
                g->score++;
                g->result = 1;
            }
            else
            {
                g->result = 2;
            }
            g->waiting_for_player = 0;
            g->instruction_time = now;
        }
    }
    else if (now - g->instruction_time > RESULT_DISPLAY_MS)
    {
        NewInstruction(g);
    }
}

void SimonSays_Render(SimonSaysGame_t *g)
{
    UTIL_LCD_SetBackColor(0x40000000);
    if (g->waiting_for_player)
    {
        if (g->require_simon)
        {
            UTIL_LCDEx_PrintfAt(0, LINE(20), CENTER_MODE, "SIMON SAYS %s", pose_names[g->current_pose]);
        }
        else
        {
            UTIL_LCDEx_PrintfAt(0, LINE(20), CENTER_MODE, "%s", pose_names[g->current_pose]);
        }
    }
    else if (g->result == 1)
    {
        UTIL_LCDEx_PrintfAt(0, LINE(20), CENTER_MODE, "Correct!");
    }
    else if (g->result == 2)
    {
        UTIL_LCDEx_PrintfAt(0, LINE(20), CENTER_MODE, "Wrong!");
    }
    UTIL_LCDEx_PrintfAt(0, LINE(22), CENTER_MODE, "Score: %lu", g->score);
    UTIL_LCD_SetBackColor(0);
}
