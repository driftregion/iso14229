# Release Checklist

paste this checklist into the CHANGELOG under a new VERSION, then complete it.

```
## VERSION

- SUMMARY

- [ ] rebase branch on origin/main `git fetch && git merge origin main`
- [ ] update VERSION file, version.h, Doxyfile PROJECT_NUMBER
- [ ] push branch, all checks pass in CI `git push`
- [ ] update changelog in README.md, commit
- [ ] git tag `cat VERSION`
- [ ] `bazel build //:release && ./.github/release.sh`
```
