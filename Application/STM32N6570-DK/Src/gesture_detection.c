/**
 ******************************************************************************
 * @file    gesture_detection.c
 * @author  Your Name
 * @brief   Gesture detection implementation
 ******************************************************************************
 */

#include "gesture_detection.h"
#include "main.h"
#include "display_spe.h"
#include <math.h>
#include <string.h>


void Gesture_Init(GestureDetector_t *detector)
{
    memset(detector, 0, sizeof(GestureDetector_t));
    detector->last_detected_gesture = GESTURE_NONE;
    detector->current_display_gesture = GESTURE_NONE;
    detector->gesture_display_timeout = 0;
}

float32_t Gesture_CalculateDistance(float32_t x1, float32_t y1, float32_t x2, float32_t y2)
{
    float32_t dx = x2 - x1;
    float32_t dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

float32_t Gesture_CalculateSpeed(KeypointHistory_t *history, uint8_t current_idx, uint8_t frames_back)
{
    if (frames_back >= GESTURE_HISTORY_SIZE) return 0.0f;

    uint8_t prev_idx = (current_idx - frames_back + GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;

    float32_t distance = Gesture_CalculateDistance(
        history[prev_idx].x, history[prev_idx].y,
        history[current_idx].x, history[current_idx].y
    );

    uint32_t time_diff = history[current_idx].timestamp - history[prev_idx].timestamp;
    if (time_diff == 0) return 0.0f;

    return distance / (time_diff / 1000.0f); // Speed per second
}

static GestureType_t Detect_ArmSwipe(GestureDetector_t *detector, spe_pp_outBuffer_t *keypoints)
{
    uint8_t curr_idx = detector->history_index;
    uint8_t prev_idx = (curr_idx - 1 + GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;

    // Check right arm swipe
    KeypointHistory_t *right_wrist = detector->history[KEYPOINT_RIGHT_WRIST];
    KeypointHistory_t *right_shoulder = detector->history[KEYPOINT_RIGHT_SHOULDER];

    if (right_wrist[curr_idx].confidence > MIN_CONFIDENCE &&
        right_shoulder[curr_idx].confidence > MIN_CONFIDENCE) {

        // Calculate horizontal movement of wrist
        float32_t wrist_dx = right_wrist[curr_idx].x - right_wrist[prev_idx].x;
        float32_t wrist_speed = Gesture_CalculateSpeed(right_wrist, curr_idx, 3);

        // Check if wrist is moving significantly horizontally
        if (fabsf(wrist_dx) > 0.05f && wrist_speed > SWIPE_MIN_SPEED) {
            // Check if arm is extended (wrist far from shoulder)
            float32_t arm_extension = Gesture_CalculateDistance(
                right_wrist[curr_idx].x, right_wrist[curr_idx].y,
                right_shoulder[curr_idx].x, right_shoulder[curr_idx].y
            );

            if (arm_extension > 0.2f) { // Arm is extended
                if (wrist_dx > 0) {
                    return GESTURE_RIGHT_ARM_SWIPE_RIGHT;
                } else {
                    return GESTURE_RIGHT_ARM_SWIPE_LEFT;
                }
            }
        }
    }

    // Check left arm swipe (similar logic)
    KeypointHistory_t *left_wrist = detector->history[KEYPOINT_LEFT_WRIST];
    KeypointHistory_t *left_shoulder = detector->history[KEYPOINT_LEFT_SHOULDER];

    if (left_wrist[curr_idx].confidence > MIN_CONFIDENCE &&
        left_shoulder[curr_idx].confidence > MIN_CONFIDENCE) {

        float32_t wrist_dx = left_wrist[curr_idx].x - left_wrist[prev_idx].x;
        float32_t wrist_speed = Gesture_CalculateSpeed(left_wrist, curr_idx, 3);

        if (fabsf(wrist_dx) > 0.05f && wrist_speed > SWIPE_MIN_SPEED) {
            float32_t arm_extension = Gesture_CalculateDistance(
                left_wrist[curr_idx].x, left_wrist[curr_idx].y,
                left_shoulder[curr_idx].x, left_shoulder[curr_idx].y
            );

            if (arm_extension > 0.2f) {
                if (wrist_dx > 0) {
                    return GESTURE_LEFT_ARM_SWIPE_RIGHT;
                } else {
                    return GESTURE_LEFT_ARM_SWIPE_LEFT;
                }
            }
        }
    }

    return GESTURE_NONE;
}

static GestureType_t Detect_SwordGestures(GestureDetector_t *detector, spe_pp_outBuffer_t *keypoints)
{
    uint8_t curr_idx = detector->history_index;

    KeypointHistory_t *right_wrist = detector->history[KEYPOINT_RIGHT_WRIST];
    KeypointHistory_t *right_shoulder = detector->history[KEYPOINT_RIGHT_SHOULDER];
    KeypointHistory_t *right_elbow = detector->history[KEYPOINT_RIGHT_ELBOW];

    if (right_wrist[curr_idx].confidence < MIN_CONFIDENCE ||
        right_shoulder[curr_idx].confidence < MIN_CONFIDENCE ||
        right_elbow[curr_idx].confidence < MIN_CONFIDENCE) {
        return GESTURE_NONE;
    }

    // Overhead strike: Check if hand moves from high to low rapidly
    if (curr_idx >= 5) { // Need some history
        uint8_t start_idx = (curr_idx - 5 + GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;

        float32_t start_y = right_wrist[start_idx].y;
        float32_t curr_y = right_wrist[curr_idx].y;
        float32_t vertical_movement = curr_y - start_y;

        // Check if hand started high and moved down quickly
        if (start_y < 0.3f && vertical_movement > 0.25f) { // Started high, moved down
            float32_t speed = Gesture_CalculateSpeed(right_wrist, curr_idx, 5);
            if (speed > SWIPE_MIN_SPEED * 1.5f) {
                return GESTURE_SWORD_OVERHEAD_STRIKE;
            }
        }
    }

    // Side slash: Check for horizontal movement with extended arm
    if (curr_idx >= 3) {
        uint8_t start_idx = (curr_idx - 3 + GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;

        float32_t horizontal_movement = fabsf(right_wrist[curr_idx].x - right_wrist[start_idx].x);
        float32_t arm_extension = Gesture_CalculateDistance(
            right_wrist[curr_idx].x, right_wrist[curr_idx].y,
            right_shoulder[curr_idx].x, right_shoulder[curr_idx].y
        );

        if (horizontal_movement > 0.2f && arm_extension > 0.25f) {
            float32_t speed = Gesture_CalculateSpeed(right_wrist, curr_idx, 3);
            if (speed > SWIPE_MIN_SPEED) {
                return GESTURE_SWORD_SIDE_SLASH;
            }
        }
    }

    return GESTURE_NONE;
}

GestureType_t Gesture_Detect(GestureDetector_t *detector, spe_pp_outBuffer_t *keypoints)
{
    uint32_t current_time = HAL_GetTick();

    // Update history with current keypoints
    for (int i = 0; i < AI_POSE_PP_POSE_KEYPOINTS_NB; i++) {
        detector->history[i][detector->history_index].x = keypoints[i].x_center;
        detector->history[i][detector->history_index].y = keypoints[i].y_center;
        detector->history[i][detector->history_index].confidence = keypoints[i].proba;
        detector->history[i][detector->history_index].timestamp = current_time;
    }

    // Advance history index
    detector->history_index = (detector->history_index + 1) % GESTURE_HISTORY_SIZE;

    // Prevent detecting same gesture too quickly
    if (current_time - detector->last_gesture_time < 1000) { // 1 second cooldown
        return GESTURE_NONE;
    }

    // Try different gesture detection algorithms
    GestureType_t detected_gesture = GESTURE_NONE;

    // Check for arm swipes first
    detected_gesture = Detect_ArmSwipe(detector, keypoints);
    if (detected_gesture != GESTURE_NONE) {
        detector->last_detected_gesture = detected_gesture;
        detector->last_gesture_time = current_time;
        detector->current_display_gesture = detected_gesture;
        detector->gesture_display_timeout = current_time + GESTURE_DISPLAY_TIME;
        return detected_gesture;
    }

    // Check for sword gestures
    detected_gesture = Detect_SwordGestures(detector, keypoints);
    if (detected_gesture != GESTURE_NONE) {
        detector->last_detected_gesture = detected_gesture;
        detector->last_gesture_time = current_time;
        detector->current_display_gesture = detected_gesture;
        detector->gesture_display_timeout = current_time + GESTURE_DISPLAY_TIME;
        return detected_gesture;
    }

    return GESTURE_NONE;
}

const char* Gesture_GetName(GestureType_t gesture)
{
    switch (gesture) {
        case GESTURE_RIGHT_ARM_SWIPE_LEFT:   return "Right Arm Swipe Left";
        case GESTURE_RIGHT_ARM_SWIPE_RIGHT:  return "Right Arm Swipe Right";
        case GESTURE_LEFT_ARM_SWIPE_LEFT:    return "Left Arm Swipe Left";
        case GESTURE_LEFT_ARM_SWIPE_RIGHT:   return "Left Arm Swipe Right";
        case GESTURE_BOTH_ARMS_RAISED:       return "Both Arms Raised";
        case GESTURE_SWORD_OVERHEAD_STRIKE:  return "Sword Overhead Strike";
        case GESTURE_SWORD_SIDE_SLASH:       return "Sword Side Slash";
        default:                             return "No Gesture";
    }
}

GestureType_t Gesture_GetCurrentDisplayGesture(GestureDetector_t *detector)
{
    uint32_t current_time = HAL_GetTick();

    // Check if display timeout has expired
    if (current_time > detector->gesture_display_timeout) {
        detector->current_display_gesture = GESTURE_NONE;
    }

    return detector->current_display_gesture;
}

void Gesture_GetKeypointDebugInfo(GestureDetector_t *detector, uint8_t keypoint_idx,
                                  float32_t *current_x, float32_t *current_y,
                                  float32_t *current_confidence, float32_t *current_speed)
{
    if (keypoint_idx >= AI_POSE_PP_POSE_KEYPOINTS_NB) {
        *current_x = 0.0f;
        *current_y = 0.0f;
        *current_confidence = 0.0f;
        *current_speed = 0.0f;
        return;
    }

    // Get current index (last updated position)
    uint8_t curr_idx = (detector->history_index - 1 + GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;

    // Get current keypoint data
    *current_x = detector->history[keypoint_idx][curr_idx].x;
    *current_y = detector->history[keypoint_idx][curr_idx].y;
    *current_confidence = detector->history[keypoint_idx][curr_idx].confidence;

    // Calculate speed over last 3 frames
    *current_speed = Gesture_CalculateSpeed(detector->history[keypoint_idx], curr_idx, 3);
}
