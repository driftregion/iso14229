# Release Checklist

- [ ] rebase branch on origin/main `git fetch && git merge origin main`
- [ ] update CHANGELOG, add a new version: "X.Y.Z"
    - until version 1.0.0, anything can change at any time. I increment Y if the API changed, otherwise X.
- [ ] update `VERSION`, `src/version.h`, `Doxyfile` PROJECT_NUMBER to match the latest CHANGELOG version
- [ ] paste and run:
```sh
tools/run_clang_format.sh && make update_srcs && test/test_version.sh
```
- [ ] commit, push, confirm that all checks pass in CI 
- [ ] paste and run:
```sh
bazel build //:release && ./.github/release.sh
```
This creates a tag named with the contents of the `VERSION` file.
