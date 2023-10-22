cc_library(
    name = "env",
    srcs = ["env.c", "env.h"],
    deps = [
        "//:iso14229"
    ]
)

[
    cc_test(
        name=n.split(".c")[0] , 
        srcs=[n], 
        deps=[":env"],
    )

    for n in glob(["test_*.c"])
]