#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/entities.h"

// We include the main.c file to get access to the produceBread function, which is currently defined there.
// We use a preprocessor trick to rename the main function in main.c to something else, so that we can call it from our test.
//We done that becasue I created a test folder and I want to be able to run the tests without running the main simulation, which is defined in main.c
#define main disabled_original_main
#include "../src/main.c"
#undef main

// Explicit declaration since the function is currently inside main.c
void produceBread(Bakery* bakeries, int bakeryCount);

void test_capacity_limit() {
    printf("Running Capacity Limit Test...\n");

    Bakery bakeries[1];
    // Setup a rule that has a 100% chance of producing 10 breads
    ProductionRule rules[1] = {{10, 1.0}};
    initBakery(0, (Position){0, 0}, rules, 1, 15, &bakeries[0]);

    // Initial inventory is 8. Adding 10 would be 18, which exceeds capacity of 15.
    bakeries[0].inventory = 8;

    produceBread(bakeries, 1);

    // If your logic is correct, it should cap exactly at 15
    assert(bakeries[0].inventory == 15);
    printf("Capacity Limit Test Passed!\n");

    freeBakery(&bakeries[0]);
}

void test_normal_production() {
    printf("Running Normal Production Test...\n");

    Bakery bakeries[2];
    // Setup a rule that has a 100% chance of producing 5 breads
    ProductionRule rules[1] = {{5, 1.0}};

    initBakery(0, (Position){0, 0}, rules, 1, 20, &bakeries[0]);
    initBakery(1, (Position){1, 1}, rules, 1, 20, &bakeries[1]);

    bakeries[0].inventory = 0;
    bakeries[1].inventory = 5;

    produceBread(bakeries, 2);

    assert(bakeries[0].inventory == 5);
    assert(bakeries[1].inventory == 10);
    printf("Normal Production Test Passed!\n");

    freeBakery(&bakeries[0]);
    freeBakery(&bakeries[1]);
}

int main() {
    printf("=== Starting ProduceBread Tests ===\n");
    test_capacity_limit();
    test_normal_production();
    printf("=== All tests passed successfully! ===\n");
    return 0;
}