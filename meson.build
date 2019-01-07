project('weston-extra-dip', ['c', 'cpp'], version: '0.0.1', license: 'Unlicense',
	default_options: ['cpp_std=c++17', 'warning_level=2'])

cxx = meson.get_compiler('cpp')

subdir('protocols')
subdir('schemas')

weston = dependency('libweston-5')
weston_desktop = dependency('libweston-desktop-5')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
webp = dependency('libwebp')
libinput = dependency('libinput')
flatbuffers = dependency('Flatbuffers', method: 'cmake', modules: ['flatbuffers::flatbuffers_shared'])

install_headers('weston-extra-dip-capabilities-api.h')

capabilities = shared_module('capabilities',
	'capabilities.cpp', capabilities_code, capabilities_server_header,
	dependencies: [weston, wayland_server],
	cpp_args: ['-fno-rtti', '-fno-exceptions'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

key_modifier_binds = shared_module('key-modifier-binds',
	'key-modifier-binds.cpp',
	dependencies: [weston, wayland_server],
	cpp_args: ['-fno-rtti', '-fno-exceptions'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

gamma_control = shared_module('gamma-control',
	'gamma-control.cpp', gamma_control_code, gamma_control_server_header,
	dependencies: [weston, wayland_server],
	cpp_args: ['-fno-rtti', '-fno-exceptions'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

layer_shell = shared_module('layer-shell',
	'layer-shell.cpp', layer_shell_code, layer_shell_server_header, xdg_shell_code,
	dependencies: [weston, wayland_server],
	cpp_args: ['-fno-rtti', '-fno-exceptions'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

layered_screenshot = shared_module('layered-screenshot',
	'layered-screenshot.cpp', layered_screenshot_fb, layered_screenshot_code, layered_screenshot_server_header,
	dependencies: [weston, wayland_server, flatbuffers],
	cpp_args: ['-fno-rtti'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

layered_screenshooter = executable('layered-screenshooter',
	'layered-screenshooter.cpp', layered_screenshot_fb, layered_screenshot_code, layered_screenshot_client_header,
	dependencies: [wayland_client, flatbuffers, webp],
	install: true)

compositor_management = shared_module('compositor-management',
	'compositor-management.cpp', compositor_management_fb, compositor_management_code, compositor_management_server_header,
	dependencies: [weston, weston_desktop, wayland_server, libinput, flatbuffers],
	cpp_args: ['-fno-rtti'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

compositor_manager = executable('compositor-manager',
	'compositor-manager.cpp', compositor_management_fb, compositor_management_code, compositor_management_client_header,
	dependencies: [wayland_client, flatbuffers, webp],
	install: true)

all_srcs = [
	'weston-extra-dip-capabilities-api.h',
	'capabilities.cpp',
	'key-modifier-binds.cpp',
	'gamma-control.cpp',
	'layer-shell.cpp',
	'layered-screenshot.cpp',
	'layered-screenshooter.cpp',
	'compositor-management.cpp',
	'compositor-manager.cpp',
]

prog_clang_format = find_program('clang-format80', 'clang-format70', 'clang-format60', 'clang-format', required: false)
if prog_clang_format.found()
	run_target('fmt', command: [ prog_clang_format, '-i', all_srcs ])
endif

prog_clang_tidy = find_program('clang-tidy80', 'clang-tidy70', 'clang-tidy60', 'clang-tidy', required: false)
if prog_clang_tidy.found()
	run_target('tidy', command: [ prog_clang_tidy, all_srcs ])
	run_target('fix',  command: [ prog_clang_tidy, '-fix-errors', '-fix', all_srcs ])
endif

prog_cppcheck = find_program('cppcheck', required: false)
if prog_cppcheck.found()
	run_target('check', command: [ prog_cppcheck, all_srcs ])
endif
