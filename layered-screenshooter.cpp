#include <iostream>
#include <wayland-client.h>
#include "wldip-layered-screenshooter-client-protocol.h"
#include "Screenshot_generated.h"

static struct wldip_layered_screenshooter *shooter;

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, "wldip_layered_screenshooter") == 0) {
		shooter = reinterpret_cast<struct wldip_layered_screenshooter*>(
				wl_registry_bind(registry, name, &wldip_layered_screenshooter_interface, 1));
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener registry_listener = {
	handle_global,
	handle_global_remove
};

static bool received = false;


static void on_done(void *data, struct wldip_layered_screenshooter *shooter, struct wl_array *contents) {
	received = true;
}

static const struct wldip_layered_screenshooter_listener shooter_listener = { on_done };

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(nullptr);
	if (display == nullptr) {
		std::cerr << "failed to create display" << std::endl;
		return -1;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, nullptr);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	if (shooter == nullptr) {
		std::cerr << "failed to find shooter interface" << std::endl;
		return -1;
	}

	wldip_layered_screenshooter_add_listener(shooter, &shooter_listener, nullptr);
	wldip_layered_screenshooter_shoot(shooter);
	while (!received) {
		wl_display_roundtrip(display);
	}
}
