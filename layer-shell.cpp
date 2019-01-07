#include <iostream>
#include <tuple>
#include <unordered_map>
#include <utility>

extern "C" {
#include <compositor.h>
#include <linux/input.h>
#include <unistd.h>
#include "weston-extra-dip-capabilities-api.h"
#include "wlr-layer-shell-unstable-v1-server-protocol.h"

static struct weston_compositor *compositor = nullptr;
static const struct weston_extra_dip_capabilities_api *caps = nullptr;

struct lsh_context;
static std::unordered_map<struct weston_view *, struct lsh_context *> lsh_views;

struct weston_layer lr_background = {nullptr};
struct weston_layer lr_bottom = {nullptr};
struct weston_layer lr_top = {nullptr};
struct weston_layer lr_overlay = {nullptr};

static void committed_callback(struct weston_surface *surface, int32_t sx, int32_t sy);

static void set_size(struct wl_client *client, struct wl_resource *resource, uint32_t width,
                     uint32_t height);
static void set_anchor(struct wl_client *client, struct wl_resource *resource, uint32_t anchor);
static void set_exclusive_zone(struct wl_client *client, struct wl_resource *resource,
                               int32_t zone);
static void set_margin(struct wl_client *client, struct wl_resource *resource, int32_t top,
                       int32_t right, int32_t bottom, int32_t left);
static void set_keyboard_interactivity(struct wl_client *client, struct wl_resource *resource,
                                       uint32_t keyboard_interactivity);
static void get_popup(struct wl_client *client, struct wl_resource *resource,
                      struct wl_resource *popup);
static void ack_configure(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
static void destroy_lsh(struct wl_client *client, struct wl_resource *resource);

static struct zwlr_layer_surface_v1_interface lsh_impl = {
    set_size,  set_anchor,    set_exclusive_zone, set_margin, set_keyboard_interactivity,
    get_popup, ack_configure, destroy_lsh};

static void lsh_destructor(struct wl_resource *resource);

static void on_output_gone(struct wl_listener *listener, void *data);
static void on_surface_gone(struct wl_listener *listener, void *data);

const auto t = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
const auto r = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
const auto b = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
const auto l = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;

using coords = std::pair<int32_t, int32_t>;

struct lsh_margin {
	int32_t top, right, bottom, left;
};

struct lsh_context {
	struct weston_surface *surface;
	struct weston_view *view;
	struct weston_head *head;
	zwlr_layer_shell_v1_layer layer;
	zwlr_layer_surface_v1_anchor anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
	coords req_size;
	struct lsh_margin margin = {0, 0, 0, 0};
	struct wl_resource *resource;
	struct wl_listener output_destroy_listener {
		.notify = on_output_gone
	};
	struct wl_listener surface_destroy_listener {
		.notify = on_surface_gone
	};
	bool keyboard_interactive = false;
	bool destroyed = false;

	lsh_context(struct weston_surface *s, struct weston_head *h, zwlr_layer_shell_v1_layer l,
	            struct wl_client *client, uint32_t id)
	    : surface(s), head(h), layer(l) {
		if (head != nullptr) {
			wl_signal_add(&weston_head_get_output(head)->destroy_signal, &output_destroy_listener);
			weston_log("layer-shell: attached to output %p\n", weston_head_get_output(head));
		}
		wl_signal_add(&surface->destroy_signal, &surface_destroy_listener);
		view = weston_view_create(surface);
		lsh_views.emplace(view, this);
		surface->committed_private = this;
		surface->committed = committed_callback;
		resource = wl_resource_create(client, &zwlr_layer_surface_v1_interface, 1, id);
		wl_resource_set_implementation(resource, &lsh_impl, this, lsh_destructor);
		if (!weston_view_is_mapped(view)) {
			// the client could commit everything before this constructor ends,
			// so the committed callback wouldn't be called. force a reconfig here
			zwlr_layer_surface_v1_send_configure(resource, 0, 1, 1);
		}
	}

	coords position(coords surface_size, coords output_size) {
		int32_t w, h, ow, oh, x = 0, y = 0;
		std::tie(w, h) = surface_size;
		std::tie(ow, oh) = output_size;
		if ((anchor & (t | b)) == (t | b) ||
		    !(((anchor & t) != 0) || ((anchor & b) != 0))) {  // both or neither
			y = oh / 2 - h / 2 + margin.top / 2 - margin.bottom / 2;
		} else if ((anchor & b) != 0) {
			y = oh - h - margin.bottom;
		} else if ((anchor & t) != 0) {
			y = margin.top;
		}
		if ((anchor & (l | r)) == (l | r) ||
		    !(((anchor & l) != 0) || ((anchor & r) != 0))) {  // both or neither
			x = ow / 2 - w / 2 + margin.left / 2 - margin.right / 2;
		} else if ((anchor & r) != 0) {
			x = ow - w - margin.right;
		} else if ((anchor & l) != 0) {
			x = margin.left;
		}
		return std::make_pair(x, y);
	}

	coords next_size(coords old_size, coords output_size) {
		int32_t w, h, ow, oh, rw, rh;
		std::tie(w, h) = old_size;
		std::tie(ow, oh) = output_size;
		std::tie(rw, rh) = req_size;
		if (rw > 0) {
			w = rw;
		}
		if (rh > 0) {
			h = rh;
		}
		if ((anchor & (l | r)) == (l | r)) {
			w = ow - margin.left - margin.right;
		}
		if ((anchor & (t | b)) == (t | b)) {
			h = oh - margin.top - margin.bottom;
		}
		return std::make_pair(w, h);
	}

	~lsh_context() {
		if (destroyed) {
			weston_log("layer-shell: BUG: destroying already destroyed surface\n");
			return;
		}
		destroyed = true;
		weston_log("layer-shell: destroying surface\n");
		weston_view_damage_below(view);
		weston_view_destroy(view);
		lsh_views.erase(view);
		weston_surface_unmap(surface);
		weston_compositor_schedule_repaint(surface->compositor);
		surface->committed = nullptr;
		surface->committed_private = nullptr;
		wl_resource_set_user_data(resource, nullptr);
		if (head != nullptr) {
			wl_list_remove(&output_destroy_listener.link);
		}
	}

	lsh_context(lsh_context &&) = delete;
};

static void on_output_gone(struct wl_listener *listener, void *data) {
	auto *ctx = wl_container_of(listener, static_cast<struct lsh_context *>(nullptr),
	                            output_destroy_listener);
	weston_log("layer-shell: output gone, sending close to surface\n");
	zwlr_layer_surface_v1_send_closed(ctx->resource);
}

static void on_surface_gone(struct wl_listener *listener, void *data) {
	auto *ctx = wl_container_of(listener, static_cast<struct lsh_context *>(nullptr),
	                            surface_destroy_listener);
	weston_log("layer-shell: surface gone, destroying layer surface\n");
	delete ctx;
	// will set resource user_data to nullptr
}

static void committed_callback(struct weston_surface *surface, int32_t sx, int32_t sy) {
	auto *ctx = static_cast<struct lsh_context *>(surface->committed_private);
	weston_log("layer-shell: is_mapped: %d\n", static_cast<int>(weston_view_is_mapped(ctx->view)));
	if (!weston_view_is_mapped(ctx->view)) {
		switch (ctx->layer) {
			break;
			case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
				weston_layer_entry_insert(&lr_background.view_list, &ctx->view->layer_link);
				break;
			case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
				weston_layer_entry_insert(&lr_bottom.view_list, &ctx->view->layer_link);
				break;
			case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
				weston_layer_entry_insert(&lr_top.view_list, &ctx->view->layer_link);
				break;
			case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
				weston_layer_entry_insert(&lr_overlay.view_list, &ctx->view->layer_link);
		}
		ctx->view->is_mapped = true;
	}
	if (ctx->head != nullptr) {
		ctx->view->output = weston_head_get_output(ctx->head);
	}
	if (ctx->view->output == nullptr) {
		weston_view_update_transform(ctx->view);  // assigns an output if there was none
	}
	if (ctx->view->output == nullptr) {
		weston_log("layer-shell: WTF: no calculated output for surface\n");
		return;
	}
	auto output_size = std::make_pair(ctx->view->output->width, ctx->view->output->height);
	weston_log("layer-shell: output size: %d, %d\n", ctx->view->output->width,
	           ctx->view->output->height);
	auto surface_size = std::make_pair(surface->width, surface->height);
	weston_log("layer-shell: surface size: %d, %d\n", surface->width, surface->height);
	int32_t x, y, nw, nh;
	std::tie(x, y) = ctx->position(surface_size, output_size);
	weston_log("layer-shell: calculated position + output position: %d + %d, %d + %d\n", x,
	           ctx->view->output->x, y, ctx->view->output->y);
	weston_view_set_position(ctx->view, x + ctx->view->output->x, y + ctx->view->output->y);
	std::tie(nw, nh) = ctx->next_size(surface_size, output_size);
	weston_log("layer-shell: next size: %d, %d\n", nw, nh);
	if (nw != surface->width || nh != surface->height) {
		weston_log("layer-shell: sending configure\n");
		zwlr_layer_surface_v1_send_configure(ctx->resource, 0, nw, nh);
	}
	weston_view_update_transform(
	    ctx->view);  // -> view_assign_output -> view_set_output -> sets destroy listener
	weston_surface_damage(surface);
	weston_compositor_schedule_repaint(surface->compositor);
}

static void set_size(struct wl_client *client, struct wl_resource *resource, uint32_t width,
                     uint32_t height) {
	auto *ctx = static_cast<struct lsh_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	ctx->req_size.first = width;
	ctx->req_size.second = height;
}

static void set_anchor(struct wl_client *client, struct wl_resource *resource, uint32_t anchor) {
	auto *ctx = static_cast<struct lsh_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	ctx->anchor = static_cast<zwlr_layer_surface_v1_anchor>(anchor);
}

static void set_exclusive_zone(struct wl_client *client, struct wl_resource *resource,
                               int32_t zone) {
	weston_log("layer-shell: exclusive zone not supported yet\n");
}

static void set_margin(struct wl_client *client, struct wl_resource *resource, int32_t top,
                       int32_t right, int32_t bottom, int32_t left) {
	auto *ctx = static_cast<struct lsh_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	ctx->margin.top = top;
	ctx->margin.right = right;
	ctx->margin.bottom = bottom;
	ctx->margin.left = left;
}

static void set_keyboard_interactivity(struct wl_client *client, struct wl_resource *resource,
                                       uint32_t keyboard_interactivity) {
	auto *ctx = static_cast<struct lsh_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	ctx->keyboard_interactive = !!keyboard_interactivity;
}

static void get_popup(struct wl_client *client, struct wl_resource *resource,
                      struct wl_resource *popup) {
	weston_log("layer-shell: popup not supported yet\n");
}

static void ack_configure(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
}

static void destroy_lsh(struct wl_client *client, struct wl_resource *resource) {
	lsh_destructor(resource);
}

static void lsh_destructor(struct wl_resource *resource) {
	auto *ctx = static_cast<struct lsh_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	delete ctx;
}

static void get_layer_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                              struct wl_resource *res_surface, struct wl_resource *res_output,
                              uint32_t layer, const char *ns) {
	if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		wl_resource_post_error(resource, ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER,
		                       "layer is bigger than enum max");
		return;
	}

	if (layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY &&
	    !caps->check(caps->get(compositor), client, "layer-shell-overlay")) {
		weston_log(
		    "layer-shell: client %p does not have layer-shell-overlay capability, using top layer\n",
		    client);
		layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	}

	auto *surface = static_cast<struct weston_surface *>(wl_resource_get_user_data(res_surface));

	if (weston_surface_set_role(surface, "layer-shell", resource, ZWLR_LAYER_SHELL_V1_ERROR_ROLE) <
	    0) {
		return;
	}

	auto *head = res_output != nullptr
	                 ? static_cast<struct weston_head *>(wl_resource_get_user_data(res_output))
	                 : nullptr;

	new lsh_context(surface, head, static_cast<zwlr_layer_shell_v1_layer>(layer), client, id);
}

static struct zwlr_layer_shell_v1_interface shell_impl = {get_layer_surface};

static void bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	if (!caps->check(caps->get(compositor), client, "layer-shell")) {
		weston_log("layer-shell: client %p does not have layer-shell capability\n", client);
		return;
	}
	struct wl_resource *resource = wl_resource_create(client, &zwlr_layer_shell_v1_interface, 1, id);
	wl_resource_set_implementation(resource, &shell_impl, data, nullptr);
}

// XXX: keyboard interactivity handling: fine for below-desktop, incomplete for above-desktop
//
// When a layer-shell surface gets focus, desktop shell will still think that it was focused,
// so the Mod+Tab window switcher will go to the next surface on the first hit

static bool refocus_topmost_interactive_surface(struct weston_seat *seat) {
	// Relies on the fact that our binding runs after the desktop shell's because plugins are loaded
	// after the shell!
	for (auto &p : lsh_views) {
		if (p.second->keyboard_interactive && p.second->layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
			weston_view_activate(p.first, seat, WESTON_ACTIVATE_FLAG_CONFIGURE);
			return true;
		}
	}
	for (auto &p : lsh_views) {
		if (p.second->keyboard_interactive && p.second->layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
			weston_view_activate(p.first, seat, WESTON_ACTIVATE_FLAG_CONFIGURE);
			return true;
		}
	}
	return false;
}

static void click_to_activate_binding(struct weston_pointer *pointer, const struct timespec *time,
                                      uint32_t button, void *data) {
	if (pointer->grab != &pointer->default_grab || pointer->focus == nullptr ||
	    refocus_topmost_interactive_surface(pointer->seat) || lsh_views.count(pointer->focus) == 0) {
		return;
	}
	auto *ctx = lsh_views[pointer->focus];
	if (!ctx->keyboard_interactive) {
		return;
	}
	weston_view_activate(pointer->focus, pointer->seat,
	                     WESTON_ACTIVATE_FLAG_CLICKED | WESTON_ACTIVATE_FLAG_CONFIGURE);
}

static void touch_to_activate_binding(struct weston_touch *touch, const struct timespec *time,
                                      void *data) {
	if (touch->grab != &touch->default_grab || touch->focus == nullptr ||
	    refocus_topmost_interactive_surface(touch->seat) || lsh_views.count(touch->focus) == 0) {
		return;
	}
	auto *ctx = lsh_views[touch->focus];
	if (!ctx->keyboard_interactive) {
		return;
	}
	weston_view_activate(touch->focus, touch->seat, WESTON_ACTIVATE_FLAG_CONFIGURE);
}

WL_EXPORT int wet_module_init(struct weston_compositor *ec, int *argc, char *argv[]) {
	compositor = ec;
	if ((caps = weston_extra_dip_capabilities_get_api(compositor)) == nullptr) {
		weston_log(
		    "layer-shell: did not find capabilities api, did you put the capabilities plugin before "
		    "this one?\n");
		return -1;
	}
	caps->create(caps->get(compositor), "layer-shell");
	caps->create(caps->get(compositor), "layer-shell-overlay");
	weston_layer_init(&lr_background, compositor);
	weston_layer_set_position(&lr_background, WESTON_LAYER_POSITION_BACKGROUND);
	weston_layer_init(&lr_bottom, compositor);
	weston_layer_set_position(&lr_bottom, WESTON_LAYER_POSITION_BOTTOM_UI);
	weston_layer_init(&lr_top, compositor);
	weston_layer_set_position(&lr_top, WESTON_LAYER_POSITION_UI);
	weston_layer_init(&lr_overlay, compositor);
	weston_layer_set_position(&lr_overlay, WESTON_LAYER_POSITION_LOCK);
	weston_compositor_add_button_binding(ec, BTN_LEFT, static_cast<enum weston_keyboard_modifier>(0),
	                                     click_to_activate_binding, nullptr);
	weston_compositor_add_button_binding(ec, BTN_RIGHT, static_cast<enum weston_keyboard_modifier>(0),
	                                     click_to_activate_binding, nullptr);
	weston_compositor_add_touch_binding(ec, static_cast<enum weston_keyboard_modifier>(0),
	                                    touch_to_activate_binding, nullptr);
	wl_global_create(compositor->wl_display, &zwlr_layer_shell_v1_interface, 1, nullptr, bind_shell);
	return 0;
}
}
