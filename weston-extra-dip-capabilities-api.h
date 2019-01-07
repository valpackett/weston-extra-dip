#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <unistd.h>

#include <plugin-registry.h>

struct weston_compositor;
struct weston_view;
struct weston_seat;
struct extra_dip_capabilities;

#define WESTON_EXTRA_DIP_CAPABILITIES_API_NAME "weston_extra_dip_capabilities_v1"

struct weston_extra_dip_capabilities_api {
	struct extra_dip_capabilities *(*get)(struct weston_compositor *compositor);

	void (*create)(struct extra_dip_capabilities *shell, const char *capability);
	bool (*check)(struct extra_dip_capabilities *shell, struct wl_client *client,
	              const char *capability);
	void (*grant)(struct extra_dip_capabilities *shell, struct wl_client *client,
	              const char *capability);
	void (*revoke)(struct extra_dip_capabilities *shell, struct wl_client *client,
	               const char *capability);
};

static inline const struct weston_extra_dip_capabilities_api *weston_extra_dip_capabilities_get_api(
    struct weston_compositor *compositor) {
	const void *api;
	api = weston_plugin_api_get(compositor, WESTON_EXTRA_DIP_CAPABILITIES_API_NAME,
	                            sizeof(struct weston_extra_dip_capabilities_api));
	/* The cast is necessary to use this function in C++ code */
	return (const struct weston_extra_dip_capabilities_api *)api;
}

#ifdef __cplusplus
}
#endif
