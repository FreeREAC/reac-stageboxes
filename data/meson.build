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

# The application identity: a full-colour icon plus its symbolic variant, both
# in the hicolor theme. gtk-update-icon-cache is not run here — that is a
# system-wide operation the RPM's %post/%postun/%posttrans scriptlets own, the
# same way every other hicolor-installing package on the desk does it.
install_data('icons/hicolor/scalable/apps/org.freereac.Stageboxes.svg',
  install_dir : get_option('datadir') / 'icons/hicolor/scalable/apps')
install_data('icons/hicolor/symbolic/apps/org.freereac.Stageboxes-symbolic.svg',
  install_dir : get_option('datadir') / 'icons/hicolor/symbolic/apps')

# AppStream metadata: what a software centre shows for this application. No
# screenshots are declared — there is no screenshot file in this tree, and a
# claimed one that 404s is worse than none.
metainfo_file = files('org.freereac.Stageboxes.metainfo.xml')
install_data(metainfo_file, install_dir : get_option('datadir') / 'metainfo')

appstream_util = find_program('appstreamcli', required : false)
if appstream_util.found()
  test('metainfo-validate', appstream_util,
    args : ['validate', '--no-net', metainfo_file])
endif
