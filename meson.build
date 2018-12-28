project('weston-extra-dip', ['c', 'cpp'], version: '0.0.1', license: 'Unlicense',
	default_options: ['cpp_std=c++17', 'warning_level=2'])

cxx = meson.get_compiler('cpp')

dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))
wl_code = generator(prog_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'])
wl_server_header = generator(prog_scanner,
	output: '@BASENAME@-server-protocol.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wl_client_header = generator(prog_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'])

layered_screenshot_code = wl_code.process('protocols/wldip-layered-screenshooter.xml')
layered_screenshot_server_header = wl_server_header.process('protocols/wldip-layered-screenshooter.xml')
layered_screenshot_client_header = wl_client_header.process('protocols/wldip-layered-screenshooter.xml')

compositor_management_code = wl_code.process('protocols/wldip-compositor-manager.xml')
compositor_management_server_header = wl_server_header.process('protocols/wldip-compositor-manager.xml')
compositor_management_client_header = wl_client_header.process('protocols/wldip-compositor-manager.xml')

gamma_control_code = wl_code.process('protocols/wlr-gamma-control-unstable-v1.xml')
gamma_control_server_header = wl_server_header.process('protocols/wlr-gamma-control-unstable-v1.xml')
gamma_control_client_header = wl_client_header.process('protocols/wlr-gamma-control-unstable-v1.xml')

layer_shell_code = wl_code.process('protocols/wlr-layer-shell-unstable-v1.xml')
layer_shell_server_header = wl_server_header.process('protocols/wlr-layer-shell-unstable-v1.xml')
layer_shell_client_header = wl_client_header.process('protocols/wlr-layer-shell-unstable-v1.xml')

xdg_shell_code = wl_code.process('/usr/local/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml')
xdg_shell_server_header = wl_server_header.process('/usr/local/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml')
xdg_shell_client_header = wl_client_header.process('/usr/local/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml')

prog_flatc = find_program('flatc')
flatc = generator(prog_flatc,
	output: '@BASENAME@_generated.h',
	arguments: ['--cpp', '-o', '@BUILD_DIR@', '@EXTRA_ARGS@', '@INPUT@'])

layered_screenshot_fb = flatc.process('schemas/Screenshot.fbs')
compositor_management_fb = flatc.process('schemas/Management.fbs')

# Why don't they have a pkg-config file?!?!?
lib_flatbuffers = cxx.find_library('libflatbuffers', dirs: ['/usr/local/lib', '/usr/lib'])
inc_flatbuffers = include_directories('/usr/local/include', '/usr/include')

weston = dependency('libweston-5')
weston_desktop = dependency('libweston-desktop-5')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
webp = dependency('libwebp')
libinput = dependency('libinput')


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
	include_directories: [inc_flatbuffers],
	dependencies: [weston, wayland_server, lib_flatbuffers],
	cpp_args: ['-fno-rtti'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

layered_screenshooter = executable('layered-screenshooter',
	'layered-screenshooter.cpp', layered_screenshot_fb, layered_screenshot_code, layered_screenshot_client_header,
	include_directories: [inc_flatbuffers],
	dependencies: [wayland_client, lib_flatbuffers, webp],
	install: true)

compositor_management = shared_module('compositor-management',
	'compositor-management.cpp', compositor_management_fb, compositor_management_code, compositor_management_server_header,
	include_directories: [inc_flatbuffers],
	dependencies: [weston, weston_desktop, wayland_server, libinput, lib_flatbuffers],
	cpp_args: ['-fno-rtti'],
	name_prefix: '',
	install_dir: 'lib/weston',
	install: true)

compositor_manager = executable('compositor-manager',
	'compositor-manager.cpp', compositor_management_fb, compositor_management_code, compositor_management_client_header,
	include_directories: [inc_flatbuffers],
	dependencies: [wayland_client, lib_flatbuffers, webp],
	install: true)
