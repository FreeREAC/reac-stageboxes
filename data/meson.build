# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

i18n = import('i18n')

# The desktop entry. Name and Comment are translated out of the same catalogue
# as the application, so a Catalan session gets a Catalan launcher.
desktop_file = i18n.merge_file(
  input : 'org.freereac.Stageboxes.desktop.in',
  output : 'org.freereac.Stageboxes.desktop',
  type : 'desktop',
  po_dir : '../po',
  install : true,
  install_dir : get_option('datadir') / 'applications')

desktop_utils = find_program('desktop-file-validate', required : false)
if desktop_utils.found()
  test('desktop-file', desktop_utils, args : [desktop_file])
endif

# The icon is a PLACEHOLDER: a legible symbol at the sizes GNOME draws it, and
# not a finished identity. Replacing it is a file swap, not a code change.
install_data('icons/hicolor/scalable/apps/org.freereac.Stageboxes.svg',
  install_dir : get_option('datadir') / 'icons/hicolor/scalable/apps')
