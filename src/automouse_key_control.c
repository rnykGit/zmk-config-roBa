/*
 * Custom sticky-layer control for roBa.
 *
 * Auto mouse layer (CONFIG_ROBA_AUTOMOUSE_LAYER):
 * The PMW3610 driver's own automouse-layer feature is left disabled (see the
 * comment on &trackball in config/roBa.keymap) so this file has full control:
 * - Trackball movement must be continuous for at least
 *   CONFIG_ROBA_AUTOMOUSE_MIN_MOVEMENT_MS (to ignore tiny accidental bumps).
 *   Once that's satisfied, every further movement (re)starts a
 *   CONFIG_ROBA_AUTOMOUSE_ACTIVATION_DELAY_MS countdown; if it elapses with
 *   no key pressed in the meantime, the auto mouse layer is activated.
 * - Pressing any key before activation cancels the pending activation.
 * - There is no time-based exit: once active, the layer stays active until a
 *   key not bound on it is pressed, which deactivates it immediately.
 *
 * Scroll layer (CONFIG_ROBA_SCROLL_LAYER):
 * Activated/toggled elsewhere (the &lt_sticky hold-tap in the keymap). Once
 * active, it likewise has no timeout: it stays active until a key not bound
 * on that layer is pressed, at which point it is deactivated here.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#define AUTOMOUSE_LAYER CONFIG_ROBA_AUTOMOUSE_LAYER
#define SCROLL_LAYER CONFIG_ROBA_SCROLL_LAYER

/* Any gap longer than this between trackball movement events is treated as
 * the ball having stopped, so a fresh continuous-movement streak must build
 * up again rather than counting the stopped time as movement. */
#define MOVEMENT_GAP_RESET_MS 100

static bool automouse_active;
static int64_t movement_streak_start;
static int64_t last_movement_time;

static void automouse_activation_handler(struct k_work *work) {
    automouse_active = true;
    zmk_keymap_layer_activate(AUTOMOUSE_LAYER);
}

static K_WORK_DELAYABLE_DEFINE(automouse_activation_work, automouse_activation_handler);

static void trackball_input_callback(struct input_event *evt) {
    if (evt->type != INPUT_EV_REL || (evt->code != INPUT_REL_X && evt->code != INPUT_REL_Y)) {
        return;
    }

    if (automouse_active) {
        return;
    }

    int64_t now = k_uptime_get();

    if (movement_streak_start == 0 || now - last_movement_time > MOVEMENT_GAP_RESET_MS) {
        movement_streak_start = now;
    }
    last_movement_time = now;

    if (now - movement_streak_start < CONFIG_ROBA_AUTOMOUSE_MIN_MOVEMENT_MS) {
        /* Not yet confirmed as real (sustained) movement; don't arm the
         * settle countdown on tiny/accidental bumps. */
        return;
    }

    /* Movement is confirmed. (Re)start the settle countdown so it only fires
     * once movement has been still for the full delay. */
    k_work_reschedule(&automouse_activation_work,
                      K_MSEC(CONFIG_ROBA_AUTOMOUSE_ACTIVATION_DELAY_MS));
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

/* Physical key positions used by the "muhenkan" and "henkan" combos in
 * config/roBa.keymap, which hold &lt_sticky to enter/toggle the scroll
 * layer. On the scroll layer these positions are &trans, so without this
 * exclusion, re-holding the same combo to leave the layer would first look
 * like "a key outside the layer" and deactivate it immediately, only for the
 * hold-tap to resolve moments later and toggle it straight back on. Keep
 * this list in sync with those combos' key-positions. */
static const uint32_t scroll_layer_entry_positions[] = {12, 13, 18, 19};

static bool is_scroll_layer_entry_position(uint32_t position) {
    for (size_t i = 0; i < ARRAY_SIZE(scroll_layer_entry_positions); i++) {
        if (scroll_layer_entry_positions[i] == position) {
            return true;
        }
    }
    return false;
}

static int automouse_key_control_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (automouse_active) {
        if (!is_layer_bound_key(AUTOMOUSE_LAYER, ev->position)) {
            automouse_active = false;
            zmk_keymap_layer_deactivate(AUTOMOUSE_LAYER);
        }
    } else {
        /* A key press means the trackball movement wasn't a click setup;
         * cancel any pending activation and require a fresh movement streak. */
        k_work_cancel_delayable(&automouse_activation_work);
        movement_streak_start = 0;
    }

    if (zmk_keymap_layer_active(SCROLL_LAYER) && !is_scroll_layer_entry_position(ev->position) &&
        !is_layer_bound_key(SCROLL_LAYER, ev->position)) {
        zmk_keymap_layer_deactivate(SCROLL_LAYER);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(automouse_key_control, automouse_key_control_listener);
ZMK_SUBSCRIPTION(automouse_key_control, zmk_position_state_changed);
