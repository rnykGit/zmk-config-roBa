/*
 * Auto mouse layer key control for roBa.
 *
 * - Pressing/releasing a key that has a binding on the auto mouse layer
 *   restarts the layer timeout (CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS).
 * - Pressing any other key deactivates the auto mouse layer immediately.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#define AUTOMOUSE_LAYER DT_PROP(DT_NODELABEL(trackball), automouse_layer)

#if AUTOMOUSE_LAYER > 0

/* Defined in the PMW3610 driver (zmk-pmw3610-driver/src/pmw3610.c). */
extern struct k_timer automouse_layer_timer;

static bool is_automouse_layer_key(uint32_t position) {
    const struct zmk_behavior_binding *binding =
        zmk_keymap_get_layer_binding_at_idx(AUTOMOUSE_LAYER, position);
    if (binding == NULL || binding->behavior_dev == NULL) {
        return false;
    }

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    return dev != NULL && dev != DEVICE_DT_GET(DT_NODELABEL(trans));
}

static int automouse_key_control_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !zmk_keymap_layer_active(AUTOMOUSE_LAYER)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (is_automouse_layer_key(ev->position)) {
        k_timer_start(&automouse_layer_timer, K_MSEC(CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS),
                      K_NO_WAIT);
    } else if (ev->state) {
        k_timer_stop(&automouse_layer_timer);
        zmk_keymap_layer_deactivate(AUTOMOUSE_LAYER);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(automouse_key_control, automouse_key_control_listener);
ZMK_SUBSCRIPTION(automouse_key_control, zmk_position_state_changed);

#endif
