#!/usr/bin/bash

if diff expected.txt actual.txt > /dev/null; then
    echo "Test passed!"
else
    echo "Expected and actual don't match: test failed"
fi