#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>
#include "Management_generated.h"

extern "C" {
#include <compositor-drm.h>
#include <compositor.h>
#include <desktop-shell-api.h>
#include <fcntl.h>
#include <libinput-device.h>
#include <libinput-seat.h>
#include <libweston-desktop.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include "wldip-compositor-manager-server-protocol.h"

static void on_create_surface(struct wl_listener *listener, void *data);
static void on_activate(struct wl_listener *listener, void *data);
static void on_output_created(struct wl_listener *listener, void *data);
static void on_output_destroyed(struct wl_listener *listener, void *data);
static void on_output_moved(struct wl_listener *listener, void *data);
static void on_output_resized(struct wl_listener *listener, void *data);
static void on_output_heads_changed(struct wl_listener *listener, void *data);
static void on_input_devices_changed(struct wl_listener *listener, void *data);

struct cm_context {
	struct weston_compositor *compositor;
	const struct weston_desktop_shell_api *desk_shell;
	std::unordered_set<wl_resource *> surfaces_subscribers;
	std::unordered_set<wl_resource *> outputs_subscribers;
	std::unordered_set<wl_resource *> inputdevs_subscribers;
	struct wl_listener create_surface_listener {};
	struct wl_listener activate_listener {};
	struct wl_listener output_created_listener {};
	struct wl_listener output_destroyed_listener {};
	struct wl_listener output_moved_listener {};
	struct wl_listener output_resized_listener {};
	struct wl_listener output_heads_changed_listener {};
	struct wl_listener input_devices_changed_listener {};

	cm_context(struct weston_compositor *c) : compositor(c) {
		desk_shell = weston_desktop_shell_get_api(c);
		create_surface_listener.notify = on_create_surface;
		wl_signal_add(&c->create_surface_signal, &create_surface_listener);
		activate_listener.notify = on_activate;
		wl_signal_add(&c->activate_signal, &activate_listener);
		output_created_listener.notify = on_output_created;
		wl_signal_add(&c->output_created_signal, &output_created_listener);
		output_destroyed_listener.notify = on_output_destroyed;
		wl_signal_add(&c->output_destroyed_signal, &output_destroyed_listener);
		output_moved_listener.notify = on_output_moved;
		wl_signal_add(&c->output_moved_signal, &output_moved_listener);
		output_resized_listener.notify = on_output_resized;
		wl_signal_add(&c->output_resized_signal, &output_resized_listener);
		output_heads_changed_listener.notify = on_output_heads_changed;
		wl_signal_add(&c->output_heads_changed_signal, &output_heads_changed_listener);
		input_devices_changed_listener.notify = on_input_devices_changed;
		wl_signal_add(&c->input_devices_changed_signal, &input_devices_changed_listener);
	}

	int make_update() {
		using namespace wldip::compositor_management;
		flatbuffers::FlatBufferBuilder builder(4096);

		std::vector<flatbuffers::Offset<Head>> fheads;
		struct weston_head *head;
		wl_list_for_each(head, &compositor->head_list, compositor_link) {
			const char *name = head->name != nullptr ? head->name : "";
			const char *make = head->make != nullptr ? head->make : "";
			const char *model = head->model != nullptr ? head->model : "";
			const char *serial_number = head->serial_number != nullptr ? head->serial_number : "";
			fheads.push_back(CreateHead(
			    builder, builder.CreateString(name), head->output != nullptr ? head->output->id : -1,
			    head->mm_width, head->mm_height, builder.CreateString(make), builder.CreateString(model),
			    builder.CreateString(serial_number), head->subpixel, head->connection_internal,
			    head->connected, head->non_desktop));
		}

		std::vector<flatbuffers::Offset<Output>> foutputs;
		struct weston_output *output;
		wl_list_for_each(output, &compositor->output_list, link) {
			const char *name = output->name != nullptr ? output->name : "";
			foutputs.push_back(CreateOutput(builder, output->id, builder.CreateString(name), output->x,
			                                output->y, output->width, output->height,
			                                output->current_scale, output->original_scale));
		}

		std::vector<flatbuffers::Offset<Seat>> fseats;
		struct weston_seat *seat;
		wl_list_for_each(seat, &compositor->seat_list, link) {
			std::vector<flatbuffers::Offset<InputDevice>> finputs;
			// TODO: support fbdev/scfb
			if (weston_drm_virtual_output_get_api(compositor) != nullptr) {
				auto *useat = reinterpret_cast<udev_seat *>(seat);
				struct evdev_device *device;
				wl_list_for_each(device, &useat->devices_list, link) {
					double width, height;
					libinput_device_get_size(device->device, &width, &height);
					int finger_count = libinput_device_config_tap_get_finger_count(device->device);
					std::vector<uint32_t> click_methods;
					uint32_t cmethods = libinput_device_config_scroll_get_methods(device->device);
					if ((cmethods & LIBINPUT_CONFIG_CLICK_METHOD_NONE) != 0u) {
						click_methods.push_back(ClickMethod_None);
					}
					if ((cmethods & LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) != 0u) {
						click_methods.push_back(ClickMethod_ButtonAreas);
					}
					if ((cmethods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) != 0u) {
						click_methods.push_back(ClickMethod_ClickFinger);
					}
					std::vector<uint32_t> scroll_methods;
					uint32_t smethods = libinput_device_config_scroll_get_methods(device->device);
					if ((smethods & LIBINPUT_CONFIG_SCROLL_NO_SCROLL) != 0u) {
						scroll_methods.push_back(ScrollMethod_None);
					}
					if ((smethods & LIBINPUT_CONFIG_SCROLL_2FG) != 0u) {
						scroll_methods.push_back(ScrollMethod_TwoFingers);
					}
					if ((smethods & LIBINPUT_CONFIG_SCROLL_EDGE) != 0u) {
						scroll_methods.push_back(ScrollMethod_Edge);
					}
					if ((smethods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) != 0u) {
						scroll_methods.push_back(ScrollMethod_OnButtonDown);
					}
					std::vector<uint8_t> capabilities;
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_KEYBOARD) != 0) {
						capabilities.push_back(DeviceCapability_Keyboard);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_POINTER) != 0) {
						capabilities.push_back(DeviceCapability_Pointer);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_TOUCH) != 0) {
						capabilities.push_back(DeviceCapability_Touch);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_TABLET_TOOL) !=
					    0) {
						capabilities.push_back(DeviceCapability_TabletTool);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_TABLET_PAD) != 0) {
						capabilities.push_back(DeviceCapability_TabletPad);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_GESTURE) != 0) {
						capabilities.push_back(DeviceCapability_Gesture);
					}
					if (libinput_device_has_capability(device->device, LIBINPUT_DEVICE_CAP_SWITCH) != 0) {
						capabilities.push_back(DeviceCapability_Switch);
					}
					finputs.push_back(CreateInputDevice(
					    builder, libinput_device_get_id_product(device->device),
					    libinput_device_get_id_vendor(device->device), width, height,
					    libinput_device_touch_get_touch_count(device->device), finger_count,
					    libinput_device_config_tap_get_default_enabled(device->device) != 0u,
					    libinput_device_config_tap_get_enabled(device->device) != 0u,
					    finger_count == 0 ? TapButtonMap_MIN
					                      : static_cast<TapButtonMap>(
					                            libinput_device_config_tap_get_button_map(device->device)),
					    finger_count == 0
					        ? TapButtonMap_MIN
					        : static_cast<TapButtonMap>(
					              libinput_device_config_tap_get_default_button_map(device->device)),
					    libinput_device_config_tap_get_default_drag_enabled(device->device) != 0u,
					    libinput_device_config_tap_get_drag_enabled(device->device) != 0u,
					    libinput_device_config_tap_get_default_drag_lock_enabled(device->device) != 0u,
					    libinput_device_config_tap_get_drag_lock_enabled(device->device) != 0u,
					    static_cast<SendEventsMode>(
					        libinput_device_config_send_events_get_default_mode(device->device)),
					    static_cast<SendEventsMode>(
					        libinput_device_config_send_events_get_mode(device->device)),
					    libinput_device_config_accel_get_default_speed(device->device),
					    libinput_device_config_accel_get_speed(device->device),
					    static_cast<AccelerationProfile>(
					        libinput_device_config_accel_get_default_profile(device->device)),
					    static_cast<AccelerationProfile>(
					        libinput_device_config_accel_get_profile(device->device)),
					    libinput_device_config_scroll_has_natural_scroll(device->device) != 0,
					    libinput_device_config_scroll_get_default_natural_scroll_enabled(device->device) != 0,
					    libinput_device_config_scroll_get_natural_scroll_enabled(device->device) != 0,
					    libinput_device_config_left_handed_is_available(device->device) != 0,
					    libinput_device_config_left_handed_get_default(device->device) != 0,
					    libinput_device_config_left_handed_get(device->device) != 0,
					    builder.CreateVector(click_methods),
					    static_cast<ClickMethod>(
					        libinput_device_config_click_get_default_method(device->device)),
					    static_cast<ClickMethod>(libinput_device_config_click_get_method(device->device)),
					    libinput_device_config_middle_emulation_is_available(device->device) != 0,
					    libinput_device_config_middle_emulation_get_default_enabled(device->device) != 0u,
					    libinput_device_config_middle_emulation_get_enabled(device->device) != 0u,
					    builder.CreateVector(scroll_methods),
					    static_cast<ScrollMethod>(
					        libinput_device_config_scroll_get_default_method(device->device)),
					    static_cast<ScrollMethod>(libinput_device_config_scroll_get_method(device->device)),
					    libinput_device_config_scroll_get_default_button(device->device),
					    libinput_device_config_scroll_get_button(device->device),
					    libinput_device_config_dwt_is_available(device->device) != 0,
					    libinput_device_config_dwt_get_default_enabled(device->device) != 0u,
					    libinput_device_config_dwt_get_enabled(device->device) != 0u,
					    libinput_device_config_rotation_is_available(device->device) != 0,
					    libinput_device_config_rotation_get_default_angle(device->device),
					    libinput_device_config_rotation_get_angle(device->device),
					    builder.CreateVector(capabilities),
					    builder.CreateString(libinput_device_get_name(
					        device->device)),  // libinput promises to never return NULL
					    builder.CreateString(libinput_device_get_sysname(device->device))));
				}
			}
			fseats.push_back(CreateSeat(builder, builder.CreateString(seat->seat_name),
			                            builder.CreateVector(finputs)));
		}

		std::vector<flatbuffers::Offset<Surface>> fsurfaces;
		std::unordered_set<struct weston_surface *> surfaces;
		struct weston_view *view;
		wl_list_for_each(view, &compositor->view_list, link) { surfaces.insert(view->surface); }
		for (const auto surface : surfaces) {
			flatbuffers::Offset<DesktopSurface> dsurfo = 0;
			if (weston_surface_is_desktop_surface(surface)) {
				auto dsurf = weston_surface_get_desktop_surface(surface);
				const char *titlestr = weston_desktop_surface_get_title(dsurf);
				auto titlestro = builder.CreateString(titlestr != nullptr ? titlestr : "");
				const char *appidstr = weston_desktop_surface_get_app_id(dsurf);
				auto appidstro = builder.CreateString(appidstr != nullptr ? appidstr : "");
				DesktopSurfaceBuilder dsurfb(builder);
				dsurfb.add_title(titlestro);
				dsurfb.add_app_id(appidstro);
				dsurfb.add_pid(weston_desktop_surface_get_pid(dsurf));
				dsurfb.add_activated(weston_desktop_surface_get_activated(dsurf));
				dsurfb.add_maximized(weston_desktop_surface_get_maximized(dsurf));
				dsurfb.add_fullscreen(weston_desktop_surface_get_fullscreen(dsurf));
				dsurfb.add_resizing(weston_desktop_surface_get_resizing(dsurf));
				auto max_size = weston_desktop_surface_get_max_size(dsurf);
				dsurfb.add_max_width(max_size.width);
				dsurfb.add_max_height(max_size.height);
				auto min_size = weston_desktop_surface_get_min_size(dsurf);
				dsurfb.add_min_width(min_size.width);
				dsurfb.add_min_height(min_size.height);
				dsurfo = dsurfb.Finish();
			}

			const char *rolename = surface->role_name != nullptr ? surface->role_name : "";
			auto rolenameo = builder.CreateString(rolename);
			std::string label;
			if (surface->get_label != nullptr) {
				label.resize(1024);
				label.resize(surface->get_label(surface, const_cast<char *>(label.c_str()), 1024));
			}
			auto labelo = builder.CreateString(label);
			SurfaceBuilder surfb(builder);
			surfb.add_uid(reinterpret_cast<uint64_t>(surface) % 1000000);
			Role role = Role_Other;
			surfb.add_other_role(rolenameo);
			if (std::string(rolename) == "xdg_toplevel") {
				role = Role_XdgToplevel;
			} else if (std::string(rolename) == "layer-shell") {
				role = Role_Lsh;
			}
			surfb.add_role(role);
			surfb.add_label(labelo);
			if (surface->output != nullptr) {
				surfb.add_primary_output_id(surface->output->id);
			}
			if (weston_surface_is_desktop_surface(surface)) {
				surfb.add_desktop(dsurfo);
			}
			fsurfaces.push_back(surfb.Finish());
		}

		builder.Finish(
		    CreateCompositorState(builder, compositor->kb_repeat_rate, compositor->kb_repeat_delay,
		                          builder.CreateVector(fheads), builder.CreateVector(foutputs),
		                          builder.CreateVector(fseats), builder.CreateVector(fsurfaces)));
		int fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0644);
		ftruncate(fd, builder.GetSize());
		write(fd, builder.GetBufferPointer(), builder.GetSize());
		lseek(fd, 0, SEEK_SET);
		return fd;
	}

	void send_update_to(struct wl_resource *resource) {
		int fd = make_update();
		wldip_compositor_manager_send_update(resource, fd);
		close(fd);
	}

	void send_update_surface() {
		int fd = make_update();
		for (auto resource : surfaces_subscribers) {
			wldip_compositor_manager_send_update(resource, fd);
		}
		close(fd);
	}

	void send_update_output() {
		int fd = make_update();
		for (auto resource : outputs_subscribers) {
			wldip_compositor_manager_send_update(resource, fd);
		}
		close(fd);
	}

	void send_update_inputdevs() {
		int fd = make_update();
		for (auto resource : inputdevs_subscribers) {
			wldip_compositor_manager_send_update(resource, fd);
		}
		close(fd);
	}

	void with_input_device(uint32_t seat_idx, uint32_t device_idx,
	                       const std::function<void(evdev_device *)> &f) {
		struct weston_seat *seat;
		uint32_t cur_seat = 0, cur_dev = 0;
		wl_list_for_each(seat, &compositor->seat_list, link) {
			if (cur_seat++ != seat_idx) {
				continue;
			}
			// TODO: support fbdev/scfb
			if (weston_drm_virtual_output_get_api(compositor) != nullptr) {
				auto *useat = reinterpret_cast<udev_seat *>(seat);
				struct evdev_device *device;
				wl_list_for_each(device, &useat->devices_list, link) {
					if (cur_dev++ == device_idx) {
						f(device);
						break;
					}
				}
			}
		}
	}

	cm_context(cm_context &&) = delete;
};

static void on_create_surface(struct wl_listener *listener, void *data) {
	auto *ctx =
	    wl_container_of(listener, static_cast<struct cm_context *>(nullptr), create_surface_listener);
	ctx->send_update_surface();
}

static void on_activate(struct wl_listener *listener, void *data) {
	auto *ctx =
	    wl_container_of(listener, static_cast<struct cm_context *>(nullptr), activate_listener);
	ctx->send_update_surface();
}

static void on_output_created(struct wl_listener *listener, void *data) {
	auto *ctx =
	    wl_container_of(listener, static_cast<struct cm_context *>(nullptr), output_created_listener);
	ctx->send_update_output();
}

static void on_output_destroyed(struct wl_listener *listener, void *data) {
	auto *ctx = wl_container_of(listener, static_cast<struct cm_context *>(nullptr),
	                            output_destroyed_listener);
	ctx->send_update_output();
}

static void on_output_moved(struct wl_listener *listener, void *data) {
	auto *ctx =
	    wl_container_of(listener, static_cast<struct cm_context *>(nullptr), output_moved_listener);
	ctx->send_update_output();
}

static void on_output_resized(struct wl_listener *listener, void *data) {
	auto *ctx =
	    wl_container_of(listener, static_cast<struct cm_context *>(nullptr), output_resized_listener);
	ctx->send_update_output();
}

static void on_output_heads_changed(struct wl_listener *listener, void *data) {
	auto *ctx = wl_container_of(listener, static_cast<struct cm_context *>(nullptr),
	                            output_heads_changed_listener);
	ctx->send_update_output();
}

static void on_input_devices_changed(struct wl_listener *listener, void *data) {
	auto *ctx = wl_container_of(listener, static_cast<struct cm_context *>(nullptr),
	                            input_devices_changed_listener);
	ctx->send_update_inputdevs();
}

static void cm_subscribe(struct wl_client *client, struct wl_resource *resource, uint32_t topics) {
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	if ((topics & WLDIP_COMPOSITOR_MANAGER_TOPIC_SURFACES) != 0u) {
		ctx->surfaces_subscribers.insert(resource);
	}
	if ((topics & WLDIP_COMPOSITOR_MANAGER_TOPIC_OUTPUTS) != 0u) {
		ctx->outputs_subscribers.insert(resource);
	}
	if ((topics & WLDIP_COMPOSITOR_MANAGER_TOPIC_INPUTDEVS) != 0u) {
		ctx->inputdevs_subscribers.insert(resource);
	}
}

static void cm_get(struct wl_client *client, struct wl_resource *resource) {
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	ctx->send_update_to(resource);
}

static void cm_desktop_surface_activate(struct wl_client *client, struct wl_resource *resource,
                                        uint32_t surface_uid) {
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	struct weston_view *view;
	wl_list_for_each(view, &ctx->compositor->view_list, link) {
		if (reinterpret_cast<uint64_t>(view->surface) % 1000000 == surface_uid) {
			struct weston_seat *seat;  // TODO smarter than first seat
			wl_list_for_each(seat, &ctx->compositor->seat_list, link) { break; }
			if (!weston_surface_is_desktop_surface(view->surface)) {
				weston_log("compositor-management: trying to activate a non-desktop surface uid %d\n",
				           surface_uid);
				break;
			}
			ctx->desk_shell->activate(ctx->desk_shell->get(ctx->compositor), view, seat,
			                          WESTON_ACTIVATE_FLAG_CONFIGURE);
			break;
		}
	}
}

static void cm_output_set_scale(struct wl_client *client, struct wl_resource *resource,
                                uint32_t output_id, wl_fixed_t new_scale) {
	if (wl_fixed_to_double(new_scale) < 1.0) {
		return;
	}
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	struct weston_output *output;
	wl_list_for_each(output, &ctx->compositor->output_list, link) {
		if (output->id == output_id) {
			weston_output_set_scale(output, wl_fixed_to_double(new_scale));
			break;
		}
	}
}

static void cm_device_set_natural_scrolling(struct wl_client *client, struct wl_resource *resource,
                                            uint32_t seat_idx, uint32_t device_idx,
                                            uint32_t enable) {
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	ctx->with_input_device(seat_idx, device_idx, [enable](const struct evdev_device *device) {
		libinput_device_config_scroll_set_natural_scroll_enabled(device->device, enable);
	});
}

static void cm_destructor(struct wl_resource *resource) {
	auto *ctx = static_cast<struct cm_context *>(wl_resource_get_user_data(resource));
	ctx->surfaces_subscribers.erase(resource);
	ctx->outputs_subscribers.erase(resource);
	ctx->inputdevs_subscribers.erase(resource);
}

static struct wldip_compositor_manager_interface cm_impl = {
    cm_subscribe, cm_get, cm_desktop_surface_activate, cm_output_set_scale,
    cm_device_set_natural_scrolling};

static void bind_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource =
	    wl_resource_create(client, &wldip_compositor_manager_interface, 1, id);
	// TODO privilege check
	wl_resource_set_implementation(resource, &cm_impl, data, cm_destructor);
}

WL_EXPORT int wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[]) {
	auto *ctx = new cm_context(compositor);
	wl_global_create(compositor->wl_display, &wldip_compositor_manager_interface, 1,
	                 reinterpret_cast<void *>(ctx), bind_manager);
	return 0;
}
}
