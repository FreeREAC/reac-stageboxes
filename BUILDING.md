# Building reac-stageboxes

Target: Fedora + PipeWire 1.4, GTK 4.10+, libadwaita 1.4+. The only runtime
dependency beyond the toolkit is `reac-pw`, which publishes the nodes this reads.

## From source

```
sudo dnf install meson ninja-build gcc gettext desktop-file-utils \
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

## The RPM

`packaging/reac-stageboxes.spec` builds the package from a release tarball; it
`Requires: reac-pw`.

```
meson dist -C build --formats gztar
cp build/meson-dist/reac-stageboxes-0.1.0.tar.gz ~/rpmbuild/SOURCES/
rpmbuild -bb packaging/reac-stageboxes.spec
```

## Releasing

RPMs are built, signed and published by GitHub Actions
(`.github/workflows/release-rpm.yml`), never from a desk. The package carries
the `fmx` suffix (`reac-stageboxes-0.1.0-1.fc44.fmx`).

- Push a tag `dryrun-v<version>`: the RPM is built and signed with a throwaway
  key into the job's workspace, and the signed dnf tree is checked and kept as an
  artifact. Nothing is published.
- Dispatch the workflow with `tag` = `v<version>` and `publish` ticked, from
  `main`: the same build is signed with the org packages key and pushed to the
  FreeREAC dnf repo, and the RPM is attached to the release. Any other branch is
  refused. A tag push alone never publishes.

The version in the tag, `meson.build` and the spec must agree; `build-rpm.sh`
refuses otherwise. Secrets, by name: `PACKAGES_GPG_KEY` and
`PACKAGES_GPG_PASSPHRASE` (the signing key) and `FREEREAC_PAGES_TOKEN` (push to
the dnf tree). The key's fingerprint must match the one the dnf tree publishes,
or the publish stops before signing.

## Translations

A binary run from the build tree finds the catalogues meson built beside it, so
`LANGUAGE=ca build/reac-stageboxes --list` is Catalan without installing
anything. `REAC_STAGEBOXES_LOCALEDIR` overrides the search when you want to
point it at another tree; an installed binary falls back to its configured
`localedir`.

Adding a string means wrapping it in `_()` and translating it in every
catalogue: `meson test` fails otherwise, both on a string that never reached the
template and on a template entry no catalogue translates.
