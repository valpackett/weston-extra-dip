#include <compositor.h>
#include <cstring>

#define KEY_ESC 1
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_CAPSLOCK 58
#define KEY_KPLEFTPAREN 179
#define KEY_KPRIGHTPAREN 180

// time funcs from Weston

#define NSEC_PER_SEC 1000000000

static inline void timespec_sub(struct timespec *r, const struct timespec *a,
                                const struct timespec *b) {
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

static inline int64_t timespec_to_msec(const struct timespec *a) {
	return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}

struct keymod_binding_grab {
	struct weston_keyboard_grab grab;
	struct weston_seat *seat;
	struct timespec press_time;
	uint32_t this_key;
	intptr_t emit_key;
};

static void keymod_binding_key(struct weston_keyboard_grab *grab, const struct timespec *time,
                               uint32_t key, uint32_t state) {
	struct keymod_binding_grab *kg = reinterpret_cast<struct keymod_binding_grab *>(grab);
	struct weston_compositor *compositor = kg->seat->compositor;
	struct wl_display *display = compositor->wl_display;

	if (key == kg->this_key) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			memcpy(&kg->press_time, time, sizeof(struct timespec));
			// keep grab
			return;
		} else {
			// only do the press if it was held for a short time
			struct timespec diff;
			timespec_sub(&diff, time, &kg->press_time);
			if (timespec_to_msec(&diff) < 400) {
				struct wl_resource *resource;
				wl_resource_for_each(resource, &grab->keyboard->focus_resource_list) {
					wl_keyboard_send_key(resource, wl_display_next_serial(display), timespec_to_msec(time),
					                     kg->emit_key, WL_KEYBOARD_KEY_STATE_PRESSED);
					wl_keyboard_send_key(resource, wl_display_next_serial(display), timespec_to_msec(time),
					                     kg->emit_key, WL_KEYBOARD_KEY_STATE_RELEASED);
				}
			}
		}
	}

	// *we* have to emit that press here
	if (key != kg->this_key) {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &grab->keyboard->focus_resource_list) {
			wl_keyboard_send_key(resource, wl_display_next_serial(display), timespec_to_msec(time), key,
			                     state);
		}
	}

	weston_keyboard_end_grab(grab->keyboard);
	if (grab->keyboard->input_method_resource) {
		grab->keyboard->grab = &grab->keyboard->input_method_grab;
	}
	delete kg;
}

static void keymod_binding_modifiers(struct weston_keyboard_grab *grab, uint32_t serial,
                                     uint32_t mods_depressed, uint32_t mods_latched,
                                     uint32_t mods_locked, uint32_t group) {
	struct wl_list *resource_list = &grab->keyboard->focus_resource_list;

	struct wl_resource *resource;
	wl_resource_for_each(resource, resource_list) {
		wl_keyboard_send_modifiers(resource, serial, mods_depressed, mods_latched, mods_locked, group);
	}
}

static void keymod_binding_cancel(struct weston_keyboard_grab *grab) {
	struct keymod_binding_grab *kg = reinterpret_cast<struct keymod_binding_grab *>(grab);
	weston_keyboard_end_grab(grab->keyboard);
	delete kg;
}

struct weston_keyboard_grab_interface keymod_binding_keyboard_grab = {
    keymod_binding_key,
    keymod_binding_modifiers,
    keymod_binding_cancel,
};

static void start_keymod_grab(struct weston_keyboard *keyboard, const struct timespec *time,
                              uint32_t key, void *data) {
	struct keymod_binding_grab *grab = new keymod_binding_grab;
	grab->seat = keyboard->seat;
	grab->this_key = key;
	grab->emit_key = reinterpret_cast<intptr_t>(data);
	grab->grab.interface = &keymod_binding_keyboard_grab;
	weston_keyboard_start_grab(keyboard, &grab->grab);
}

WL_EXPORT extern "C" int wet_module_init(struct weston_compositor *compositor, int *argc,
                                         char *argv[]) {
	// for now just hardcoded stuff here

	auto nomod = (enum weston_keyboard_modifier)(0);
	weston_compositor_add_key_binding(compositor, KEY_CAPSLOCK, nomod, start_keymod_grab,
	                                  reinterpret_cast<void *>(KEY_ESC));
	weston_compositor_add_key_binding(compositor, KEY_LEFTSHIFT, nomod, start_keymod_grab,
	                                  reinterpret_cast<void *>(KEY_KPLEFTPAREN));
	weston_compositor_add_key_binding(compositor, KEY_RIGHTSHIFT, nomod, start_keymod_grab,
	                                  reinterpret_cast<void *>(KEY_KPRIGHTPAREN));

	return 0;
}
