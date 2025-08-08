/**
 ******************************************************************************
 * @file    ninja_fruit_game.h
 * @author  Antonio Mendoza
 * @brief   Ninja Fruit Game Header - Virtual fruit slicing game using pose detection
 ******************************************************************************
 */

#ifndef __NINJA_FRUIT_GAME_H
#define __NINJA_FRUIT_GAME_H

#include "main.h"
#include "gesture_detection.h"
#include <stdint.h>

// Game configuration
#define MAX_FRUITS 8
#define FRUIT_SPAWN_INTERVAL 2000  // milliseconds
#define FRUIT_FALL_SPEED 0.15f     // normalized screen coordinates per second
#define FRUIT_SIZE 30              // pixels
#define SLICE_TOLERANCE 40         // pixels for slice detection
#define MAX_MISSED_FRUITS 5        // game over condition
#define SLICE_ANIMATION_TIME 500   // milliseconds

// Fruit types
typedef enum {
    FRUIT_APPLE = 0,
    FRUIT_ORANGE,
    FRUIT_BANANA,
    FRUIT_STRAWBERRY,
    FRUIT_TYPES_COUNT
} FruitType_t;

// Fruit state
typedef enum {
    FRUIT_STATE_FALLING = 0,
    FRUIT_STATE_SLICED,
    FRUIT_STATE_MISSED,
    FRUIT_STATE_INACTIVE
} FruitState_t;

// Gameplay mode
typedef enum {
    NINJA_MODE_SLICE = 0,
    NINJA_MODE_POP
} NinjaGameMode_t;



// Fruit structure
typedef struct {
    float32_t x;                    // normalized screen coordinates (0.0 to 1.0)
    float32_t y;                    // normalized screen coordinates (0.0 to 1.0)
    float32_t velocity_y;           // falling speed
    float32_t velocity_x;           // horizontal drift
    FruitType_t type;
    FruitState_t state;
    uint32_t spawn_time;
    uint32_t slice_time;
    uint8_t slice_direction;        // 0=left, 1=right
} Fruit_t;

// Game state
typedef struct {
    Fruit_t fruits[MAX_FRUITS];
    uint32_t score;
    uint32_t missed_count;
    uint32_t last_spawn_time;
    uint8_t game_over;
    uint8_t game_started;
    uint32_t game_start_time;
    uint32_t level;                 // increases difficulty
    float32_t spawn_rate_multiplier; // increases spawn frequency
    NinjaGameMode_t mode;           // slicing or pop mode
} NinjaGame_t;

// Swipe trajectory for slice detection
typedef struct {
    float32_t start_x, start_y;
    float32_t end_x, end_y;
    uint32_t timestamp;
    uint8_t active;
} SwipeTrajectory_t;

// Function prototypes
void NinjaGame_Init(NinjaGame_t *game);
void NinjaGame_Update(NinjaGame_t *game, GestureDetector_t *gesture_detector, spe_pp_outBuffer_t *keypoints);
void NinjaGame_Render(NinjaGame_t *game);
void NinjaGame_Reset(NinjaGame_t *game);
void NinjaGame_SetMode(NinjaGame_t *game, NinjaGameMode_t mode);
// Private functions
static void SpawnFruit(NinjaGame_t *game);
static void UpdateFruits(NinjaGame_t *game);
static void CheckSlices(NinjaGame_t *game, SwipeTrajectory_t *swipe);
static void CheckBubblePops(NinjaGame_t *game, spe_pp_outBuffer_t *keypoints);
static void RenderFruit(Fruit_t *fruit, uint32_t screen_width, uint32_t screen_height);
static void RenderUI(NinjaGame_t *game);
static uint8_t LineIntersectsCircle(float32_t x1, float32_t y1, float32_t x2, float32_t y2,
                                   float32_t cx, float32_t cy, float32_t radius);

#endif /* __NINJA_FRUIT_GAME_H */

