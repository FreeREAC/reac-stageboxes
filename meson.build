# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# reac-stageboxes — a GTK4/libadwaita application that manages every REAC
# stagebox's preamps with nothing but reac-pw installed.
#
# It is a PIPEWIRE client and not a REAC one: it links libpipewire-0.3 and
# libspa-0.2 and NOTHING ELSE from the REAC stack. Every protocol fact it needs
# — the preamp count, the capability tokens, the head-amp base, the box model,
# the firmware, the REAC version, the link state — is a property reac-pw already
# publishes on the node that carries reac.segment. Linking libreac here would
# be a second copy of a decision the daemon already made.

project('reac-stageboxes', 'c',
  version : '0.1.0',
  license : 'GPL-3.0-or-later',
  meson_version : '>= 0.60',
  default_options : ['c_std=c11', 'warning_level=2', 'buildtype=debugoptimized'])

cc = meson.get_compiler('c')

# SPA's inline string.h uses newlocale()/uselocale()/locale_t, which are GNU/XPG
# extensions — define _GNU_SOURCE for every TU that includes a PipeWire/SPA
# header, the way PipeWire itself builds.
add_project_arguments('-D_GNU_SOURCE', language : 'c')

app_id = 'org.freereac.Stageboxes'
gettext_package = meson.project_name()

prefix     = get_option('prefix')
localedir  = prefix / get_option('localedir')

config_args = [
  '-DAPP_ID="@0@"'.format(app_id),
  '-DGETTEXT_PACKAGE="@0@"'.format(gettext_package),
  '-DLOCALEDIR="@0@"'.format(localedir),
]

gtk_dep      = dependency('gtk4', version : '>= 4.10')
adw_dep      = dependency('libadwaita-1', version : '>= 1.4')  # AdwSwitchRow, AdwToolbarView,
                                                               # AdwNavigationSplitView
gio_dep      = dependency('gio-2.0')
gobject_dep  = dependency('gobject-2.0')
glib_dep     = dependency('glib-2.0')
pipewire_dep = dependency('libpipewire-0.3', version : '>= 0.3.65')
spa_dep      = dependency('libspa-0.2')

# --- the pure layer, shared by the app and the tests ------------------------
# rs-headamp is the head-amp vocabulary (key grammar, wire address, the dBu
# curve, the asserted cell list, the availability rule) with no PipeWire and no
# GTK in it; rs-props builds the one pod this application writes and needs only
# SPA's header-only pod builder. Both are compiled into the tests directly, so
# `meson test` drives them offline.
headamp_sources = files('src/rs-headamp.c')
props_sources   = files('src/rs-props.c')
box_sources     = files('src/rs-box.c')
walk_sources    = files('src/rs-walk.c')

app_sources = headamp_sources + props_sources + box_sources + walk_sources + files(
  'src/main.c',
  'src/rs-list.c',
  'src/rs-pw.c',
  'src/rs-window.c',
)

executable('reac-stageboxes',
  app_sources,
  c_args : config_args,
  include_directories : include_directories('src'),
  dependencies : [gtk_dep, adw_dep, gio_dep, gobject_dep, glib_dep,
                  pipewire_dep, spa_dep],
  install : true)

subdir('data')
subdir('po')
subdir('tests')
