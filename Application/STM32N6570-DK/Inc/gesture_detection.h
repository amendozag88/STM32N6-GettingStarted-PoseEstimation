
#ifndef GESTURE_DETECTION_H
#define GESTURE_DETECTION_H

#include <stdint.h>
#include "app_config.h"
#include "display_spe.h"

// Keypoint indices for MoveNet (adjust based on your model)
#define KEYPOINT_NOSE           0
#define KEYPOINT_LEFT_SHOULDER  1
#define KEYPOINT_RIGHT_SHOULDER 2
#define KEYPOINT_LEFT_ELBOW     3
#define KEYPOINT_RIGHT_ELBOW    4
#define KEYPOINT_LEFT_WRIST     5
#define KEYPOINT_RIGHT_WRIST    6
#define KEYPOINT_LEFT_HIP       7
#define KEYPOINT_RIGHT_HIP      8
#define KEYPOINT_LEFT_KNEE      9
#define KEYPOINT_RIGHT_KNEE     10
#define KEYPOINT_LEFT_ANKLE     11
#define KEYPOINT_RIGHT_ANKLE    12

// Gesture detection parameters
#define GESTURE_HISTORY_SIZE    10
#define MIN_CONFIDENCE          0.5f
#define SWIPE_MIN_DISTANCE      0.3f   // Minimum distance for swipe (normalized)
#define SWIPE_MIN_SPEED         0.05f  // Minimum speed for swipe
#define SWIPE_MAX_FRAMES        8      // Maximum frames for swipe gesture
#define GESTURE_DISPLAY_TIME    2000   // How long to display gesture (ms)

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_RIGHT_ARM_SWIPE_LEFT,
    GESTURE_RIGHT_ARM_SWIPE_RIGHT,
    GESTURE_LEFT_ARM_SWIPE_LEFT,
    GESTURE_LEFT_ARM_SWIPE_RIGHT,
    GESTURE_BOTH_ARMS_RAISED,
    GESTURE_SWORD_OVERHEAD_STRIKE,
    GESTURE_SWORD_SIDE_SLASH
} GestureType_t;

typedef struct {
    float32_t x;
    float32_t y;
    float32_t confidence;
    uint32_t timestamp;
} KeypointHistory_t;

typedef struct {
    KeypointHistory_t history[AI_POSE_PP_POSE_KEYPOINTS_NB][GESTURE_HISTORY_SIZE];
    uint8_t history_index;
    GestureType_t last_detected_gesture;
    uint32_t last_gesture_time;
    GestureType_t current_display_gesture;  // For visual feedback
    uint32_t gesture_display_timeout;       // How long to show gesture
} GestureDetector_t;

// Function prototypes
void Gesture_Init(GestureDetector_t *detector);
GestureType_t Gesture_Detect(GestureDetector_t *detector, spe_pp_outBuffer_t *keypoints);
const char* Gesture_GetName(GestureType_t gesture);
float32_t Gesture_CalculateDistance(float32_t x1, float32_t y1, float32_t x2, float32_t y2);
float32_t Gesture_CalculateSpeed(KeypointHistory_t *history, uint8_t current_idx, uint8_t frames_back);

#endif /* GESTURE_DETECTION_H */
