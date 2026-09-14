# SPDX-License-Identifier: GPL-3.0-or-later
# reac-stageboxes — the REAC stagebox preamp application.
Name:           reac-stageboxes
Version:        %{?version_override}%{!?version_override:0.1.0}
Release:        1%{?dist}
Summary:        Manage REAC stagebox preamps (phantom, pad, sensitivity)

License:        GPL-3.0-or-later
URL:            https://github.com/FreeREAC/reac-stageboxes
Source0:        %{name}-%{version}.tar.gz

# find-debuginfo produces an empty debugsourcefiles.list for this single small
# binary under the desk's current toolchain (gcc 16, fat-LTO objects) and rpm
# then refuses to build an empty debugsource subpackage. Not a code defect —
# skip the split debuginfo package rather than carry a build that cannot
# finish; revisit if the toolchain moves.
%global debug_package %{nil}

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
# Validates the AppStream metainfo at build time, the same way
# desktop-file-validate proves the .desktop file.
BuildRequires:  appstream

# reac-pw is what makes this application do anything: it publishes the nodes
# this reads and writes, and >= 1.0.5 is the first version this was built and
# proven against. A REQUIRES and not a RECOMMENDS — installed alone, this
# window would have nothing to show and no door to write to.
Requires:       reac-pw >= 1.0.5
Requires:       pipewire
Requires:       hicolor-icon-theme

# The icon cache is a system-wide index gtk-update-icon-cache maintains over
# %%{_datadir}/icons/hicolor; every package that drops a file under it must
# refresh that cache the same way, so the icon is found the moment this
# package lands and is retired the moment it leaves.
Requires(post):   gtk-update-icon-cache
Requires(postun): gtk-update-icon-cache

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

%post
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache -q %{_datadir}/icons/hicolor &>/dev/null || :
fi

%postun
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache -q %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache -q %{_datadir}/icons/hicolor &>/dev/null || :
fi

%files -f %{name}.lang
%license LICENSE
%doc README.md
%{_bindir}/reac-stageboxes
%{_datadir}/applications/org.freereac.Stageboxes.desktop
%{_datadir}/metainfo/org.freereac.Stageboxes.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/org.freereac.Stageboxes.svg
%{_datadir}/icons/hicolor/symbolic/apps/org.freereac.Stageboxes-symbolic.svg

%changelog
* Mon Sep 14 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First package.
