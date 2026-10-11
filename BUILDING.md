# Building reac-stageboxes

Target: Fedora 44 and Debian trixie, PipeWire 1.4, GTK 4.10+, libadwaita 1.4+. The only runtime
dependency beyond the toolkit is `reac-pw`, which publishes the nodes this reads.

## From source

```
sudo dnf install meson ninja-build gcc gettext desktop-file-utils appstream \
                 pipewire-devel gtk4-devel libadwaita-devel
meson setup build
meson compile -C build
meson test -C build
sudo meson install -C build
```

`meson test` needs no PipeWire session, no graph and no box: the head-amp
vocabulary, the pod a write carries, the box model and the registry walk are
driven offline. The i18n test that runs the binary under `LANGUAGE=ca` needs the
`ca_ES.UTF-8` locale (`glibc-langpack-ca`); without it the test reports
itself skipped rather than passing. The suite also checks that the README carries
no build command: build steps belong here.

## The packages

`packaging/reac-stageboxes.spec` and `debian/` build the RPM and the DEB. Both are built by the shared
release workflows of [FreeMixer/.github](https://github.com/FreeMixer/.github), never from a desk; to try
the RPM build by hand:

```
meson dist -C build --formats gztar
cp build/meson-dist/reac-stageboxes-0.1.0.tar.gz ~/rpmbuild/SOURCES/
rpmbuild -bb packaging/reac-stageboxes.spec
```

## Releasing

`.github/workflows/ci.yml` runs `meson test` on Fedora 44 and Debian trixie for every push to `main` and
every pull request. `.github/workflows/release.yml` calls the shared build-rpm and build-deb workflows:

- On a pull request or a manual run it builds and lints the RPMs and DEBs and publishes nothing.
- A tag `v<version>` builds them on x86_64/amd64 and aarch64/arm64, signs them with the organisation's
  packages key, publishes them into the FreeMixer channel and attaches the RPMs to the GitHub release.

The version is one number: `meson.build`, the spec's `Version:`, the top of `debian/changelog` and the newest
`<release>` in the AppStream file must agree, and `meson test` fails when they do not. The tag is `v` and that
number. Release notes and package changelogs are generated from the git log.

## Translations

A binary run from the build tree finds the catalogues meson built beside it, so
`LANGUAGE=ca build/reac-stageboxes --list` is Catalan without installing
anything. `REAC_STAGEBOXES_LOCALEDIR` overrides the search when you want to
point it at another tree; an installed binary falls back to its configured
`localedir`.

Adding a string means wrapping it in `_()` and translating it in every
catalogue: `meson test` fails otherwise, both on a string that never reached the
template and on a template entry no catalogue translates.
