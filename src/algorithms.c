#include "../include/algorithms.h"
#include <math.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

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
//
// Each customer's distanceMatrixRow field is set to its row index in the matrix,
// so lookups remain correct even after the customers array is reordered by qsort.
//
// NOTE: This matrix assumes customer positions are static throughout the simulation.
// If customers ever change position, the matrix must be recomputed.
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

    // Step 3: Compute the distances in parallel and tag each customer with its stable row index.
    // We parallelize the outer loop, meaning different threads will handle different customers.
    #pragma omp parallel for default(none) shared(matrix, customers, customerCount, bakeries, bakeryCount)
    for (int i = 0; i < customerCount; i++) {
        customers[i].distanceMatrixRow = i;
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

// Comparison function for qsort to sort customers in descending order based on their tempScore
int compareCustomersDesc(const void* a, const void* b) {
    Customer* c1 = (Customer*)a;
    Customer* c2 = (Customer*)b;
    if (c1->tempScore < c2->tempScore) return 1;
    if (c1->tempScore > c2->tempScore) return -1;
    return 0;
}

// --- Helper function: Calculate the average velocity and capacity ---
void calculateDroneAverages(Drone* drones, int droneCount, double* avgVelocity, double* avgCapacity) {
    double sumVel = 0.0;
    double sumCap = 0.0;
    
    #pragma omp parallel for default(none) shared(drones, droneCount) reduction(+:sumVel, sumCap)
    for (int i = 0; i < droneCount; i++) {
        sumVel += drones[i].velocity;
        sumCap += drones[i].capacity;
    }
    
    *avgVelocity = sumVel / droneCount;
    *avgCapacity = sumCap / droneCount;
}

// --- Stage 2: Calculate and Sort ---
// Calculates the heuristic score for each active customer and stores it in their tempScore field.
void calculateCustomerScoresStage2(Customer* customers, int cCount, double avgVelocity, double avgCapacity) {
    #pragma omp parallel for default(none) shared(customers, cCount, avgVelocity, avgCapacity)
    for (int i = 0; i < cCount; i++) {
        if (customers[i].status != CUSTOMER_ACTIVE) {
            customers[i].tempScore = -1.0;
            continue;
        }

        double d_min = customers[i].closestBakeryDistance;
        double t_base = d_min / avgVelocity;
        if (t_base <= 0.0) t_base = 0.1; 

        double q_c = (double)customers[i].demand;
        double n_trips = ceil(q_c / avgCapacity);
        double t_c = t_base * n_trips;

        customers[i].tempScore = customers[i].priority / t_c;
    }
}
// --- Stage 3: Drone Scheduling and Routing ---
typedef struct {
    int bakeryId;
    int droneId;
    double bestTime;
    int isValid;
} Proposal;

// Assigns drones to customers, planning the route and updating inventory.
void assignDronesStage3(Customer* customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound) {
    
    // Step 1: Pre-compute drone-to-bakery distances to save expensive sqrt calculations inside the loop
    double** droneBakeryDist = malloc(dCount * sizeof(double*));
    if (droneBakeryDist == NULL) exit(1);
    for (int d = 0; d < dCount; d++) {
        droneBakeryDist[d] = malloc(bCount * sizeof(double));
        if (droneBakeryDist[d] == NULL) exit(1);
        for (int b = 0; b < bCount; b++) {
            droneBakeryDist[d][b] = calculateDistance(drones[d].pos, bakeries[b].pos);
        }
    }

    Proposal* proposals = malloc(cCount * sizeof(Proposal));
    if (proposals == NULL) exit(1);

    int matchMade = 1; 

    // Loop until no more matches can be made (Handles order splitting natively!)
    while (matchMade) {
        matchMade = 0; 

        // --- Phase 1: Parallel Search ---
        //
        // SYNCHRONIZATION NOTE: This parallel region reads drones[d].currentCustomer and
        // bakeries[b].inventory, which are modified in the sequential Phase 2 below.
        // This is safe because the implicit barrier at the end of #pragma omp parallel for
        // guarantees all threads complete Phase 1 before any thread enters Phase 2.
        // However, this two-phase design is order-dependent — do NOT merge the phases or
        // remove the barrier without adding explicit synchronization.
        //
        #pragma omp parallel for default(none) shared(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, droneBakeryDist, proposals)
        for (int c = 0; c < cCount; c++) {
            proposals[c].isValid = 0;
            proposals[c].bestTime = INFINITY;

            // Only process active customers who still need bread
            if (customers[c].status != CUSTOMER_ACTIVE || customers[c].demand == 0) continue;

            // Use the customer's stable row index to look up the correct distance matrix row,
            // since qsort may have reordered the customers array.
            int matrixRow = customers[c].distanceMatrixRow;

            for (int b = 0; b < bCount; b++) {
                if (bakeries[b].inventory == 0) continue; // Bakery is empty

                for (int d = 0; d < dCount; d++) {
                    // Drone must be free right now
                    if (drones[d].currentCustomer != NULL) continue;

                    // Route: Drone -> Bakery -> Customer
                    double totalDist = droneBakeryDist[d][b] + distanceMatrix[matrixRow][b];
                    double flightTime = totalDist / drones[d].velocity; 

                    if (flightTime < proposals[c].bestTime) {
                        proposals[c].bestTime = flightTime;
                        proposals[c].bakeryId = b;
                        proposals[c].droneId = d;
                        proposals[c].isValid = 1;
                    }
                }
            }
        }

        // --- Phase 2: Sequential Commitment ---
        // Iterate over customers (they are already sorted by Score!)
        for (int c = 0; c < cCount; c++) {
            if (proposals[c].isValid) {
                int dId = proposals[c].droneId;
                int bId = proposals[c].bakeryId;

                // Ensure another higher-priority customer didn't steal our drone or bread
                if (drones[dId].currentCustomer == NULL && bakeries[bId].inventory > 0) {
                    
                    matchMade = 1; // Match successful! Loop will run again to see if splits are needed
                    
                    drones[dId].currentCustomer = &customers[c];
                    
                    // Calculate how much bread the drone takes
                    int breadToTake = (customers[c].demand < drones[dId].capacity) ? customers[c].demand : drones[dId].capacity;
                    if (breadToTake > bakeries[bId].inventory) breadToTake = bakeries[bId].inventory;
                    
                    // Update Bakery and Customer
                    bakeries[bId].inventory -= breadToTake;
                    customers[c].demand -= breadToTake;
                    
                    // Update Drone status (Delivery Execution)
                    int timeTaken = (int)ceil(proposals[c].bestTime);
                    drones[dId].availableAtRound = currentRound + timeTaken;
                    drones[dId].pos = customers[c].pos;
                    drones[dId].load = breadToTake;

                    if (customers[c].demand == 0) {
                        customers[c].status = CUSTOMER_SERVED;
                    }
                    
                    printf("  -> Assigned Drone %d to Customer %d (Takes %d bread from Bakery %d, Arrives at round %d)\n", 
                           drones[dId].id, customers[c].id, breadToTake, bakeries[bId].id, drones[dId].availableAtRound);
                }
            }
        }
    }
    // Cleanup memory
    free(proposals);
    for (int d = 0; d < dCount; d++) free(droneBakeryDist[d]);
    free(droneBakeryDist);
}