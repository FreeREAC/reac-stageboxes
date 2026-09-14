# reac-stageboxes

**REAC Stageboxes** — a GTK4/libadwaita application that sets phantom power, the
pad and the sensitivity on every input of every REAC stagebox on the wire.

## What it is

One window. The sidebar lists every REAC segment in the PipeWire graph — box
model, segment, firmware and REAC protocol version, and whether the link is
established. The main pane is one row per input of the selected box: a phantom
switch, a pad switch, and sensitivity as a slider in dBu across the travel the
node publishes.

**It is a PipeWire client, not a REAC one.** reac-pw's master node takes
per-input head-amp changes as `SPA_PARAM_Props` under the keys
`reac.headamp.<wire channel>.{phantom,pad,sens}`, and publishes the box badge,
the preamp shape, the head-amp base and the asserted state as node properties on
that same node. This application binds the node, renders those properties and
writes those params. There is no REAC protocol code here, no socket, no REST, no
configuration file and no second copy of any fact the daemon holds. Installing
`reac-pw` is the whole dependency.

**A control that cannot move says why.** A change is written and then read back
from the daemon's own published state; a refusal arrives as a code and is shown
on the row, with the control snapped back to what is actually asserted. Where
the capability is absent the page carries the reason instead of a dead switch:

- a box strapped to **REAC master mode** takes no preamp control over the wire in
  either direction — its preamps are set on the box, over its serial port;
- a segment **mastered by somebody else** has no control door at all, because the
  daemon builds its control node only in the master role;
- **no box**, or a box that has announced **no head-amp base**, means there is no
  wire address, and guessing one would move a different box's preamps silently;
- a daemon that publishes **no head-amp readback** gets read-only rows, because a
  change that cannot be confirmed must not be drawn as applied.

Sensitivity is shown in **dBu**, the unit the protocol carries:
`-10 - step + (pad ? 20 : 0)`, a flat 1 dB per step. Sensitivity runs the other
way from gain — the hottest setting is the most negative number — and it is not
the same number a mixing desk shows for the same preamp.

Target: Fedora + PipeWire 1.4, GTK 4.10+, libadwaita 1.4+.

## Install

**From source.**

```
sudo dnf install meson ninja-build gcc gettext desktop-file-utils \
                 pipewire-devel gtk4-devel libadwaita-devel
meson setup build
meson compile -C build
meson test -C build
sudo meson install -C build
```

**From an RPM.** `packaging/reac-stageboxes.spec` builds the package; it
`Requires: reac-pw`, which is what publishes the nodes this reads.

## Usage

Start `reac-stageboxes`, or launch **REAC Stageboxes** from the desktop. Nothing
is configured: the application watches the PipeWire registry, and a segment
appears in the sidebar as soon as reac-pw publishes it and disappears when it
goes. Pick a box; set its inputs.

`reac-stageboxes --list` prints the same walk with no window: every segment it
finds, its badge and capability, and each input's asserted values. It only
reads. It exits 0 when it found at least one segment and 1 when it found none,
and it prints how many nodes it bound either way — because "no stageboxes" and
"the walk saw nothing" are different facts that look identical in an empty list.

```
$ reac-stageboxes --list
3 REAC segments (27 nodes bound)

segment enp131s0.11      door=sink   model=s1608          width=16x8   link=established
        base=32 channels=16 caps=phantom,pad,sens sens.max=55
        mac=00:40:ab:c4:80:3b fw=2.200 reac=2.302 master=us refused=none readback=yes -> ok
          in  1  ch  32  phantom=on  pad=off sens=32 (-42 dBu)
          ...
```

Anyone in the session can write, because reac-pw is a user service on the user's
PipeWire socket — the same permission any application has over any device's
volume.

## Translations

English and Catalan (`po/`). The interface and `--list` both go through the same
catalogue, so a Catalan session gets Catalan in the window and in the terminal.

A binary run from the build tree finds the catalogues meson built beside it, so
`LANGUAGE=ca b/reac-stageboxes --list` is Catalan without installing anything.
`REAC_STAGEBOXES_LOCALEDIR` overrides the search when you want to point it at
another tree; an installed binary falls back to its configured `localedir`.

Adding a string means wrapping it in `_()` and translating it in every
catalogue: `meson test` fails otherwise, both on a string that never reached the
template and on a template entry no catalogue translates.

## Licence

GPL-3.0-or-later. Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>.
