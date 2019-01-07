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

# local protos

capabilities_code = wl_code.process('wldip-capabilities.xml')
capabilities_server_header = wl_server_header.process('wldip-capabilities.xml')
capabilities_client_header = wl_client_header.process('wldip-capabilities.xml')

layered_screenshot_code = wl_code.process('wldip-layered-screenshooter.xml')
layered_screenshot_server_header = wl_server_header.process('wldip-layered-screenshooter.xml')
layered_screenshot_client_header = wl_client_header.process('wldip-layered-screenshooter.xml')

compositor_management_code = wl_code.process('wldip-compositor-manager.xml')
compositor_management_server_header = wl_server_header.process('wldip-compositor-manager.xml')
compositor_management_client_header = wl_client_header.process('wldip-compositor-manager.xml')

gamma_control_code = wl_code.process('wlr-gamma-control-unstable-v1.xml')
gamma_control_server_header = wl_server_header.process('wlr-gamma-control-unstable-v1.xml')
gamma_control_client_header = wl_client_header.process('wlr-gamma-control-unstable-v1.xml')

layer_shell_code = wl_code.process('wlr-layer-shell-unstable-v1.xml')
layer_shell_server_header = wl_server_header.process('wlr-layer-shell-unstable-v1.xml')
layer_shell_client_header = wl_client_header.process('wlr-layer-shell-unstable-v1.xml')

# system protos

dep_protos = dependency('wayland-protocols')
system_protos_dir = dep_protos.get_pkgconfig_variable('pkgdatadir')

xdg_shell_code = wl_code.process(system_protos_dir / 'stable/xdg-shell/xdg-shell.xml')
xdg_shell_server_header = wl_server_header.process(system_protos_dir / 'stable/xdg-shell/xdg-shell.xml')
xdg_shell_client_header = wl_client_header.process(system_protos_dir / 'stable/xdg-shell/xdg-shell.xml')
