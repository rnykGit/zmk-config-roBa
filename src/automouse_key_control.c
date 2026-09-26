/*
 * Custom sticky-layer control for roBa.
 *
 * Auto mouse layer (CONFIG_ROBA_AUTOMOUSE_LAYER):
 * The PMW3610 driver's own automouse-layer feature is left disabled (see the
 * comment on &trackball in config/roBa.keymap) so this file has full control:
 * - Activated only after CONFIG_ROBA_AUTOMOUSE_ACTIVATION_DELAY_MS of
 *   continuous trackball movement, to avoid accidental activation from brief
 *   bumps.
 * - While active, any further trackball movement, or pressing a key bound on
 *   the mouse layer, restarts the exit timeout (CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS).
 * - Pressing any other key exits the layer immediately.
 *
 * Scroll layer (CONFIG_ROBA_SCROLL_LAYER):
 * Activated elsewhere (the &lt_sticky hold-tap in the keymap). Once active,
 * it has no timeout of its own: it simply stays active until a key not bound
 * on that layer is pressed, at which point it is deactivated here.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#define AUTOMOUSE_LAYER CONFIG_ROBA_AUTOMOUSE_LAYER
#define SCROLL_LAYER CONFIG_ROBA_SCROLL_LAYER

/* Any gap longer than this between trackball movement events is treated as
 * the ball having stopped, so a fresh activation-delay streak must build up
 * again rather than counting the stopped time as movement. */
#define MOVEMENT_GAP_RESET_MS 100

static bool automouse_active;
static int64_t movement_streak_start;
static int64_t last_movement_time;

static void automouse_timeout_handler(struct k_work *work) {
    automouse_active = false;
    zmk_keymap_layer_deactivate(AUTOMOUSE_LAYER);
}

static K_WORK_DELAYABLE_DEFINE(automouse_timeout_work, automouse_timeout_handler);

static void extend_automouse_timeout(void) {
    k_work_reschedule(&automouse_timeout_work, K_MSEC(CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS));
}

static void trackball_input_callback(struct input_event *evt) {
    if (evt->type != INPUT_EV_REL || (evt->code != INPUT_REL_X && evt->code != INPUT_REL_Y)) {
        return;
    }

    int64_t now = k_uptime_get();

    if (automouse_active) {
        extend_automouse_timeout();
        return;
    }

    if (movement_streak_start == 0 || now - last_movement_time > MOVEMENT_GAP_RESET_MS) {
        movement_streak_start = now;
    }
    last_movement_time = now;

    if (now - movement_streak_start >= CONFIG_ROBA_AUTOMOUSE_ACTIVATION_DELAY_MS) {
        automouse_active = true;
        zmk_keymap_layer_activate(AUTOMOUSE_LAYER);
        extend_automouse_timeout();
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(trackball)), trackball_input_callback);

static bool is_layer_bound_key(uint32_t layer, uint32_t position) {
    const struct zmk_behavior_binding *binding =
        zmk_keymap_get_layer_binding_at_idx(layer, position);
    if (binding == NULL || binding->behavior_dev == NULL) {
        return false;
    }

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    return dev != NULL && dev != DEVICE_DT_GET(DT_NODELABEL(trans));
}

static int automouse_key_control_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (automouse_active) {
        if (is_layer_bound_key(AUTOMOUSE_LAYER, ev->position)) {
            extend_automouse_timeout();
        } else if (ev->state) {
            k_work_cancel_delayable(&automouse_timeout_work);
            automouse_active = false;
            zmk_keymap_layer_deactivate(AUTOMOUSE_LAYER);
        }
    }

    if (ev->state && zmk_keymap_layer_active(SCROLL_LAYER) &&
        !is_layer_bound_key(SCROLL_LAYER, ev->position)) {
        zmk_keymap_layer_deactivate(SCROLL_LAYER);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(automouse_key_control, automouse_key_control_listener);
ZMK_SUBSCRIPTION(automouse_key_control, zmk_position_state_changed);
