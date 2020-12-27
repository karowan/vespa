// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/testkit/testapp.h>

TEST_SETUP(Test);

int Test::Main() {
    char * v = static_cast<char *>(malloc(0x400001));
    char * nv = static_cast<char *>(realloc(v, 0x500001));
    ASSERT_TRUE(v == nv);
    v = static_cast<char *>(realloc(nv, 0x600001));
    ASSERT_TRUE(v != nv);
    free(v);

    char *t = static_cast<char *>(malloc(70));
#pragma GCC diagnostic push
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
    free (t+7);
    t = static_cast<char *>(malloc(0x400001));
    free (t+7);
#pragma GCC diagnostic pop
    return 0;
}
