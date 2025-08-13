#ifndef SIMON_SAYS_GAME_H
#define SIMON_SAYS_GAME_H

#include <stdint.h>
#include "display_spe.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Number of supported poses in Simon Says */
typedef enum {
    POSE_HANDS_UP = 0,
    POSE_HANDS_ON_HEAD,
    POSE_HANDS_ON_HIPS,
    POSE_LEFT_ARM_OUT,
    POSE_RIGHT_ARM_OUT,
    POSE_COUNT
} SimonPose_t;

/** Game context for Simon Says mode */
typedef struct {
    SimonPose_t current_pose;      /**< Pose requested in current round */
    uint8_t require_simon;         /**< 1 if instruction prefixed with "Simon says" */
    uint8_t waiting_for_player;    /**< 1 when waiting for player response */
    uint8_t result;                /**< 0 none, 1 correct, 2 wrong */
    uint32_t instruction_time;     /**< Tick when instruction was issued */
    uint32_t score;                /**< Player score */
} SimonSaysGame_t;

void SimonSays_Init(SimonSaysGame_t *game);
void SimonSays_Update(SimonSaysGame_t *game, spe_pp_outBuffer_t *keypoints);
void SimonSays_Render(SimonSaysGame_t *game);

#ifdef __cplusplus
}
#endif

#endif /* SIMON_SAYS_GAME_H */
