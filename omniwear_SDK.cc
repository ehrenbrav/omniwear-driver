#define _USE_MATH_DEFINES

#include "omniwear_SDK.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "omniwear.h"           // HID interface to omniwear device

// Some convenience definitions.
#define PITCH 0
#define YAW 1
#define ROLL 2

#define set_vector(a,b,c,d) ((a)[0]=(b),(a)[1]=(c),(a)[2]=(d))
#define subtract_vec3(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define dot_product(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define vector_length(a) (sqrt((double)dot_product(a, a)))
#define normalize_vec3(v) {float length = (float) sqrt(dot_product((v),(v)));if (length) length = 1.0f / length;(v)[0] *= length;(v)[1] *= length;(v)[2] *= length;}

typedef struct matrix4x4_s {

  float m[4][4];
} matrix4x4_t;

struct omniwear_device_impl {
  HID::DeviceP device;
};


#define MAX_SCALED_INTENSITY 255 // Always 255.

// Size of the packet.
#define PACKET_SIZE 3

// Transform a vec3 by a 4x4 matrix.
static void transform_vec3(const matrix4x4_t *in, const float v[3], float out[3])
{
  out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + in->m[0][3];
  out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + in->m[1][3];
  out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + in->m[2][3];
}

// Create a matrix to rotate a vec3 through a given angle.
static void create_rotation_matrix(matrix4x4_t *out, double angle, double x, double y, double z)
{
  double len, c, s;

  len = x*x+y*y+z*z;
  if (len != 0.0f)
    len = 1.0f / sqrt(len);
  x *= len;
  y *= len;
  z *= len;

  angle *= (-M_PI / 180.0);
  c = cos(angle);
  s = sin(angle);
  out->m[0][0]=x * x + c * (1 - x * x);
  out->m[0][1]=x * y * (1 - c) + z * s;
  out->m[0][2]=z * x * (1 - c) - y * s;
  out->m[0][3]=0.0f;
  out->m[1][0]=x * y * (1 - c) - z * s;
  out->m[1][1]=y * y + c * (1 - y * y);
  out->m[1][2]=y * z * (1 - c) + x * s;
  out->m[1][3]=0.0f;
  out->m[2][0]=z * x * (1 - c) + y * s;
  out->m[2][1]=y * z * (1 - c) - x * s;
  out->m[2][2]=z * z + c * (1 - z * z);
  out->m[2][3]=0.0f;
  out->m[3][0]=0.0f;
  out->m[3][1]=0.0f;
  out->m[3][2]=0.0f;
  out->m[3][3]=1.0f;
}

// Calculate the cannonical angle vectors.
static void get_angle_vectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
  double angle, sr, sp, sy, cr, cp, cy;

  angle = angles[YAW] * (M_PI*2 / 360);
  sy = sin(angle);
  cy = cos(angle);
  angle = angles[PITCH] * (M_PI*2 / 360);
  sp = sin(angle);
  cp = cos(angle);
  if (forward)
    {
      forward[0] = cp*cy;
      forward[1] = cp*sy;
      forward[2] = -sp;
    }
  if (right || up)
    {
      if (angles[ROLL])
        {
          angle = angles[ROLL] * (M_PI*2 / 360);
          sr = sin(angle);
          cr = cos(angle);
          if (right)
            {
              right[0] = -1*(sr*sp*cy+cr*-sy);
              right[1] = -1*(sr*sp*sy+cr*cy);
              right[2] = -1*(sr*cp);
            }
          if (up)
            {
              up[0] = (cr*sp*cy+-sr*-sy);
              up[1] = (cr*sp*sy+-sr*cy);
              up[2] = cr*cp;
            }
        }
      else
        {
          if (right)
            {
              right[0] = sy;
              right[1] = -cy;
              right[2] = 0;
            }
          if (up)
            {
              up[0] = (sp*cy);
              up[1] = (sp*sy);
              up[2] = cp;
            }
        }
    }
}

static void initialize_haptic_motors(haptic_device_state_t *state) {

  int i;

  // The motors are situated on a unit sphere around the player.
  // We're looking down at the player's head, with his nose at (0, 1, 0) or LOW_N.
  set_vector(state->motors[LOW_N].position, 1, 0, 0);
  set_vector(state->motors[LOW_S].position, -1, 0, 0);
  set_vector(state->motors[LOW_W].position, 0, 1, 0);
  set_vector(state->motors[LOW_E].position, 0, -1, 0);
  set_vector(state->motors[LOW_NW].position, .7, .7, 0);
  set_vector(state->motors[LOW_NE].position, .7, -.7, 0);
  set_vector(state->motors[LOW_SE].position, -.7, -.7, 0);
  set_vector(state->motors[LOW_SW].position, -.7, .7, 0);
  set_vector(state->motors[MID_N].position, .7, 0, .7);
  set_vector(state->motors[MID_S].position, -.7, 0, .7);
  set_vector(state->motors[MID_W].position, 0, .7, .7);
  set_vector(state->motors[MID_E].position, 0, -.7, .7);
  set_vector(state->motors[TOP].position, 0, 0, 1);

  for (i = 0; i<NUMBER_OF_MOTORS; i++) {state->motors[i].is_running = false;}

  // Set global intensity to 0.
  state->current_global_intensity = 0;
  state->global_intensity_ceiling = 0;
}

static void calculate_range_and_bearing(haptic_device_state_t *state) {

  int i;
  for (i = 0; i<state->haptic_target_list_len; i++) {

    haptic_target_t *target = &state->haptic_target_list[i];

    // Calculate the vector to this target.
    vec3_t vec_to_target;
    subtract_vec3(target->location, state->player_origin, vec_to_target);

    // Range.
    target->range = vector_length(vec_to_target);

    // Normalize and save.
    normalize_vec3(vec_to_target);
    set_vector(target->vec_to_target, vec_to_target[0], vec_to_target[1], vec_to_target[2]);

    // See what haptic effect goes with this target.
    int j;
    haptic_effect_t haptic_effect;
    haptic_effect_map_t *map;
    for (j = 0; j<state->haptic_effect_maps_len; j++) {

      map = &state->haptic_effect_maps[j];
      if (map->target_type == target->type) {
        haptic_effect = map->haptic_effect;
        break;
      }
    }

    // Set the appropriate haptic period based on the effect.
    float angle;
    vec3_t int_viewangles;
    vec3_t forward, right, up;
    switch(haptic_effect) {

    case BUZZ_CONTINUOUSLY:
      target->turn_motor_on = true;
      break;

    case BUZZ_ONCE_FOR_PERIOD:
      target->period = map->period;
      target->is_locked_for_period = true;
      break;

    case BUZZ_IF_TARGET_IS_LOOKING_AT_PLAYER:

      // Calculate the direction the target is looking.
      set_vector(int_viewangles, (int)target->viewangles_deg[0], (int)target->viewangles_deg[1], (int)target->viewangles_deg[2]);
      get_angle_vectors(int_viewangles, forward, right, up);

      // If vec_to_target and target's viewangles are close to anti-parallel, buzz.
      angle = acosf(dot_product(vec_to_target, forward));

      if ((angle < M_PI - LOOK_ANGLE_LIMIT)) {
        target->turn_motor_on = false;
      } else {
        target->turn_motor_on = true;
      }
      break;

    case PULSE_BY_RANGE:
      target->period = (float)MIN_PERIOD + (float)MAX_PERIOD * ((float) target->range/MAX_RANGE);
      break;

    case PULSE_BY_PERIOD:
      target->period = map->period;
      break;

    case NOTHING:
      printf("WARNING in calculate_range_and_bearing: haptic_effect NOTHING reached.\n");
      return;

    default:
      printf("WARNING in calculate_range_and_bearing: unrecognized haptic_effect.\n");
      return;
    }

    // Toggle if needed.
    // The reason we put this here rather than for a specific motor is so that the pulsing
    // will remain with the target as it shifts from motor to motor.
    if ((haptic_effect == PULSE_BY_PERIOD)
        || (haptic_effect == BUZZ_ONCE_FOR_PERIOD)
        || (haptic_effect == PULSE_BY_RANGE)) {

      // See if we're past the toggle period.
      if (state->last_update - target->last_toggle > target->period) {

        target->turn_motor_on = !target->turn_motor_on;
        target->last_toggle = state->last_update;
      }
    }
  }
}

// Comparison function for two target ranges.
static int cmp_range(const void *t1, const void *t2) {

  haptic_target_t *struct_t1, *struct_t2;

  struct_t1 = (haptic_target_t *)t1;
  struct_t2 = (haptic_target_t *)t2;

  if (struct_t1->range >= struct_t2->range) return 1;
  return -1;
}

void open_omniwear_device(haptic_device_state_t *state) {

  // Error check.
  if (!state) {
    printf("ERROR in open_omniwear_device: null pointer for state.\n");
    return;
  }

  // If no device is configured, try to open.
  if (!state->device_impl) {

    state->device_impl = new omniwear_device_impl;
    auto& impl = *state->device_impl;

    impl.device = Omniwear::open ();

    if (impl.device) {
      printf("ERROR in open_omniwear_device: could not open haptic device.\n");
      state->device_impl = nullptr;
      return;
    }

    state->last_update = 0;

    // Initialize motors.
    initialize_haptic_motors(state);
    reset_omniwear_device(state);
  }
}

void close_omniwear_device(haptic_device_state_t *state) {

  // Error check.
  if (!state) {
    printf("ERROR in close_omniwear_device: null pointer for state.\n");
    return;
  }

  reset_omniwear_device(state);

  state->device_impl = nullptr;
}

void command_haptic_motor (haptic_device_state_t* state, int motor, int duty)
{
  if (!state) {
    printf ("***ERR: invalid state\n");
    return;
  }

  if (motor < 0 || motor > C_MOTORS) {
    printf ("***ERR: motor number must be from 0 to %d\n", C_MOTORS - 1);
    return;
  }

  if (duty < 0 || duty > 100) {
    printf ("***ERR: intensity must be from 0 to 100%%\n");
    return;
  }

  if (state->haptic_volume == 0)
    printf("WARNING: in command_haptic_motor: haptic_volume set to 0.\n");

  // Adjust for the global haptic volume.
  duty = (duty*state->haptic_volume + 50)/100;

  Omniwear::configure_motor (state->device_impl->device.get (), motor, duty);
}

void reset_omniwear_device(haptic_device_state_t *state)
{

  if (!state) {
    printf ("***ERR: invalid state\n");
    return;
  }

  // Housekeeping.
  state->current_global_intensity = 0;
  state->global_intensity_ceiling = 0;
  state->haptic_target_list_len = 0;

  Omniwear::reset_motors (state->device_impl->device.get ());

  // Turn off motors.
  for (auto i = 0; i < C_MOTORS; ++i)
    state->motors[i].is_running = false;
}

void do_throb(haptic_device_state_t *state, unsigned int intensity_ceiling, float throb_period_sec, double game_time) {

  // Error check
  if (intensity_ceiling > 100) {

    printf("ERROR in do_throb: intensity_ceiling is not 0-100.\n");
    return;
  }

  if (throb_period_sec < 0) {

    printf("ERROR in do_throb: period is less than zero.\n");
    return;
  }

  if (!state) {
    printf("ERROR in do_throb: null pointer for state.\n");
    return;
  }

  // Set amplitude of throb to max out at the intensity_ceiling.
  state->global_intensity_ceiling = intensity_ceiling;

  // Clamp the current global intensity to the (maybe new) ceiling and floor.
  // Toggle direction if necessary.
  if (state->current_global_intensity > state->global_intensity_ceiling) {

    state->current_global_intensity = state->global_intensity_ceiling;
    state->global_intensity_is_rising = false;
  } else if (state->current_global_intensity <= 0) {

    state->current_global_intensity = 0;
    state->global_intensity_is_rising = true;
  }

  // Increment.
  double time_from_last_update = game_time - state->last_update;

  // Clamp to sane values.
  if (time_from_last_update > 5) time_from_last_update = 5;

  double steps = throb_period_sec / time_from_last_update;
  double increment = (state->global_intensity_ceiling + 1) / steps; // Adding one so it actually starts.

  // Clamp.
  if (increment < 0) increment = 0;
  if (increment > 100) increment = 100;

  // Set.
  if (state->global_intensity_is_rising) state->current_global_intensity += increment;
  else state->current_global_intensity -= increment;
}

void stop_throbbing(haptic_device_state_t *state) {

  // Error check.
  if (!state) {
    printf("ERROR in stop_throbbing: null pointer for state.\n");
    return;
  }

  // Reset everything.
  state->global_intensity_ceiling = 0;
  state->current_global_intensity = 0;
}

void update_haptic_radar(haptic_device_state_t *state, haptic_target_t updated_targets[], int updated_targets_len, vec3_t player_origin, vec3_t player_viewangles_deg) {

  // Error check.
  if (!state) {
    printf("ERROR in update_haptic_radar: null pointer for state.\n");
    return;
  }

  if (!updated_targets) {
    printf("ERROR in update_haptic_radar: null pointer for updated_targets.\n");
    return;
  }

  if (updated_targets_len > MAX_TARGETS) {
    printf("ERROR in update_haptic_radar: number of targets passed to this function exceeds MAX_TARGETS.\n");
    return;
  }

  // TODO - error check position vectors?

  // Update the player origin.
  set_vector(state->player_origin, player_origin[0], player_origin[1], player_origin[2]);

  // Update the viewangles.
  set_vector(state->player_viewangles_deg, player_viewangles_deg[0], player_viewangles_deg[1], player_viewangles_deg[2]);

  // Loop through the list of updated targets we were handed.
  int i, j;
  for (i = 0; i<updated_targets_len; i++) {

    haptic_target_t *updated_target = &updated_targets[i];

    bool target_updated = false;
    haptic_target_t *existing_target;
    for (j = 0; j<state->haptic_target_list_len; j++) {

      existing_target = &state->haptic_target_list[j];

      // If the updated target is already in our list, update its data.
      if (existing_target->index == updated_target->index) {

        set_vector(existing_target->location, updated_target->location[0], updated_target->location[1], updated_target->location[2]);
        set_vector(existing_target->viewangles_deg, updated_target->viewangles_deg[0], updated_target->viewangles_deg[1], updated_target->viewangles_deg[2]);
        existing_target->healthvalue = updated_target->healthvalue;
        target_updated = true;
        break;
      }
    }

    // If we didn't find the updated target in our existing list, add.
    if (!target_updated) {

      // Allocate.
      haptic_target_t *new_target = &state->haptic_target_list[state->haptic_target_list_len];

      // Initialize.
      memset(new_target, 0, sizeof(*new_target));

      // Record.
      new_target->type = updated_target->type;
      new_target->index = updated_target->index;
      set_vector(new_target->location, updated_target->location[0], updated_target->location[1], updated_target->location[2]);
      set_vector(new_target->viewangles_deg, updated_target->viewangles_deg[0], updated_target->viewangles_deg[1], updated_target->viewangles_deg[2]);
      new_target->healthvalue = updated_target->healthvalue;
      new_target->last_toggle = 0;

      // Increment list length.
      state->haptic_target_list_len++;
    }
  }

  // Finally, prune the list of unneeded targets.
  for (j = 0; j<state->haptic_target_list_len;) {

    haptic_target_t *existing_target = &state->haptic_target_list[j];
    bool index_found = false;

    for (i = 0; i<updated_targets_len; i++) {

      haptic_target_t *updated_target = &updated_targets[i];

      if (existing_target->index == updated_target->index) {
        index_found = true;
        break;
      }
    }

    // This target was added/updated this frame...continue.
    if (index_found) {
      j++;
      continue;
    }

    // If this is a locked target whose period not yet expired, do not clear.
    if (existing_target->is_locked_for_period &&
        state->last_update - existing_target->last_toggle < existing_target->period) {
      j++;
      continue;
    }

    // No update for this target...shift everything left.
    for (i = j; i<state->haptic_target_list_len - 1; i++) {

      haptic_target_t *this_target = &state->haptic_target_list[i];
      haptic_target_t *next_target = &state->haptic_target_list[i+1];

      // Copy the next target into the current one.
      memcpy(this_target, next_target, sizeof(*this_target));
    }

    // Clear the last slot.
    memset(&state->haptic_target_list[state->haptic_target_list_len - 1], 0, sizeof(*existing_target));

    // Decrement the list length.
    state->haptic_target_list_len--;
  }

  // Now that we've updated the target list, calculate ranges and bearings.
  calculate_range_and_bearing(state);

  // Sort the list by range.
  qsort(state->haptic_target_list, state->haptic_target_list_len, sizeof(state->haptic_target_list[0]), cmp_range);
}

void stop_haptic_radar(haptic_device_state_t *state) {

  // Error check.
  if (!state) {

    printf("ERROR in stop_haptic_radar: state pointer is null.\n");
    return;
  }

  memset(state->haptic_target_list, 0, sizeof(*state->haptic_target_list));
  state->haptic_target_list_len = 0;
}

void set_haptic_effect(haptic_device_state_t *state, int target_type, haptic_effect_t haptic_effect, float period) {

  // Sanity check.
  if (!state) {
    printf("ERROR in set_haptic_effect: state pointer is null.\n");
    return;
  }

  if ((haptic_effect == PULSE_BY_PERIOD) || (haptic_effect == BUZZ_ONCE_FOR_PERIOD)) {

    if (period <= 0) {

      printf("ERROR in set_haptic_effect: insane value for period: %f.\n", period);
      return;
    }
  }

  // See if there's any entry for this target type already.
  bool updated_existing_entry = false;
  int i;
  for (i = 0; i<state->haptic_effect_maps_len; i++) {

    haptic_effect_map_t *existing_haptic_effect_map = &state->haptic_effect_maps[i];
    if (existing_haptic_effect_map->target_type == target_type) {

      // Update.
      existing_haptic_effect_map->haptic_effect = haptic_effect;
      existing_haptic_effect_map->period = period;
      updated_existing_entry = true;
      break;
    }
  }

  // Add.
  if (!updated_existing_entry) {

    haptic_effect_map_t *new_haptic_effect_map = &state->haptic_effect_maps[state->haptic_effect_maps_len];
    new_haptic_effect_map->target_type = target_type;
    new_haptic_effect_map->haptic_effect = haptic_effect;
    new_haptic_effect_map->period = period;
    state->haptic_effect_maps_len++;
  }
}

// Remove a haptic effect map.
void clear_haptic_effect(haptic_device_state_t *state, int target_type) {

  int i, j;
  // Locate this haptic effect map.
  for (j = 0; j<state->haptic_effect_maps_len; j++) {

    haptic_effect_map_t *existing_haptic_effect_map = &state->haptic_effect_maps[j];
    if (existing_haptic_effect_map->target_type == target_type) {

      // Shift everything left by one.
      for (i = j; i<state->haptic_effect_maps_len - 1; i++) {

        haptic_effect_map_t *this_map = &state->haptic_effect_maps[i];

        haptic_effect_map_t *next_map = &state->haptic_effect_maps[i+1];

        // Copy the next map into the current one.
        memcpy(this_map, next_map, sizeof(*this_map));
      }

      // Clear the last slot.
      memset(&state->haptic_effect_maps[state->haptic_effect_maps_len - 1], 0, sizeof(*existing_haptic_effect_map));

      // Decrement the list length.
      state->haptic_effect_maps_len--;
      break;
    }
  }
}

void execute_haptic_effects(haptic_device_state_t *state, double game_time) {

  // Update our clock.
  state->last_update = game_time;

  // Convert to int viewangles.
  vec3_t int_viewangles;
  set_vector(int_viewangles, (int)state->player_viewangles_deg[0], (int)state->player_viewangles_deg[1], (int)state->player_viewangles_deg[2]);

  // Calculate the relevant vectors from the player's perspective.
  vec3_t forward, right, up;
  get_angle_vectors(int_viewangles, forward, right, up);

  // Loop through the actuators.
  int motor_num;
  for (motor_num = 0; motor_num<NUMBER_OF_MOTORS; motor_num++) {

    haptic_motor_t *motor = &state->motors[motor_num];

    // Calculate this motor's position in the game.
    vec3_t motor_vec;
    set_vector(motor_vec, motor->position[0], motor->position[1], motor->position[2]);

    // Rotate it by yaw angle about the z-axis.
    matrix4x4_t rotate_matrix;
    create_rotation_matrix(&rotate_matrix, int_viewangles[YAW], up[0], up[1], up[2]);
    vec3_t temp_vec;
    transform_vec3(&rotate_matrix, motor_vec, temp_vec);

    // Now rotate it by the pitch.
    create_rotation_matrix(&rotate_matrix, int_viewangles[PITCH], right[0], right[1], right[2]);
    transform_vec3(&rotate_matrix, temp_vec, motor_vec);

    // Normalize (just in case).
    normalize_vec3(motor_vec);

    // Get the closest target that is also within this motor's MAX_ANGLE.
    bool tracking_target = false;
    int target_num;
    unsigned char intensity;
    for (target_num = 0; target_num<state->haptic_target_list_len; target_num++) {

      haptic_target_t *target = &state->haptic_target_list[target_num];

      // If we're out of range, break (since this list is sorted by range).
      if (target->range >= MAX_RANGE) break;

      // Calculate the angle from the motor's normal to the target.
      float angle = acosf(dot_product(target->vec_to_target, motor_vec));

      // If the angle is too big, try the next closest target.
      if (angle > MAX_ANGLE) continue;

      // Set the tracking target flag.
      tracking_target = true;

      // Scale this motor's intensity by the target's angle from the normal.
      intensity = 100 * (1 - (angle/MAX_ANGLE));

      // Set the motor.
      if (target->turn_motor_on) {

        command_haptic_motor(state, motor_num, intensity);
        motor->is_running = true;
      } else {

        command_haptic_motor(state, motor_num, 0);
        motor->is_running = false;
      }

      // Now move on to next motor since this motor is tracking a target.
      break;
    }

    // If there's no eligible target, turn off motor or else handle global intensities.
    if (!tracking_target) {

      // Handle global intensities.
      intensity = state->current_global_intensity;

      // Handle zero global intensity.
      if (intensity == 0) motor->is_running = false;

      // Set the motor.
      command_haptic_motor(state, motor_num, intensity);
    }
  }
}
