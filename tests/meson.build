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
