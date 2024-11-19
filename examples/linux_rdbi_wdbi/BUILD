cc_binary(
    name = "server",
    srcs = [ "server.c" ],
    deps = [ "//:iso14229" ],
)

cc_binary(
    name = "client",
    srcs = [ "client.c" ],
    deps = [ "//:iso14229" ], 
)

sh_test(
    name = "test",
    srcs = ["test.sh"],
    data = [
        "server",
        "client",
    ],
    args = [
        "$(location :server)",
        "$(location :client)",
    ],
    tags=["vcan"],
)
