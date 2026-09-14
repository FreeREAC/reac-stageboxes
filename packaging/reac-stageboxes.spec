# SPDX-License-Identifier: GPL-3.0-or-later
# reac-stageboxes — the REAC stagebox preamp application.
Name:           reac-stageboxes
Version:        %{?version_override}%{!?version_override:0.1.0}
Release:        1%{?dist}
Summary:        Manage REAC stagebox preamps (phantom, pad, sensitivity)

License:        GPL-3.0-or-later
URL:            https://github.com/FreeREAC/reac-stageboxes
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  meson >= 0.60
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  desktop-file-utils
# The i18n test runs the built binary under LANGUAGE=ca and requires Catalan out
# of it. gettext IGNORES LANGUAGE when the locale is C, so without this langpack
# the test cannot observe anything and skips — a build that reports success
# while proving nothing about the translation it ships.
BuildRequires:  glibc-langpack-ca
BuildRequires:  pipewire-devel
BuildRequires:  gtk4-devel
BuildRequires:  libadwaita-devel
BuildRequires:  pkgconfig(libpipewire-0.3)
BuildRequires:  pkgconfig(libspa-0.2)
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(libadwaita-1)

# reac-pw is what makes this application do anything: it publishes the nodes
# this reads and writes. A REQUIRES and not a RECOMMENDS — installed alone,
# this window would have nothing to show and no door to write to.
Requires:       reac-pw
Requires:       pipewire

%description
A GTK4/libadwaita application that sets phantom power, the pad and the
sensitivity on every input of every REAC stagebox on the wire.

It is a PipeWire client and nothing else. reac-pw's master node takes per-input
head-amp changes as SPA_PARAM_Props under the keys
reac.headamp.<wire channel>.{phantom,pad,sens}, and publishes the box badge,
the preamp shape and the asserted state as node properties on the same node.
This application binds that node and reads and writes it. There is no REAC
protocol code here, no socket, no configuration file and no second copy of any
fact the daemon already holds — installing reac-pw is enough.

Where the capability is not there, the page says which: a box strapped to REAC
master mode takes no preamp control over the wire in either direction and its
rows carry that sentence rather than a dimmed switch; a segment mastered by
somebody else has no control door at all; and a daemon that publishes no
head-amp readback gets read-only rows, because a change that cannot be
confirmed must not be shown as applied.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install
%find_lang %{name}

%check
%meson_test

%files -f %{name}.lang
%license LICENSE
%doc README.md
%{_bindir}/reac-stageboxes
%{_datadir}/applications/org.freereac.Stageboxes.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.freereac.Stageboxes.svg

%changelog
* Mon Sep 14 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First package.
