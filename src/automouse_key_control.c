/*
 * Custom sticky-layer control for roBa.
 *
 * Auto mouse layer (CONFIG_ROBA_AUTOMOUSE_LAYER):
 * The PMW3610 driver's own automouse-layer feature is left disabled (see the
 * comment on &trackball in config/roBa.keymap) so this file has full control:
 * - Every trackball movement (re)starts a CONFIG_ROBA_AUTOMOUSE_ACTIVATION_DELAY_MS
 *   countdown. If it elapses with no other key press, the auto mouse layer is
 *   activated. Pressing any key before it elapses cancels the countdown.
 * - There is no time-based exit: once active, the layer stays active until a
 *   key not bound on it is pressed, which deactivates it immediately.
 *
 * Scroll layer (CONFIG_ROBA_SCROLL_LAYER):
 * Activated elsewhere (the &lt_sticky hold-tap in the keymap). Once active,
 * it likewise has no timeout: it stays active until a key not bound on that
 * layer is pressed, at which point it is deactivated here.
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

static bool automouse_active;

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

    /* (Re)start the countdown on every movement, so it only fires once
     * movement has been still for the full delay. */
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
         * cancel any pending activation. */
        k_work_cancel_delayable(&automouse_activation_work);
    }

    if (zmk_keymap_layer_active(SCROLL_LAYER) && !is_layer_bound_key(SCROLL_LAYER, ev->position)) {
        zmk_keymap_layer_deactivate(SCROLL_LAYER);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(automouse_key_control, automouse_key_control_listener);
ZMK_SUBSCRIPTION(automouse_key_control, zmk_position_state_changed);
