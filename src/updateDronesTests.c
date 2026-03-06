#include <stdio.h>
#include <assert.h>
#include "../include/entities.h"

// Explicit declaration since the function is inside main.c
void updateDrones(Drone* drones, const int droneCount, const int currentRound);

void test_drone_release() {
    printf("Running Drone Release Tests...\n");

    Drone drones[3];
    Customer dummyCustomer; // We just need an address to point to

    // Case 1: Drone finished exactly in this round (round 10)
    drones[0].availableAtRound = 10;
    drones[0].currentCustomer = &dummyCustomer;

    // Case 2: Drone finished in a previous round (round 8)
    drones[1].availableAtRound = 8;
    drones[1].currentCustomer = &dummyCustomer;

    // Case 3: Drone is busy until round 12
    drones[2].availableAtRound = 12;
    drones[2].currentCustomer = &dummyCustomer;

    // Run the function for round 10 [cite: 7, 19]
    updateDrones(drones, 3, 10);

    // Assertions for Case 1 (Should be freed) [cite: 52]
    assert(drones[0].currentCustomer == NULL);
    assert(drones[0].availableAtRound == 10);

    // Assertions for Case 2 (Should be freed AND time synced to 10) [cite: 52]
    assert(drones[1].currentCustomer == NULL);
    assert(drones[1].availableAtRound == 10);

    // Assertions for Case 3 (Should NOT be touched)
    assert(drones[2].currentCustomer == &dummyCustomer);
    assert(drones[2].availableAtRound == 12);

    printf("Drone Release Tests Passed!\n");
}

int main() {
    printf("=== Starting UpdateDrones Tests ===\n");
    test_drone_release();
    printf("=== All tests passed successfully! ===\n");
    return 0;
}