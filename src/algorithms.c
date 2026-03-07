#include "../include/algorithms.h"
#include <math.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>

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
        return INFINITY;
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
        return INFINITY;
    }
    // Cast priority to double to ensure floating-point division
    return (double)priority / estimatedTime;
}

// Creates a 2D matrix where rows represent customers and columns represent bakeries.
// It precomputes the distances in parallel to optimize the scheduling algorithm later.
double** calculateDistanceMatrix(Bakery* bakeries, int bakeryCount, Customer* customers, int customerCount) {
    
    // Step 1: Allocate memory for the array of row pointers (one pointer per customer)
    double** matrix = (double**)malloc(customerCount * sizeof(double*));
    if (matrix == NULL) {
        return NULL; // Safety check in case memory allocation fails
    }

    // Step 2: Allocate memory for each row (columns = number of bakeries)
    // We do this sequentially because calling malloc concurrently can cause lock contention.
    for (int i = 0; i < customerCount; i++) {
        matrix[i] = (double*)malloc(bakeryCount * sizeof(double));
        if (matrix[i] == NULL) exit(1);
    }

    // Step 3: Compute the distances in parallel
    // We parallelize the outer loop, meaning different threads will handle different customers.
    #pragma omp parallel for default(none) shared(matrix, customers, customerCount, bakeries, bakeryCount)
    for (int i = 0; i < customerCount; i++) {
        for (int j = 0; j < bakeryCount; j++) {
            // Using the calculateDistance function we already wrote
            matrix[i][j] = calculateDistance(customers[i].pos, bakeries[j].pos);
        }
    }

    return matrix;
}

// Frees the dynamically allocated 2D distance matrix
void freeDistanceMatrix(double** matrix, int customerCount) {
    if (matrix == NULL) return;
    
    // First, free each individual row
    for (int i = 0; i < customerCount; i++) {
        free(matrix[i]);
    }
    
    // Finally, free the main array of pointers
    free(matrix);
}

// Processes customers who have been served, deciding if they leave or order again
void processCustomerTransitions(Customer* customers, int customerCount) {
    
    // Parallel loop over all customers
    #pragma omp parallel for default(none) shared(customers, customerCount)
    for (int i = 0; i < customerCount; i++) {
        
        // Only process customers who just received their bread
        if (customers[i].status == CUSTOMER_SERVED) {
            
            // Create a unique, thread-safe seed using the current time, thread ID, and customer ID
            unsigned int local_seed = (unsigned int)time(NULL) ^ omp_get_thread_num() ^ customers[i].id;

            // Generate a random double between 0.0 and 1.0 using the POSIX rand_r function
            int randomInteger = rand_r(&local_seed);
            double randomProb = (double)randomInteger / RAND_MAX;

            // State Transition Logic
            if (randomProb < 0.2) {
                // 20% Chance: Customer departs the simulation permanently
                customers[i].status = CUSTOMER_DEPARTED;
                customers[i].demand = 0;
            } else {
                // 80% Chance: Customer stays and places a new order
                customers[i].status = CUSTOMER_ACTIVE;
                
                // Generate a new random demand for bread (e.g., between 1 and 5 loaves)
                int newDemand = (rand_r(&local_seed) % 5) + 1;
                customers[i].demand = newDemand;
                
                // Reset priority for the new order
                customers[i].priority = 1;
            }
        }
    }
}