#include <stdio.h>
#include <stdlib.h>
#include "../include/entities.h"
#include "../include/algorithms.h"

// A function that runs quick tests on our mathematical functions
void test_algorithms() {
    printf("--- Testing algorithms.c ---\n");

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

    // Test 4: Flight time with velocity 0 (edge case that should return INFINITY based on our updated code)
    double time2 = calculateFlightTime(50.0, 0.0);
    printf("Test 4 (Flight Time edge case): d=50, v=0 -> Expected: inf, Got: %.2f\n", time2);

    printf("----------------------------\n\n");
}

int main() {
    printf("Welcome to the Parallel Drone Delivery Simulation!\n\n");
    
    test_algorithms();

    return 0;
}