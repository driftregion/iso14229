
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "hedron_compile_commands",
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/e16062717d9b098c3c2ac95717d2b3e661c50608.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-e16062717d9b098c3c2ac95717d2b3e661c50608",
    sha256 = "ed5aea1dc87856aa2029cb6940a51511557c5cac3dbbcb05a4abd989862c36b4"
)
load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()

http_archive(
    name = "cmocka",
    url = "https://cmocka.org/files/1.1/cmocka-1.1.7.tar.xz",
    sha256 = "810570eb0b8d64804331f82b29ff47c790ce9cd6b163e98d47a4807047ecad82",
    build_file = "//test:cmocka.BUILD",
    strip_prefix = "cmocka-1.1.7",
)

http_archive(
    name = "s32k_sdk",
    url = "https://www.keil.com/pack/Keil.S32_SDK_DFP.1.5.0.pack",
    sha256 = "95f7649aee66deb656cc999f300663544113245aecd407cfc90548377355353b",
    type = "zip",
    build_file = "//platforms:s32k_sdk.BUILD",
)

http_archive(
    name = "CMSIS",
    build_file = "//platforms:cmsis.BUILD",
    sha256 = "14b366f2821ee5d32f0d3bf48ef9657ca45347261d0531263580848e9d36f8f4",
    urls = ["http://www.keil.com/pack/ARM.CMSIS.5.9.0.pack"],
    type = ".zip",
)