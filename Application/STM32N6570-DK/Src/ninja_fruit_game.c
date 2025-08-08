
/**
 ******************************************************************************
 * @file    ninja_fruit_game.c
 * @author  Antonio Mendoza
 * @brief   Ninja Fruit Game Implementation
 ******************************************************************************
 */

#include "ninja_fruit_game.h"
#include "utils.h"
#include <stdlib.h>
#include <math.h>


#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Fruit colors
static const uint32_t fruit_colors[FRUIT_TYPES_COUNT] = {
    UTIL_LCD_COLOR_RED,     // Apple
    UTIL_LCD_COLOR_ORANGE,  // Orange
    UTIL_LCD_COLOR_YELLOW,  // Banana
    0xFFFF8080              // Strawberry (pink)
};

// Fruit names for display
static const char* fruit_names[FRUIT_TYPES_COUNT] = {
    "Apple", "Orange", "Banana", "Berry"
};

void NinjaGame_Init(NinjaGame_t *game)
{
    memset(game, 0, sizeof(NinjaGame_t));

    // Initialize all fruits as inactive
    for (int i = 0; i < MAX_FRUITS; i++) {
        game->fruits[i].state = FRUIT_STATE_INACTIVE;
    }

    game->spawn_rate_multiplier = 1.0f;
    game->level = 1;
    game->mode = NINJA_MODE_SLICE;
}

void NinjaGame_Update(NinjaGame_t *game, GestureDetector_t *gesture_detector, spe_pp_outBuffer_t *keypoints)
{
    uint32_t current_time = HAL_GetTick();

    // Start game on first gesture detection
    if (!game->game_started && !game->game_over) {
        GestureType_t current_gesture = Gesture_GetCurrentDisplayGesture(gesture_detector);
        if (current_gesture != GESTURE_NONE) {
            game->game_started = 1;
            game->game_start_time = current_time;
        }
        return;
    }

    if (game->game_over) {
        // Check for restart gesture
        GestureType_t current_gesture = Gesture_GetCurrentDisplayGesture(gesture_detector);
        if (current_gesture != GESTURE_NONE) {
            NinjaGame_Reset(game);
        }
        return;
    }

    // Update difficulty based on time played
    uint32_t game_time = current_time - game->game_start_time;
    game->level = (game_time / 30000) + 1; // Increase level every 30 seconds
    game->spawn_rate_multiplier = 1.0f + (game->level - 1) * 0.3f;

    // Spawn new fruits
    uint32_t spawn_interval = FRUIT_SPAWN_INTERVAL / game->spawn_rate_multiplier;
    if (current_time - game->last_spawn_time > spawn_interval) {
        SpawnFruit(game);
        game->last_spawn_time = current_time;
    }

    // Update existing fruits
    UpdateFruits(game);

    if (game->mode == NINJA_MODE_SLICE) {
        // Check for slicing gestures
        GestureType_t detected_gesture = Gesture_GetCurrentDisplayGesture(gesture_detector);

        if (detected_gesture == GESTURE_LEFT_ARM_SWIPE_LEFT ||
            detected_gesture == GESTURE_LEFT_ARM_SWIPE_RIGHT ||
            detected_gesture == GESTURE_RIGHT_ARM_SWIPE_LEFT ||
            detected_gesture == GESTURE_RIGHT_ARM_SWIPE_RIGHT ||
            detected_gesture == GESTURE_SWORD_OVERHEAD_STRIKE ||
            detected_gesture == GESTURE_SWORD_SIDE_SLASH) {

            // Create swipe trajectory from wrist movement
            SwipeTrajectory_t swipe = {0};

            // Get current wrist position
            if (detected_gesture == GESTURE_LEFT_ARM_SWIPE_LEFT || detected_gesture == GESTURE_LEFT_ARM_SWIPE_RIGHT) {
                swipe.end_x = keypoints[KEYPOINT_LEFT_WRIST].x_center;
                swipe.end_y = keypoints[KEYPOINT_LEFT_WRIST].y_center;
            } else {
                swipe.end_x = keypoints[KEYPOINT_RIGHT_WRIST].x_center;
                swipe.end_y = keypoints[KEYPOINT_RIGHT_WRIST].y_center;
            }

            // Estimate start position based on gesture direction
            float32_t dx = (detected_gesture == GESTURE_LEFT_ARM_SWIPE_RIGHT ||
                           detected_gesture == GESTURE_RIGHT_ARM_SWIPE_RIGHT) ? -0.2f : 0.2f;
            float32_t dy = (detected_gesture == GESTURE_SWORD_OVERHEAD_STRIKE) ? -0.3f : 0.0f;

            swipe.start_x = swipe.end_x + dx;
            swipe.start_y = swipe.end_y + dy;
            swipe.timestamp = current_time;
            swipe.active = 1;

            CheckSlices(game, &swipe);
        }
    } else {
        // Pop mode: check if wrists are inside fruit circles
        CheckBubblePops(game, keypoints);
    }

    // Check game over condition
    if (game->missed_count >= MAX_MISSED_FRUITS) {
        game->game_over = 1;
    }
}

static void SpawnFruit(NinjaGame_t *game)
{
    // Find inactive fruit slot
    for (int i = 0; i < MAX_FRUITS; i++) {
        if (game->fruits[i].state == FRUIT_STATE_INACTIVE) {
            Fruit_t *fruit = &game->fruits[i];

            // Random spawn position at top of screen
            fruit->x = 0.25f + (rand() % 500) / 1000.0f; // Random X between 0.1 and 0.9
            fruit->y = 0.03 + (FRUIT_SIZE/2)/SCREEN_HEIGHT;

            // Random horizontal drift
            fruit->velocity_x = ((rand() % 100) - 50) / 10000.0f; // Small horizontal movement
            fruit->velocity_y = FRUIT_FALL_SPEED + ((rand() % 50) / 1000.0f); // Random fall speed variation

            // Random fruit type
            fruit->type = rand() % FRUIT_TYPES_COUNT;
            fruit->state = FRUIT_STATE_FALLING;
            fruit->spawn_time = HAL_GetTick();

            break;
        }
    }
}

static void UpdateFruits(NinjaGame_t *game)
{
    uint32_t current_time = HAL_GetTick();
    float32_t dt = 0.016f; // Assuming ~60fps update rate

    for (int i = 0; i < MAX_FRUITS; i++) {
        Fruit_t *fruit = &game->fruits[i];

        if (fruit->state == FRUIT_STATE_FALLING) {
            // Update position
            fruit->y += fruit->velocity_y * dt;
            fruit->x += fruit->velocity_x * dt;

            // Check if fruit fell off screen
            if (fruit->y > (0.98- (FRUIT_SIZE/2)/SCREEN_HEIGHT)) {
                fruit->state = FRUIT_STATE_MISSED;
                game->missed_count++;
            }
        }
        else if (fruit->state == FRUIT_STATE_SLICED) {
            // Remove sliced fruits after animation time
            if (current_time - fruit->slice_time > SLICE_ANIMATION_TIME) {
                fruit->state = FRUIT_STATE_INACTIVE;
            }
        }
        else if (fruit->state == FRUIT_STATE_MISSED) {
            // Remove missed fruits immediately
            fruit->state = FRUIT_STATE_INACTIVE;
        }
    }
}

static void CheckSlices(NinjaGame_t *game, SwipeTrajectory_t *swipe)
{
    if (!swipe->active) return;

    for (int i = 0; i < MAX_FRUITS; i++) {
        Fruit_t *fruit = &game->fruits[i];

        if (fruit->state == FRUIT_STATE_FALLING) {
            // Check if swipe line intersects with fruit circle
            float32_t radius = FRUIT_SIZE / 2.0f / 800.0f; // Normalized radius

            if (LineIntersectsCircle(swipe->start_x, swipe->start_y,
                                   swipe->end_x, swipe->end_y,
                                   fruit->x, fruit->y, radius)) {

                // Fruit sliced!
                fruit->state = FRUIT_STATE_SLICED;
                fruit->slice_time = HAL_GetTick();
                fruit->slice_direction = (swipe->end_x > swipe->start_x) ? 1 : 0;

                // Increase score based on fruit type and level
                uint32_t base_score = 10;
                switch (fruit->type) {
                    case FRUIT_APPLE: base_score = 10; break;
                    case FRUIT_ORANGE: base_score = 15; break;
                    case FRUIT_BANANA: base_score = 20; break;
                    case FRUIT_STRAWBERRY: base_score = 25; break;
                }

                game->score += base_score * game->level;
            }
        }
    }
}

static void CheckBubblePops(NinjaGame_t *game, spe_pp_outBuffer_t *keypoints)
{
    float32_t lx = keypoints[KEYPOINT_LEFT_WRIST].x_center;
    float32_t ly = keypoints[KEYPOINT_LEFT_WRIST].y_center;
    float32_t rx = keypoints[KEYPOINT_RIGHT_WRIST].x_center;
    float32_t ry = keypoints[KEYPOINT_RIGHT_WRIST].y_center;
    float32_t radius = FRUIT_SIZE / 2.0f / 800.0f;
    float32_t radius_sq = radius * radius;

    for (int i = 0; i < MAX_FRUITS; i++) {
        Fruit_t *fruit = &game->fruits[i];

        if (fruit->state == FRUIT_STATE_FALLING) {
            float32_t dx = lx - fruit->x;
            float32_t dy = ly - fruit->y;
            float32_t dist_left_sq = dx * dx + dy * dy;

            dx = rx - fruit->x;
            dy = ry - fruit->y;
            float32_t dist_right_sq = dx * dx + dy * dy;

            if (dist_left_sq <= radius_sq || dist_right_sq <= radius_sq) {
                fruit->state = FRUIT_STATE_SLICED;
                fruit->slice_time = HAL_GetTick();
                fruit->slice_direction = 0;

                uint32_t base_score = 10;
                switch (fruit->type) {
                    case FRUIT_APPLE: base_score = 10; break;
                    case FRUIT_ORANGE: base_score = 15; break;
                    case FRUIT_BANANA: base_score = 20; break;
                    case FRUIT_STRAWBERRY: base_score = 25; break;
                }
                game->score += base_score * game->level;
            }
        }
    }
}

static uint8_t LineIntersectsCircle(float32_t x1, float32_t y1, float32_t x2, float32_t y2,
                                   float32_t cx, float32_t cy, float32_t radius)
{
    // Calculate distance from circle center to line segment
    float32_t dx = x2 - x1;
    float32_t dy = y2 - y1;
    float32_t length = sqrtf(dx*dx + dy*dy);

    if (length < 0.001f) return 0; // Degenerate line

    // Normalize direction vector
    dx /= length;
    dy /= length;

    // Vector from line start to circle center
    float32_t fx = cx - x1;
    float32_t fy = cy - y1;

    // Project circle center onto line
    float32_t t = fx * dx + fy * dy;

    // Clamp to line segment
    if (t < 0) t = 0;
    if (t > length) t = length;

    // Find closest point on line segment
    float32_t closest_x = x1 + t * dx;
    float32_t closest_y = y1 + t * dy;

    // Check distance to circle
    float32_t dist_sq = (cx - closest_x) * (cx - closest_x) + (cy - closest_y) * (cy - closest_y);

    return dist_sq <= (radius * radius);
}

void NinjaGame_Render(NinjaGame_t *game)
{
    uint32_t screen_width = SCREEN_WIDTH;
    uint32_t screen_height = SCREEN_HEIGHT;

    if (!game->game_started && !game->game_over) {
        // Show start screen
        UTIL_LCD_SetBackColor(0x40000000);
        UTIL_LCDEx_PrintfAt(0, LINE(8), CENTER_MODE, "NINJA FRUIT SLICER");
        UTIL_LCDEx_PrintfAt(0, LINE(10), CENTER_MODE, "Make any gesture to start!");
        UTIL_LCDEx_PrintfAt(0, LINE(12), CENTER_MODE, "Slice fruits with arm swipes");
        UTIL_LCDEx_PrintfAt(0, LINE(13), CENTER_MODE, "Don't let %d fruits fall!", MAX_MISSED_FRUITS);
        UTIL_LCD_SetBackColor(0);
        return;
    }

    if (game->game_over) {
        // Show game over screen
        UTIL_LCD_SetBackColor(0x40FF0000);
        UTIL_LCDEx_PrintfAt(0, LINE(8), CENTER_MODE, "GAME OVER!");
        UTIL_LCDEx_PrintfAt(0, LINE(10), CENTER_MODE, "Final Score: %lu", game->score);
        UTIL_LCDEx_PrintfAt(0, LINE(11), CENTER_MODE, "Level Reached: %lu", game->level);
        UTIL_LCDEx_PrintfAt(0, LINE(13), CENTER_MODE, "Make any gesture to restart");
        UTIL_LCD_SetBackColor(0);
        return;
    }

    // Render fruits
    for (int i = 0; i < MAX_FRUITS; i++) {
        if (game->fruits[i].state != FRUIT_STATE_INACTIVE) {
            RenderFruit(&game->fruits[i], screen_width, screen_height);
        }
    }

    // Render UI
    RenderUI(game);
}

static void RenderFruit(Fruit_t *fruit, uint32_t screen_width, uint32_t screen_height)
{
    // Convert normalized coordinates to screen coordinates
    int screen_x = (int)(fruit->x * screen_width);
    int screen_y = (int)(fruit->y * screen_height);

    uint32_t color = fruit_colors[fruit->type];

    if (fruit->state == FRUIT_STATE_FALLING) {
        // Draw whole fruit
        UTIL_LCD_FillCircle(screen_x, screen_y, FRUIT_SIZE/2, color);

        // Add highlight for 3D effect
        UTIL_LCD_FillCircle(screen_x - FRUIT_SIZE/6, screen_y - FRUIT_SIZE/6, FRUIT_SIZE/8,
                           UTIL_LCD_COLOR_WHITE);
    }
    else if (fruit->state == FRUIT_STATE_SLICED) {
        // Draw sliced fruit halves
        uint32_t elapsed = HAL_GetTick() - fruit->slice_time;
        float32_t separation = (elapsed / (float32_t)SLICE_ANIMATION_TIME) * FRUIT_SIZE;

        if (fruit->slice_direction) {
            // Right slice - left half moves left, right half moves right
            UTIL_LCD_FillCircle(screen_x - separation/2, screen_y + separation/4, FRUIT_SIZE/3, color);
            UTIL_LCD_FillCircle(screen_x + separation/2, screen_y + separation/4, FRUIT_SIZE/3, color);
        } else {
            // Left slice
            UTIL_LCD_FillCircle(screen_x + separation/2, screen_y + separation/4, FRUIT_SIZE/3, color);
            UTIL_LCD_FillCircle(screen_x - separation/2, screen_y + separation/4, FRUIT_SIZE/3, color);
        }
    }
}

static void RenderUI(NinjaGame_t *game)
{
    UTIL_LCD_SetBackColor(0x40000000);

    // Score
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCDEx_PrintfAt(0, LINE(1), LEFT_MODE, "Score: %lu", game->score);

    // Level
    UTIL_LCDEx_PrintfAt(0, LINE(2), LEFT_MODE, "Level: %lu", game->level);

    // Missed fruits
    if (game->missed_count > MAX_MISSED_FRUITS / 2) {
        UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_RED);
    }
    UTIL_LCDEx_PrintfAt(0, LINE(3), LEFT_MODE, "Missed: %lu/%d", game->missed_count, MAX_MISSED_FRUITS);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);

    // Game time
    uint32_t game_time = (HAL_GetTick() - game->game_start_time) / 1000;
    UTIL_LCDEx_PrintfAt(600, LINE(1), LEFT_MODE, "Time: %lus", game_time);

    UTIL_LCD_SetBackColor(0);
}

void NinjaGame_Reset(NinjaGame_t *game)
{
    // Reset game state but keep high score for comparison
    uint32_t high_score = (game->score > 0) ? game->score : 0;

    NinjaGame_Init(game);
    game->game_started = 1;
    game->game_start_time = HAL_GetTick();

    // Could store high score for display
}

void NinjaGame_SetMode(NinjaGame_t *game, NinjaGameMode_t mode)
{
    game->mode = mode;
}
