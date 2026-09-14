# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Three offline tests. None of them needs a PipeWire session, a graph or a box.

# The param-name mapping and the dB/wire-value conversion. The expected key
# strings and dB figures are written out longhand, because a key this
# application misspells is not an error on a live desk — reac-pw's parse skips
# what it does not recognise and set_param still returns success.
test_headamp_map = executable('test_headamp_map',
  ['test_headamp_map.c'] + headamp_sources,
  include_directories : include_directories('../src'))
test('headamp-map', test_headamp_map)

# The SPA pod a write actually carries, taken apart with the same iteration
# reac-pw's parser uses.
test_props_pod = executable('test_props_pod',
  ['test_props_pod.c'] + props_sources + headamp_sources,
  include_directories : include_directories('../src'),
  dependencies : [spa_dep])
test('props-pod', test_props_pod)

# The model, driven by a synthetic node property dict — the literal type a
# pw_node_info event carries. This is the mocked-PipeWire tier: it covers the
# whole read path with no daemon. The registry walk, the proxy binding and the
# set_param round trip are NOT covered here and are proven on the desk.
test_box_model = executable('test_box_model',
  ['test_box_model.c'] + box_sources + headamp_sources,
  include_directories : include_directories('../src'),
  dependencies : [glib_dep, gobject_dep, spa_dep])
test('box-model', test_box_model)

# The registry walk, driven by a FAKE REGISTRY whose `global` events carry only
# the subset a real one carries — node.name, media.class, object.serial — with
# every reac.* key arriving in the node's `info` event after the bind. This is
# the regression: filtering on reac.segment at global time found zero segments
# on a graph carrying twelve.
test_registry_walk = executable('test_registry_walk',
  ['test_registry_walk.c'] + walk_sources + box_sources + headamp_sources,
  include_directories : include_directories('../src'),
  dependencies : [glib_dep, gobject_dep, spa_dep])
test('registry-walk', test_registry_walk)

# EVERY USER-VISIBLE STRING IS IN THE CATALOGUE. Re-extracts the template from
# POTFILES, refuses a source string the committed .pot has not got, and requires
# msgcmp to find nothing untranslated in any catalogue. A string added without
# _() has no symptom at all: the program runs and prints English.
test('i18n-complete', find_program('i18n-complete.sh'),
  args : [meson.project_source_root()])

# THE CATALOGUE IS ACTUALLY BOUND, from the build tree. The operator's Catalan
# desktop got an English window with complete translations and a correct install
# path, because an uninstalled run bound the install locale dir. Only running the
# binary can see that; this runs it with LANGUAGE=ca against the catalogues meson
# just built, and carries a negative control against the English run.
test('i18n-catalan', find_program('i18n-catalan.sh'),
  args : [reac_stageboxes_exe, meson.project_build_root() / 'po'],
  depends : [reac_stageboxes_exe])

# NO USER-VISIBLE STRING ESCAPES THE CATALOGUE. i18n-complete cannot see this:
# a string never wrapped in _() is absent from both the re-extracted template
# and the committed one, so they agree and it passes while the operator reads
# English. Found by sabotage — a printf() added to the list output went green
# through the whole i18n suite. This reads the SOURCES instead.
test('i18n-no-bare-strings', find_program('i18n-no-bare-strings.py'),
  args : [meson.project_source_root() / 'src'])
