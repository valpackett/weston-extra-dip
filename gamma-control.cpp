#include <vector>

extern "C" {
#include <compositor.h>
#include <sys/stat.h>
#include <unistd.h>
#include "wlr-gamma-control-unstable-v1-server-protocol.h"

static void set_gamma(struct wl_client *client, struct wl_resource *resource, int32_t fd);
static void control_destructor(struct wl_resource *resource);
static void destroy_control(struct wl_client *client, struct wl_resource *resource);

static struct zwlr_gamma_control_v1_interface control_impl = {set_gamma, destroy_control};

struct gamma_context {
	const struct weston_head *head;
	struct wl_resource *resource;

	gamma_context(const struct weston_head *h, struct wl_client *client, uint32_t id) : head(h) {
		resource = wl_resource_create(client, &zwlr_gamma_control_v1_interface, 1, id);
		wl_resource_set_implementation(resource, &control_impl, this, control_destructor);
	}

	~gamma_context() { wl_resource_set_user_data(resource, nullptr); }

	gamma_context(gamma_context &&) = delete;
};

static void set_gamma(struct wl_client *client, struct wl_resource *resource, int32_t fd) {
	auto *ctx = static_cast<struct gamma_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		close(fd);
		return;
	}

	auto ramp_size = ctx->head->output->gamma_size;
	size_t elems = ramp_size * 3;
	off_t bytesize = elems * sizeof(uint16_t);

	struct stat recv_stat {};
	fstat(fd, &recv_stat);
	if (recv_stat.st_size != bytesize) {
		close(fd);
		wl_resource_post_error(resource, ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA,
		                       "fd size is not correct");
		return;
	}
	lseek(fd, 0, SEEK_SET);

	std::vector<uint16_t> table;
	table.resize(elems);

	ssize_t n_read = read(fd, reinterpret_cast<void *>(table.data()), bytesize);
	close(fd);
	if (n_read == -1 || n_read != bytesize) {
		weston_log("gamma control: read fail: read %zd, bytesize %zu\n", n_read, bytesize);
		zwlr_gamma_control_v1_send_failed(ctx->resource);
		delete ctx;
		return;
	}

	ctx->head->output->set_gamma(ctx->head->output, ramp_size, table.data(), table.data() + ramp_size,
	                             table.data() + 2 * ramp_size);
}

static void control_destructor(struct wl_resource *resource) {
	auto *ctx = static_cast<struct gamma_context *>(wl_resource_get_user_data(resource));
	if (ctx == nullptr) {
		return;
	}
	delete ctx;
}

static void destroy_control(struct wl_client *client, struct wl_resource *resource) {
	control_destructor(resource);
}

static void get_gamma_control(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                              struct wl_resource *output) {
	auto *head = static_cast<struct weston_head *>(wl_resource_get_user_data(output));
	auto *ctx = new gamma_context(head, client, id);

	if (ctx->head->output->set_gamma == nullptr) {
		weston_log("gamma control: no set_gamma function\n");
		zwlr_gamma_control_v1_send_failed(ctx->resource);
		delete ctx;
		return;
	}

	// TODO subscribe to output destroy
	// TODO check uniqueness

	zwlr_gamma_control_v1_send_gamma_size(ctx->resource, ctx->head->output->gamma_size);
}

static void destroy_manager(struct wl_client *client, struct wl_resource *resource) {}

static struct zwlr_gamma_control_manager_v1_interface manager_impl = {get_gamma_control,
                                                                      destroy_manager};

static void bind_manager(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource =
	    wl_resource_create(client, &zwlr_gamma_control_manager_v1_interface, 1, id);
	// TODO privilege check
	wl_resource_set_implementation(resource, &manager_impl, data, nullptr);
}

WL_EXPORT int wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[]) {
	wl_global_create(compositor->wl_display, &zwlr_gamma_control_manager_v1_interface, 1, nullptr,
	                 bind_manager);
	return 0;
}
}
