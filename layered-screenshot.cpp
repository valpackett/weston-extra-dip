#include <iostream>
#include <memory>
//#include <unordered_map>
#include <compositor.h>
#include "wldip-layered-screenshooter-server-protocol.h"
#include "Screenshot_generated.h"

struct ls_context {
	struct weston_compositor *compositor;

	ls_context(struct weston_compositor *c) : compositor(c) { }

	ls_context(ls_context&&) = delete;
};

static void shoot(struct wl_client *client, struct wl_resource *resource) {
	auto *ctx = reinterpret_cast<struct ls_context*>(wl_resource_get_user_data(resource));
	struct weston_view *view;
	wl_list_for_each(view, &ctx->compositor->view_list, link) {
		weston_log("Surface role %s\n", weston_surface_get_role(view->surface));
	}
}

static struct wldip_layered_screenshooter_interface ls_impl = { shoot };

static void bind_shooter(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client, &wldip_layered_screenshooter_interface, 1, id);
	// TODO privilege check
	wl_resource_set_implementation(resource, &ls_impl, data, nullptr);
}

WL_EXPORT extern "C" int wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[]) {
	auto ctx = new ls_context(compositor);
	wl_global_create(compositor->wl_display, &wldip_layered_screenshooter_interface, 1, reinterpret_cast<void*>(ctx), bind_shooter);
	return 0;
}
