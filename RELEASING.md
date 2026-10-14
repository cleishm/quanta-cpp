# Releasing

1. **Prepare a release PR.** On a branch, bump the version in all three places
   that record it:

   - `CMakeLists.txt` (`project(quanta VERSION …)`)
   - `docs/Doxyfile` (`PROJECT_NUMBER`)
   - `idf_component.yml` (`version:`)

   Add a `## vX.Y.Z` section at the top of `CHANGELOG.md`, in the style of the
   existing ones: a lead paragraph, then `###` subsections of bullets. Versions
   follow [semantic versioning](https://semver.org/); while quanta is 0.x, a
   breaking change bumps the minor version.

2. **Merge the PR** once the `all-checks` status passes. Branch protection
   requires both.

3. **Tag the merge commit** with an annotated tag:

   ```bash
   git switch main && git pull
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push origin vX.Y.Z
   ```

4. **Publish a GitHub release**, titled with the tag and using the version's
   CHANGELOG section as its notes:

   ```bash
   awk '/^## vX.Y.Z/{f=1;next} /^## /{f=0} f' CHANGELOG.md > notes.md
   gh release create vX.Y.Z --title vX.Y.Z --notes-file notes.md --verify-tag
   ```

5. **Check the ESP Component Registry upload.** Publishing the release runs
   `.github/workflows/upload_component.yml`, which uploads `cleishm/quanta`.
   Check that the run succeeded; the new version can take a few minutes to
   appear on [components.espressif.com](https://components.espressif.com/components/cleishm/quanta).

The documentation needs no step: it is deployed to GitHub Pages on every push to
`main`, so it is current once the release PR merges.
