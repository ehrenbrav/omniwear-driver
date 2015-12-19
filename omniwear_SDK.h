#ifndef HAPTIC_DRIVER_H
#define HAPTIC_DRIVER_H


#if defined (_WIN32) || defined (__CYGWIN__)
#include "stdbool.h"
#if defined (OMNIWEAR_BUILD)
#define DLL_EXPORT __declspec(dllexport) __cdecl
#else
#define DLL_EXPORT __declspec(dllimport) __cdecl
#endif
#else
#include <stdbool.h>
#define DLL_EXPORT
#endif

#include <stdint.h>

/////////////////////////////////
// USER-CONFIGURABLE SETTINGS
/////////////////////////////////

// The maximum range the haptic radar works to.
// The intensity of the PULSE_BY_RANGE effect
// is scaled by this.
#define MAX_RANGE 5000

// The minimum distance the haptic radar works to.
// The intensity of the PULSE_BY_RANGE effect
// is scaled by this. Can be set to avoid detecting
// yourself.
#define MIN_DISTANCE 100

// The max angle the haptic radar looks away from
// the normal of each haptic actuator. In radians.
#define MAX_ANGLE 1.0f

// Offset from PI to tell if a target is looking at us. In radians.
#define LOOK_ANGLE_LIMIT .5

// The shortest and longest period for pulsing haptic effects.
// The BUZZ_BY_RANGE effect's pulse frequency is scaled
// by this. The short end might end up to physical limiations
// of the haptic actuators. In seconds.
#define MIN_PERIOD .1
#define MAX_PERIOD 2

// How many targets we can track at once.
#define MAX_TARGETS 64

/////////////////////////////////
// SETTINGS YOU PROBABLY SHOULDN'T MESS WITH
/////////////////////////////////

// These define the individual haptic actuators.
#define LOW_N  0
#define LOW_S  1
#define LOW_E  2
#define LOW_W  3
#define LOW_NE 4
#define LOW_NW 5
#define LOW_SE 6
#define LOW_SW 7
#define MID_N  8
#define MID_S  9
#define MID_E  10
#define MID_W  11
#define TOP    12

// Number of motors in the cap.
#define NUMBER_OF_MOTORS 13
#define C_MOTORS (NUMBER_OF_MOTORS)

/* Error result codes for SDK methods */
enum OMNI_RESULT {
  OMNI_SUCCESS                      = 0,
  OMNI_ERROR_NULL_STATE             = 1, /* NULL Omniwear SDK struct pointer */
  OMNI_ERROR_OPENING_DEVICE         = 2,
  OMNI_ERROR_INVALID_MOTOR          = 3,
  OMNI_ERROR_INTENSITY_OUT_OF_RANGE = 4,
  OMNI_ERROR_INVALID_PACKING	    = 5,
};

// Some convenience definitions.
typedef float vec3_t[3];

// This holds the info on a particular target to be tracked.
typedef struct haptic_target_s {

  int type; // Developer-defined categories of target.
  vec3_t location; // Location of this target.
  vec3_t viewangles_deg; // Viewangles of the target.
  vec3_t vec_to_target; // Vector from the player to the target.
  int healthvalue; // Healthvalue of the target, if applicable.
  int index; // Developer-defined value that references this target.
  double last_toggle;
  float period; // Toggle period, if applicable.
  int range; // Range from player to this target.
  bool turn_motor_on;
  bool is_locked_for_period; // Keep this target around until the period has expired.

} haptic_target_t;

// Types of haptic effects we can create.
typedef enum haptic_effect_e {

  NOTHING,
  BUZZ_CONTINUOUSLY,
  BUZZ_ONCE_FOR_PERIOD,
  BUZZ_IF_TARGET_IS_LOOKING_AT_PLAYER,
  PULSE_BY_RANGE,
  PULSE_BY_PERIOD

} haptic_effect_t;

// Struct to associate a given haptic effect with a type of target.
typedef struct haptic_effect_map_s {

  int target_type;
  haptic_effect_t haptic_effect;
  float period;

} haptic_effect_map_t;

// Struct for each actuator.
typedef struct haptic_motor_s {
  vec3_t position; // Position of the actuator on the cap.
  bool is_running;
} haptic_motor_t;

// Defines the state of the device.
struct omniwear_device_impl;

typedef struct haptic_device_state_s {

  // Global "volume" knob for haptic intensity. 0-100.
  int haptic_volume;

  // Location and orientation of the player/wearer of the device.
  vec3_t player_origin;
  vec3_t player_viewangles_deg;

  // List of targets we're tracking.
  haptic_target_t haptic_target_list[MAX_TARGETS];
  int haptic_target_list_len;

  // Time of the last execute_haptic_effects frame.
  double last_update;

  // Map of target type to haptic action.
  haptic_effect_map_t haptic_effect_maps[MAX_TARGETS];
  int haptic_effect_maps_len;

  // Handle for the haptic device.
  struct omniwear_device_impl* device_impl;

  // Structures for the actual actuators.
  haptic_motor_t motors[NUMBER_OF_MOTORS];

  // Global haptic effects.
  float current_global_intensity; // 0 - 100.
  int global_intensity_ceiling; // 0 - 100.
  bool global_intensity_is_rising;

} haptic_device_state_t;

typedef struct {
  int motor;			// Motor index 0..(C_MOTORS - 1)
  int intensity;                // Intensity 0..100
} haptic_motor_config_t;

/////////////////////////////////
// FUNCTION PROTOTYPES
/////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Set a given motor to a given intensity (0-100).
OMNI_RESULT DLL_EXPORT command_haptic_motor (haptic_device_state_t *state,
                                             int motor_number, int intensity);

// Set intensity for a set of motors
OMNI_RESULT DLL_EXPORT command_haptic_motors (haptic_device_state_t *state,
                                              haptic_motor_config_t* configs,
                                              int config_count);

// Must be called before the device can be used.
OMNI_RESULT DLL_EXPORT open_omniwear_device(haptic_device_state_t *state);

// Called at shutdown.
OMNI_RESULT DLL_EXPORT close_omniwear_device(haptic_device_state_t *state);

// Turn off all actuators and reset everything.
OMNI_RESULT DLL_EXPORT reset_omniwear_device(haptic_device_state_t *state);

// Define arbitrary mapping between packed intensity mappings and
// motor drive intensities.  Count must be 16.  The duties are motor
// drive intensities from 0 to 255.  The index of the array is the
// packed code.
OMNI_RESULT DLL_EXPORT define_packed_mapping(haptic_device_state_t* state,
                                             const uint8_t* duties, int count);

// Define a simple linear mapping between packed intensity mappings
// and motor drive intensities.  The numerator/denominator is the
// slope of the interpolation.  The mapping is from packed codes 0-15
// to intensities from 0-255.  A typical mapping might be (127, 15,
// 128) to start with code 1 as 136 and code 15 as 255. Code 0 is
// always 0.
OMNI_RESULT DLL_EXPORT define_linear_packed_mapping(haptic_device_state_t*
                                                    state,
                                                    int numerator,
                                                    int denominator,
                                                    int intercept);

// Set motor drive intensities using a packed mapping.  Intensities
// are defined for all motors from 0 to count.  The intensity values
// must be between 0 and 100.
OMNI_RESULT DLL_EXPORT command_haptic_motors_packed(haptic_device_state_t*
                                                    state,
                                                    int* intensities,
                                                    int count);

// Throb the entire cap.
// Intensity and increment are 0-100.
void DLL_EXPORT do_throb(haptic_device_state_t *state,
                         unsigned int intensity_ceiling,
                         float throb_period_sec, double game_time);

// Should be called after you're done throbbing. Otherwise, the actuators
// will be stuck in the last throb state.
void DLL_EXPORT stop_throbbing(haptic_device_state_t *state);

// Update the list of targets we're tracking for the haptic radar.
void DLL_EXPORT update_haptic_radar(haptic_device_state_t *state, haptic_target_t updated_targets[], int updated_targets_len, vec3_t player_origin, vec3_t player_viewangles);

// Turn off haptic radar.
void DLL_EXPORT stop_haptic_radar(haptic_device_state_t *state);

// Set haptic radar effect. This configures a type of haptic effect
// to be associated with a specified target type.
// period is only read if haptic_effect is a periodic one (ie - PULSE_BY_RANGE)
// TODO - implement priority ranking of types of target?
void DLL_EXPORT set_haptic_effect(haptic_device_state_t *state, int target_type, haptic_effect_t haptic_effect, float period);

// Removes a haptic effect.
void DLL_EXPORT clear_haptic_effect(haptic_device_state_t *state, int target_type);

// Called each frame to actually implment the haptic effects.
void DLL_EXPORT execute_haptic_effects(haptic_device_state_t *state, double game_time);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HAPTIC_DRIVER_H
