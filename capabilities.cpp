#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include <compositor.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <wayland-client.h>
#include "weston-extra-dip-capabilities-api.h"
#include "wldip-capabilities-server-protocol.h"

static std::string join_caps(std::unordered_set<std::string> &caps) {
	std::stringstream ss;
	for (const auto &cap : caps) {
		ss << cap << ", ";
	}
	return ss.str();
}

struct extra_dip_capabilities {
	bool first_client_arrived = false;
	struct weston_compositor *compositor = nullptr;
	std::unordered_set<std::string> known_caps;
	std::unordered_map<struct wl_client *, std::unordered_set<std::string>> caps;
	struct wl_listener vip_destroy_listener {};
};

struct extra_dip_capabilities global_capabilities;

static struct extra_dip_capabilities *api_get(struct weston_compositor *compositor) {
	return &global_capabilities;
}

static void api_create(struct extra_dip_capabilities *ctx, const char *capability) {
	weston_log("capabilities: creating capability '%s'\n", capability);
	ctx->known_caps.insert(std::string(capability));
}

static bool api_check(struct extra_dip_capabilities *ctx, struct wl_client *client,
                      const char *capability) {
	if (ctx->known_caps.count(std::string(capability)) == 0) {
		weston_log(
		    "capabilities: wanted to check capability '%s' on client %p, but it wasn't created\n",
		    capability, client);
		return false;
	}
	bool result = ctx->caps[client].count(std::string(capability)) > 0;
	weston_log("capabilities: checking capability '%s' on client %p: %d\n", capability, client,
	           static_cast<int>(result));
	weston_log("capabilities: client %p capabilities: '%s'\n", client,
	           join_caps(ctx->caps[client]).c_str());
	return result;
}

static void api_grant(struct extra_dip_capabilities *ctx, struct wl_client *client,
                      const char *capability) {
	if (ctx->known_caps.count(std::string(capability)) == 0) {
		weston_log(
		    "capabilities: wanted to grant capability '%s' to client %p, but it wasn't created\n",
		    capability, client);
		return;
	}
	weston_log("capabilities: granting capability '%s' to client %p\n", capability, client);
	ctx->caps[client].insert(std::string(capability));
	weston_log("capabilities: client %p capabilities: '%s'\n", client,
	           join_caps(ctx->caps[client]).c_str());
}

static void api_revoke(struct extra_dip_capabilities *ctx, struct wl_client *client,
                       const char *capability) {
	weston_log("capabilities: revoking capability '%s' from client %p\n", capability, client);
	ctx->caps[client].erase(std::string(capability));
}

static const struct weston_extra_dip_capabilities_api api = {api_get, api_create, api_check,
                                                             api_grant, api_revoke};

struct capability_set {
	std::unordered_set<std::string> caps;
};

static void cs_grant(struct wl_client *client, struct wl_resource *resource,
                     const char *capability) {
	auto *cs = static_cast<struct capability_set *>(wl_resource_get_user_data(resource));
	if (global_capabilities.known_caps.count(std::string(capability)) == 0) {
		wl_resource_post_error(resource, WLDIP_CAPABILITY_SET_ERROR_NONEXISTENT_CAPABILITY,
		                       "requested capability does not exist");
		return;
	}
	if (global_capabilities.caps[client].count(std::string(capability)) == 0) {
		wl_resource_post_error(resource, WLDIP_CAPABILITY_SET_ERROR_DENIED_CAPABILITY,
		                       "requested capability is not available to the current client");
		return;
	}
	cs->caps.insert(std::string(capability));
}

static void cs_spawn(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	auto *cs = static_cast<struct capability_set *>(wl_resource_get_user_data(resource));
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, &fds[0]) != 0) {
		wl_resource_post_error(resource, WLDIP_CAPABILITY_SET_ERROR_SPAWN_FAILED,
		                       "client socket creation failed");
		return;
	}
	auto *new_client = wl_client_create(wl_client_get_display(client), fds[0]);
	global_capabilities.caps[new_client] = cs->caps;
	weston_log("capabilities: spawned client %p with capabilities '%s' from client %p\n", new_client,
	           join_caps(global_capabilities.caps[new_client]).c_str(), client);
	wldip_capability_set_send_spawned(resource, serial, fds[1]);
	close(fds[1]);
}

static void cs_destructor(struct wl_resource *resource) {
	auto *cs = static_cast<struct capability_set *>(wl_resource_get_user_data(resource));
	if (cs == nullptr) {
		return;
	}
	delete cs;
}

static void cs_destroy(struct wl_client *client, struct wl_resource *resource) {
	cs_destructor(resource);
}

static const struct wldip_capability_set_interface cs_impl = {cs_grant, cs_spawn, cs_destroy};

static void create_capability_set(struct wl_client *client, struct wl_resource *resource,
                                  uint32_t id) {
	auto *cs = new capability_set;
	auto *new_resource = wl_resource_create(client, &wldip_capability_set_interface, 1, id);
	wl_resource_set_implementation(new_resource, &cs_impl, cs, cs_destructor);
}

static const struct wldip_capabilities_interface impl = {create_capability_set};

static void vip_is_down(struct wl_listener *listener, void *data) {
	weston_log("capabilities: the important client is dead, quitting for security\n");
	weston_compositor_exit(global_capabilities.compositor);
}

static void normal_client_is_down(struct wl_listener *listener, void *data) {
	weston_log("capabilities: cleanup client %p\n", data);
	global_capabilities.caps.erase(reinterpret_cast<struct wl_client *>(data));
	wl_list_remove(&listener->link);
	delete listener;
}

static void bind_caps(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	auto *resource = wl_resource_create(client, &wldip_capabilities_interface, 1, id);
	if (!global_capabilities.first_client_arrived) {
		global_capabilities.first_client_arrived = true;
		global_capabilities.caps[client] = global_capabilities.known_caps;
		global_capabilities.vip_destroy_listener.notify = vip_is_down;
		wl_client_add_destroy_listener(client, &global_capabilities.vip_destroy_listener);
		weston_log(
		    "capabilities: first client %p arrived, granted capabilities '%s', considered as "
		    "important\n",
		    client, join_caps(global_capabilities.caps[client]).c_str());
	}
	auto *destroy_listener = new wl_listener{.notify = normal_client_is_down};
	wl_client_add_destroy_listener(client, destroy_listener);
	wl_resource_set_implementation(resource, &impl, data, nullptr);
}

WL_EXPORT int wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[]) {
	global_capabilities.compositor = compositor;
	if (weston_plugin_api_register(compositor, WESTON_EXTRA_DIP_CAPABILITIES_API_NAME, &api,
	                               sizeof(api)) < 0) {
		return -1;
	}
	wl_global_create(compositor->wl_display, &wldip_capabilities_interface, 1, nullptr, bind_caps);
	return 0;
}
}
