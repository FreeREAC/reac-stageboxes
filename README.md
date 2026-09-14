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

Anyone in the session can write, because reac-pw is a user service on the user's
PipeWire socket — the same permission any application has over any device's
volume.

## Licence

GPL-3.0-or-later. Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>.
