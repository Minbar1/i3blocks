# i3blocks - with support for FreeBSD

[![Build Status](https://travis-ci.org/vivien/i3blocks.svg?branch=master)](https://travis-ci.org/vivien/i3blocks)
[![Join the chat at https://gitter.im/vivien/i3blocks](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/vivien/i3blocks?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This is a fork of [i3blocks](https://github.com/vivien/i3blocks) that add
support for FreeBSD.

i3blocks is a highly flexible **status line** for the [i3](http://i3wm.org)
window manager. It handles *clicks*, *signals* and *language-agnostic* user
*scripts*.

The content of each *block* (e.g. time, battery status, network state, ...) is
the output of a *command* provided by the user. Blocks are updated on *click*,
at a given *interval* of time or on a given *signal*, also specified by the
user.

It aims to respect the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html), providing
customization such as text alignment, urgency, color, and more.

- - -

Here is an example of status line, showing the time updated every 5 seconds,
the volume updated only when i3blocks receives a SIGRTMIN+10, and click events.
Please note that this example will not work under OpenBSD since only SIGUSR1 and
SIGUSR2 signals are supported.

```` ini
[volume]
label=Volume:
command=amixer get Master | grep -E -o '[0-9]{1,3}?%' | head -1
interval=once
signal=10
# use 'pkill -RTMIN+10 i3blocks' after changing the volume

[time]
command=date '+%D %T'
interval=5

[clickme]
full_text=Click me!
command=echo button=$BLOCK_BUTTON x=$BLOCK_X y=$BLOCK_Y
min_width=button=1 x=1366 y=768
align=left
````

You can use your own scripts, or the
[ones](https://github.com/Minbar1/i3blocks/tree/master/scripts) provided with
i3blocks. Feel free to contribute and improve them!

The default config will look like this:

![](http://i.imgur.com/p3d6MeK.png)

The scripts provided by default may use external tools:

  * `mpstat` (often provided by the *sysstat* package) used by `cpu_usage`.
  * `acpi` (often provided by a package of the same name) used by `battery`.

## Documentation

For more information about how it works, please refer to the
[**manpage**](https://github.com/Minbar1/i3blocks/blob/bsd/i3blocks.1.md).

You can also take a look at the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html) to see what
possibilities it offers you.

## Installation

i3blocks may already be packaged for your distribution:

  * Archlinux: [i3blocks](https://aur.archlinux.org/packages/i3blocks) and
  [i3blocks-git](https://aur.archlinux.org/packages/i3blocks-git) AURs.
  * Gentoo: [ebuild](https://github.com/Sabayon-Labs/spike-community-overlay/tree/master/x11-misc/i3blocks)
  * Debian: [i3blocks](https://packages.debian.org/i3blocks) and Ubuntu: [i3blocks](http://packages.ubuntu.com/i3blocks)

For *BSD a port is on its way. You may also install i3blocks from source:

    $ git clone git://github.com/Minbar1/i3blocks
    $ cd i3blocks
    $ make clean debug # or make clean all
    # make install

Note: the generation of the manpage in Linux requires the `pandoc` utility, packaged in
common distributions as `pandoc`.

### Usage

  * Set your `status_command` in a bar block of your i3 config file:

        bar {
          status_command     i3blocks -c ~/.config/i3/i3blocks.conf
        }

  * For customization, copy the default i3blocks.conf into ~/.i3blocks.conf
    (e.g. `cp /etc/i3blocks.conf ~/.i3blocks.conf`)
  * Restart i3 (e.g. `i3-msg restart`)

## Copying

i3blocks is Copyright (C) 2014 Vivien Didelot<br />
See the file COPYING for information of licensing and distribution.
