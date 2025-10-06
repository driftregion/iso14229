## Development

Welcome aboard. You will need bazel and linux. 

Use the pre-commit hook in .githooks to automate formatting the sources and updating the amalagamation.

```sh
git config core.hooksPath .githooks
```

## Running Tests

```sh
bazel test //...
```

See [test/README.md](test/README.md)

## Release

```sh
bazel build //:release
```

### Release Checklist

- [ ] branch is rebased on main `git fetch && git merge origin main`
- [ ] push branch, all checks pass in CI `git push`
- [ ] update changelog in README.md, commit
- [ ] build release `bazel build //:release`
- [ ] `./.github/release.sh`
