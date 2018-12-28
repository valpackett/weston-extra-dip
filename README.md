[![unlicense](https://img.shields.io/badge/un-license-green.svg?style=flat)](https://unlicense.org)

# Weston with Extra Dip

A pack of plugins that add extra functionality to Weston, the reference Wayland compositor.
Designed for the [numbernine] desktop environment.

[numbernine]: https://github.com/myfreeweb/numbernine

- `layer-shell`: implements `wlr_layer_shell_unstable_v1` (well, not completely..)
- `gamma-control`: implements `wlr_gamma_control_unstable_v1`, e.g. for [this fork of redshift](https://github.com/minus7/redshift/tree/wayland)
- `layered-screenshot`: dumps surface contents as separate images, included `layered-screenshooter` for now just writes them as separate webp images (but in the future there might be a cool screenshot editor..)
- `key-modifier-binds`: [xcape](https://github.com/alols/xcape) style key binds, currently hardcoded to CapsLock (scancode, no matter if you rebind to Ctrl or not) as Escape and Shifts as Parens
- `compositor-management`: notifies a manager (desktop environment) of compositor state changes, executes the manager's commands

## Installation

Requires FlatBuffers, wayland-scanner, `libweston-5` (Weston git master).

```shell
meson build
ninja -Cbuild install
```

Or e.g. If you have Weston in `~/.local`:

```shell
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig
meson build --prefix $HOME/.local
ninja -Cbuild install
```

## Usage

Add to `weston.ini` and restart Weston.

```ini
[core]
modules=key-modifier-binds.so.1,gamma-control.so.1,layered-screenshot.so.1,layer-shell.so.1
```

## Contributing

By participating in this project you agree to follow the [Contributor Code of Conduct](https://contributor-covenant.org/version/1/4/).

[The list of contributors is available on GitHub](https://github.com/myfreeweb/weston-extra-dip/graphs/contributors).

## License

This is free and unencumbered software released into the public domain.  
For more information, please refer to the `UNLICENSE` file or [unlicense.org](http://unlicense.org).
