#include <stdio.h>
#include <stdlib.h>
#include "../include/entities.h"
#include "../include/algorithms.h"

// A function that runs quick tests on our mathematical functions
void test_algorithms() {
    printf("--- Testing Math Algorithms ---\n");

    // Test 1: Euclidean distance (Classic 3-4-5 right triangle)
    Position p1 = {0.0, 0.0};
    Position p2 = {3.0, 4.0};
    double dist1 = calculateDistance(p1, p2);
    printf("Test 1 (Distance): (0,0) to (3,4) -> Expected: 5.00, Got: %.2f\n", dist1);

    // Test 2: Distance between the same point (should output 0)
    Position p3 = {15.5, 42.1};
    double dist2 = calculateDistance(p3, p3);
    printf("Test 2 (Distance): Same point -> Expected: 0.00, Got: %.2f\n", dist2);

    // Test 3: Normal flight time (distance 100, velocity 20 = 5 seconds)
    double time1 = calculateFlightTime(100.0, 20.0);
    printf("Test 3 (Flight Time): d=100, v=20 -> Expected: 5.00, Got: %.2f\n", time1);

    // Test 4: Flight time with velocity 0 (edge case that should return INFINITY)
    double time2 = calculateFlightTime(50.0, 0.0);
    printf("Test 4 (Flight Time edge case): d=50, v=0 -> Expected: inf, Got: %f\n", time2);

    printf("-------------------------------\n\n");
}

// A function to test the scheduling logic, matrices, and state transitions
void test_scheduler_and_state() {
    printf("--- Testing Scheduler & State Logic ---\n");

    // Test 5 & 6: Score Calculation
    double score1 = calculateCustomerScore(5, 10.0);
    printf("Test 5 (Normal Score): p=5, t=10.0 -> Expected: 0.50, Got: %.2f\n", score1);
    
    double score2 = calculateCustomerScore(2, 0.0);
    printf("Test 6 (Zero Time Score): p=2, t=0.0 -> Expected: inf, Got: %f\n", score2);

    // Test 7: OpenMP Priority Updates
    Customer test_customers[3];
    test_customers[0].status = CUSTOMER_ACTIVE;  test_customers[0].priority = 1;
    test_customers[1].status = CUSTOMER_SERVED;  test_customers[1].priority = 1;
    test_customers[2].status = CUSTOMER_ACTIVE;  test_customers[2].priority = 5;

    updateCustomerPriorities(test_customers, 3);
    printf("Test 7 (OMP Priority): Active C1 -> Expected: 2, Got: %d\n", test_customers[0].priority);
    printf("Test 8 (OMP Priority): Served C2 -> Expected: 1, Got: %d\n", test_customers[1].priority);
    printf("Test 9 (OMP Priority): Active C3 -> Expected: 6, Got: %d\n", test_customers[2].priority);

    // Test 10: Distance Matrix Creation & Memory Freeing
    Bakery test_bakeries[2];
    test_bakeries[0].pos = (Position){0.0, 0.0};
    test_bakeries[1].pos = (Position){10.0, 10.0};
    
    Customer test_matrix_customers[2];
    test_matrix_customers[0].pos = (Position){3.0, 4.0};   // Dist to B0 is 5.0
    test_matrix_customers[1].pos = (Position){10.0, 10.0}; // Dist to B1 is 0.0

    double** matrix = calculateDistanceMatrix(test_bakeries, 2, test_matrix_customers, 2);
    if (matrix != NULL) {
        printf("Test 10 (Matrix [0][0]): Cust 0 to Bakery 0 -> Expected: 5.00, Got: %.2f\n", matrix[0][0]);
        printf("Test 11 (Matrix [1][1]): Cust 1 to Bakery 1 -> Expected: 0.00, Got: %.2f\n", matrix[1][1]);
        freeDistanceMatrix(matrix, 2); // Free it to prevent leaks!
        printf("Test 12 (Matrix Memory): Safely freed distance matrix.\n");
    } else {
        printf("Test 10-12 Failed: Matrix allocation returned NULL.\n");
    }

    // Test 13: Customer Transitions (Using rand_r)
    Customer test_transition[1];
    test_transition[0].id = 1;
    test_transition[0].status = CUSTOMER_SERVED;
    test_transition[0].demand = 0;
    test_transition[0].priority = 99;
    
    processCustomerTransitions(test_transition, 1);
    
    // Because it's random, we just print the result to ensure it changed 
    // to either CUSTOMER_ACTIVE or CUSTOMER_DEPARTED.
    printf("Test 13 (Transitions): Served Customer -> New Status Enum: %d, New Demand: %d, New Priority: %d\n", 
            test_transition[0].status, test_transition[0].demand, test_transition[0].priority);

    printf("---------------------------------------\n\n");
}

int main() {
    printf("Welcome to the Parallel Drone Delivery Simulation! \n\n");
    
    test_algorithms();
    test_scheduler_and_state();

    return 0;
}