# Release Checklist

- [ ] rebase branch on origin/main `git fetch && git merge origin main`
- [ ] update CHANGELOG, add a new version 
- [ ] update version.h and Doxyfile PROJECT_NUMBER to match the latest CHANGELOG version 
- [ ] push branch, all checks pass in CI `git push`
- [ ] `bazel build //:release && ./.github/release.sh`
