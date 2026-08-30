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
