#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <webp/encode.h>
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

static void on_done(void *data, struct wldip_layered_screenshooter *shooter, int recv_fd) {
	using namespace wldip::layered_screenshot;
	received = true;
	//lseek(recv_fd, 0, SEEK_SET);
	struct stat recv_stat;
	fstat(recv_fd, &recv_stat);
	void *fbuf = mmap(nullptr, recv_stat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, recv_fd, 0);
	auto fshot = GetScreenshot(fbuf);
	int counter = 0;
	for (const auto *layer : *fshot->layers()) {
		std::cout << "Layer" << std::endl;
		for (const auto *surface : *layer->surfaces()) {
			uint8_t *buf = const_cast<uint8_t*>(surface->contents()->Data());
			// NOTE: pixman big-endian bgra == little endian rgba, don't touch
			std::cout << "Surface " << counter << " w=" << surface->width() << " h=" << surface->height()
				<< " x=" << surface->x() << " y=" << surface->y()
				<< " buf " << buf[0] << buf[1] << buf[2] << buf[3]
				<< std::endl;
			uint8_t *webpbuf = nullptr;
			size_t webpsiz = WebPEncodeRGBA(buf, surface->width(), surface->height(), surface->width() * 4, 98, &webpbuf);
			char *fname = nullptr;
			asprintf(&fname, "surface-%d.webp", counter++);
			std::ofstream ofs(fname, std::ofstream::binary);
			ofs.write(reinterpret_cast<char*>(webpbuf), webpsiz);
		}
	}
	close(recv_fd);
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
		wl_display_dispatch(display);
		wl_display_roundtrip(display);
	}
}
