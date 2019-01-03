#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>
#include <webp/encode.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include "Management_generated.h"
#include "wldip-compositor-manager-client-protocol.h"

static struct wldip_compositor_manager *shooter;

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
	if (strcmp(interface, "wldip_compositor_manager") == 0) {
		shooter = reinterpret_cast<struct wldip_compositor_manager *>(
		    wl_registry_bind(registry, name, &wldip_compositor_manager_interface, 1));
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {handle_global, handle_global_remove};

static std::string button_map_name(wldip::compositor_management::TapButtonMap tbm) {
	using namespace wldip::compositor_management;
	if (tbm == TapButtonMap_LeftRightMiddle) return "Left-Right-Middle";
	if (tbm == TapButtonMap_LeftMiddleRight) return "Left-Middle-Right";
	return "-UNKNOWN-";
}

static std::string send_events_mode_name(wldip::compositor_management::SendEventsMode sem) {
	using namespace wldip::compositor_management;
	if (sem == SendEventsMode_Enabled) return "Enabled";
	if (sem == SendEventsMode_Disabled) return "Disabled";
	if (sem == SendEventsMode_DisabledOnExternalMouse)
		return "Disabled when an external mouse is connected";
	return "-UNKNOWN-";
}

static std::string accel_profile_name(wldip::compositor_management::AccelerationProfile ap) {
	using namespace wldip::compositor_management;
	if (ap == AccelerationProfile_None) return "None";
	if (ap == AccelerationProfile_Flat) return "Flat";
	if (ap == AccelerationProfile_Adaptive) return "Adaptive";
	return "-UNKNOWN-";
}

static std::string click_method_name(wldip::compositor_management::ClickMethod cm) {
	using namespace wldip::compositor_management;
	if (cm == ClickMethod_None) return "None";
	if (cm == ClickMethod_ButtonAreas) return "Button areas";
	if (cm == ClickMethod_ClickFinger) return "Clickfinger";
	return "-UNKNOWN-";
}

static std::string scroll_method_name(wldip::compositor_management::ScrollMethod cm) {
	using namespace wldip::compositor_management;
	if (cm == ScrollMethod_None) return "None";
	if (cm == ScrollMethod_TwoFingers) return "Two-finger";
	if (cm == ScrollMethod_Edge) return "Edge";
	if (cm == ScrollMethod_OnButtonDown) return "Button holding";
	return "-UNKNOWN-";
}

static std::string capability_name(wldip::compositor_management::DeviceCapability cap) {
	using namespace wldip::compositor_management;
	if (cap == DeviceCapability_Keyboard) return "keyboard";
	if (cap == DeviceCapability_Pointer) return "pointer";
	if (cap == DeviceCapability_Touch) return "touch";
	if (cap == DeviceCapability_TabletTool) return "tablet-tool";
	if (cap == DeviceCapability_TabletPad) return "tablet-pad";
	if (cap == DeviceCapability_Gesture) return "gesture";
	if (cap == DeviceCapability_Switch) return "switch";
	return "-UNKNOWN-";
}

static uint64_t updates_recvd = 0;

static void on_update(void *data, struct wldip_compositor_manager *shooter, int recv_fd) {
	using namespace wldip::compositor_management;
	struct stat recv_stat {};
	fstat(recv_fd, &recv_stat);
	void *fbuf = mmap(nullptr, recv_stat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, recv_fd, 0);
	const auto state = GetCompositorState(fbuf);
	std::cout.imbue(std::locale("C"));
	std::cout << std::boolalpha;
	std::cout << "Keyboard repeat rate: " << state->kb_repeat_rate() << std::endl;
	std::cout << "Keyboard repeat delay: " << state->kb_repeat_delay() << std::endl;

	std::cout << "Seats [" << state->seats()->size() << "]:" << std::endl;
	for (const auto seat : *state->seats()) {
		std::cout << "  Name: " << seat->name()->str() << std::endl;
		std::cout << "  Input devices [" << seat->input_devices()->size() << "]:" << std::endl;
		for (const auto device : *seat->input_devices()) {
			std::cout << "    Name: " << device->name()->str() << std::endl;
			std::cout << "    System name: " << device->system_name()->str() << std::endl;
			std::cout << "    Product ID: 0x" << std::setfill('0') << std::setw(4) << std::hex
			          << device->product_id() << std::dec << std::endl;
			std::cout << "    Vendor ID: 0x" << std::setfill('0') << std::setw(4) << std::hex
			          << device->vendor_id() << std::dec << std::endl;
			std::cout << "    Capabilities:";
			for (const auto cap : *device->capabilites()) {
				std::cout << " " << capability_name(static_cast<DeviceCapability>(cap));
			}
			std::cout << std::endl;
			std::cout << "    Width: " << device->mm_width() << " mm" << std::endl;
			std::cout << "    Height: " << device->mm_height() << " mm" << std::endl;
			std::cout << "    Touchscreen finger count: " << device->touch_count() << std::endl;
			std::cout << "    Touchpad tap finger count: " << device->tap_finger_count() << std::endl;
			std::cout << "    Tap-to-click by default: " << device->tap_click_default() << std::endl;
			std::cout << "    Tap-to-click active: " << device->tap_click() << std::endl;
			std::cout << "    Tap-to-click button map: "
			          << button_map_name(device->tap_click_button_map()) << std::endl;
			std::cout << "    Tap-to-click default button map: "
			          << button_map_name(device->tap_click_button_map_default()) << std::endl;
			std::cout << "    Tap dragging by default: " << device->tap_drag_default() << std::endl;
			std::cout << "    Tap dragging active: " << device->tap_drag() << std::endl;
			std::cout << "    Drag lock by default: " << device->drag_lock_default() << std::endl;
			std::cout << "    Drag lock active: " << device->drag_lock() << std::endl;
			std::cout << "    Device mode: " << send_events_mode_name(device->send_events_mode())
			          << std::endl;
			std::cout << "    Default device mode: "
			          << send_events_mode_name(device->send_events_mode_default()) << std::endl;
			std::cout << "    Acceleration speed: " << device->acceleration_speed() << std::endl;
			std::cout << "    Default acceleration speed: " << device->acceleration_speed_default()
			          << std::endl;
			std::cout << "    Acceleration profile: "
			          << accel_profile_name(device->acceleration_profile()) << std::endl;
			std::cout << "    Default acceleration profile: "
			          << accel_profile_name(device->acceleration_profile_default()) << std::endl;
			std::cout << "    Natural scrolling available: " << device->natural_scrolling_available()
			          << std::endl;
			std::cout << "    Natural scrolling active: " << device->natural_scrolling() << std::endl;
			std::cout << "    Left-handed mode available: " << device->left_handed_mode_available()
			          << std::endl;
			std::cout << "    Left-handed mode by default: " << device->left_handed_mode_default()
			          << std::endl;
			std::cout << "    Left-handed mode active: " << device->left_handed_mode() << std::endl;
			std::cout << "    Click methods:";
			for (const auto method : *device->available_click_methods()) {
				std::cout << " " << click_method_name(static_cast<ClickMethod>(method));
			}
			std::cout << std::endl;
			std::cout << "    Default click method: " << click_method_name(device->click_method_default())
			          << std::endl;
			std::cout << "    Click method: " << click_method_name(device->click_method()) << std::endl;
			std::cout << "    Middle click emulation available: " << device->middle_emulation_available()
			          << std::endl;
			std::cout << "    Middle click emulation by default: " << device->middle_emulation_default()
			          << std::endl;
			std::cout << "    Middle click emulation active: " << device->middle_emulation() << std::endl;
			std::cout << "    Scroll methods:";
			for (const auto method : *device->available_scroll_methods()) {
				std::cout << " " << scroll_method_name(static_cast<ScrollMethod>(method));
			}
			std::cout << std::endl;
			std::cout << "    Default scroll method: "
			          << scroll_method_name(device->scroll_method_default()) << std::endl;
			std::cout << "    Scroll method: " << scroll_method_name(device->scroll_method())
			          << std::endl;
			std::cout << "    Disable while typing available: "
			          << device->disable_while_typing_available() << std::endl;
			std::cout << "    Disable while typing by default: " << device->disable_while_typing_default()
			          << std::endl;
			std::cout << "    Disable while typing active: " << device->disable_while_typing()
			          << std::endl;
			std::cout << "    Rotation available: " << device->rotation_available() << std::endl;
			std::cout << "    Rotation by default: " << device->rotation_degrees_cw_default()
			          << " ° clockwise" << std::endl;
			std::cout << "    Rotation: " << device->rotation_degrees_cw() << " ° clockwise" << std::endl;
			std::cout << std::endl;
		}

		std::cout << "Outputs [" << state->outputs()->size() << "]:" << std::endl;
		for (const auto output : *state->outputs()) {
			std::cout << "  ID: " << output->id() << std::endl;
			std::cout << "  Name: " << output->name()->str() << std::endl;
			std::cout << "  Position: " << output->x() << ", " << output->y() << std::endl;
			std::cout << "  Size: " << output->width() << " x " << output->height() << std::endl;
			std::cout << "  Current scale: " << output->current_scale() << std::endl;
			std::cout << "  Original scale: " << output->original_scale() << std::endl;
			std::cout << std::endl;
		}

		std::cout << "Heads [" << state->heads()->size() << "]:" << std::endl;
		for (const auto head : *state->heads()) {
			std::cout << "  Name: " << head->name()->str() << std::endl;
			std::cout << "  Belongs to output ID: " << head->output_id() << std::endl;
			std::cout << "  Physical size: " << head->mm_width() << " mm x " << head->mm_height() << " mm"
			          << std::endl;
			std::cout << "  Make: " << head->make()->str() << std::endl;
			std::cout << "  Model: " << head->model()->str() << std::endl;
			std::cout << "  Serial number: " << head->serial_number()->str() << std::endl;
			std::cout << "  Subpixel: " << head->subpixel() << std::endl;
			std::cout << "  Internal: " << head->connection_internal() << std::endl;
			std::cout << "  Connected: " << head->connected() << std::endl;
			std::cout << "  Non-desktop (VR): " << head->non_desktop() << std::endl;
			std::cout << std::endl;
		}

		std::cout << "Surfaces [" << state->surfaces()->size() << "]:" << std::endl;
		for (const auto surface : *state->surfaces()) {
			std::cout << "  UID: " << surface->uid() << std::endl;
			std::cout << "  Label: " << surface->label()->str() << std::endl;
			std::string role = "-UNKNOWN-";
			if (surface->role() == Role_XdgToplevel) {
				role = "application window (xdg-toplevel)";
			} else if (surface->role() == Role_Lsh) {
				role = "system UI (layer-shell)";
			} else if (surface->other_role()->str().size() > 0) {
				role = surface->other_role()->str();
			}
			std::cout << "  Role: " << role << std::endl;
			if (surface->desktop()) {
				std::cout << "  Desktop surface data:" << std::endl;
				auto dsurf = surface->desktop();
				std::cout << "    Title: " << dsurf->title()->str() << std::endl;
				std::cout << "    App ID: " << dsurf->app_id()->str() << std::endl;
				std::cout << "    PID: " << dsurf->pid() << std::endl;
				std::cout << "    Activated: " << dsurf->activated() << std::endl;
				std::cout << "    Maximized: " << dsurf->maximized() << std::endl;
				std::cout << "    Fullscreen: " << dsurf->fullscreen() << std::endl;
				std::cout << "    Resizing: " << dsurf->resizing() << std::endl;
				std::cout << "    Max size: " << dsurf->max_width() << " x " << dsurf->max_height()
				          << std::endl;
				std::cout << "    Min size: " << dsurf->min_width() << " x " << dsurf->min_height()
				          << std::endl;
			}
			std::cout << std::endl;
		}

		std::cout << "--------" << std::endl;
		std::cout << std::endl;
	}
	close(recv_fd);
	updates_recvd++;
}

static const struct wldip_compositor_manager_listener shooter_listener = {on_update};

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
		std::cerr << "failed to find manager interface" << std::endl;
		return -1;
	}

	wldip_compositor_manager_add_listener(shooter, &shooter_listener, nullptr);

	if (argc == 2 && std::string(argv[1]) == "get") {
		wldip_compositor_manager_get(shooter);
		while (updates_recvd < 1) {
			wl_display_dispatch(display);
			wl_display_roundtrip(display);
		}
	} else if (argc == 2 && std::string(argv[1]) == "watch") {
		wldip_compositor_manager_subscribe(shooter, WLDIP_COMPOSITOR_MANAGER_TOPIC_SURFACES |
		                                                WLDIP_COMPOSITOR_MANAGER_TOPIC_OUTPUTS |
		                                                WLDIP_COMPOSITOR_MANAGER_TOPIC_INPUTDEVS);
		while (true) {
			wl_display_dispatch(display);
			wl_display_roundtrip(display);
		}
	} else {
		std::cerr << "Usage: " << argv[0] << " get|watch" << std::endl;
		return -1;
	}
}
