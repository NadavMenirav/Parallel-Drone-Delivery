#include "../include/algorithms.h"
#include <math.h>
#include <omp.h>

// Calculate Euclidean distance (Pythagorean theorem)
double calculateDistance(Position p1, Position p2) {
    // Calculate the differences in x and y coordinates
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    // Return the Euclidean distance using the formula: distance = sqrt(dx^2 + dy^2)
    return sqrt((dx * dx) + (dy * dy));
}

// Calculate flight time according to the formula t = d / v
double calculateFlightTime(double distance, double velocity) {
    // Simple edge case protection to prevent division by zero 
    if (velocity <= 0.0) {
        return 0.0; 
    }
    // Return the calculated flight time
    return distance / velocity;
}

void updateCustomerPriorities(Customer* customers, int customerCount) {
    //parallelize the loop to update customer priorities using OpenMP
    #pragma omp parallel for
    for (int i = 0; i < customerCount; i++) {
        // Check if the customer is still waiting for their order
        if (customers[i].status == CUSTOMER_ACTIVE) { 
            // Increment the priority of the customer by 1 for each round they are not served
            customers[i].priority += 1;
        }
    }
}

// Formula: Score(c) = w_c / T_c (where w_c is priority and T_c is estimated completion time)
double calculateCustomerScore(int priority, double estimatedTime) {
    // Edge case protection: Prevent division by zero if the estimated time is 0 or negative
    if (estimatedTime <= 0.0) {
        return 0.0; 
    }
    // Cast priority to double to ensure floating-point division
    return (double)priority / estimatedTime;
}