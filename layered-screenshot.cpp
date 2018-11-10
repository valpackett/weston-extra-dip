#include <compositor.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Screenshot_generated.h"
#include "wldip-layered-screenshooter-server-protocol.h"

struct ls_context {
	const struct weston_compositor *compositor;

	ls_context(const struct weston_compositor *c) : compositor(c) {}

	ls_context(ls_context &&) = delete;
};

static void shoot(struct wl_client *client, struct wl_resource *resource) {
	using namespace wldip::layered_screenshot;
	auto *ctx = reinterpret_cast<struct ls_context *>(wl_resource_get_user_data(resource));
	std::unordered_map<struct weston_layer *, std::vector<struct weston_view *>> layers;
	struct weston_view *view;
	wl_list_for_each(view, &ctx->compositor->view_list, link) {
		layers[view->layer_link.layer].push_back(view);
	}
	flatbuffers::FlatBufferBuilder builder(32768);
	std::vector<flatbuffers::Offset<Layer>> flayers;
	for (const auto &kv : layers) {
		std::vector<flatbuffers::Offset<Surface>> fsurfs;
		for (const auto *view : kv.second) {
			uint8_t *buf = nullptr;
			int cw = 0, ch = 0;
			weston_surface_get_content_size(view->surface, &cw, &ch);
			// size_t len = view->surface->width * view->surface->height * 4;
			size_t len = cw * ch * 4;
			auto contents = builder.CreateUninitializedVector<uint8_t>(len, &buf);
			int ccr = weston_surface_copy_content(view->surface, reinterpret_cast<void *>(buf), len, 0, 0,
			                                      cw, ch);
			fsurfs.push_back(CreateSurface(builder, view->geometry.x, view->geometry.y, cw, ch,
			                               Layout_Pixman_A8B8G8R8, contents));
		}
		flayers.push_back(CreateLayer(builder, builder.CreateVector(fsurfs), 0));
	}
	// TODO find size
	builder.Finish(CreateScreenshot(builder, 1366, 768, builder.CreateVector(flayers)));
	int fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0644);
	ftruncate(fd, builder.GetSize());
	write(fd, builder.GetBufferPointer(), builder.GetSize());
	lseek(fd, 0, SEEK_SET);
	wldip_layered_screenshooter_send_done(resource, fd);
	close(fd);
}

static struct wldip_layered_screenshooter_interface ls_impl = {shoot};

static void bind_shooter(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource =
	    wl_resource_create(client, &wldip_layered_screenshooter_interface, 1, id);
	// TODO privilege check
	wl_resource_set_implementation(resource, &ls_impl, data, nullptr);
}

WL_EXPORT extern "C" int wet_module_init(struct weston_compositor *compositor, int *argc,
                                         char *argv[]) {
	auto ctx = new ls_context(compositor);
	wl_global_create(compositor->wl_display, &wldip_layered_screenshooter_interface, 1,
	                 reinterpret_cast<void *>(ctx), bind_shooter);
	return 0;
}
