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

void updateCustomerPriorities(Customer** customers, int customerCount) {
    //parallelize the loop to update customer priorities using OpenMP
    #pragma omp parallel for
    for (int i = 0; i < customerCount; i++) {
        // Check if the customer is still waiting for their order
        if (customers[i]->status == CUSTOMER_ACTIVE) { 
            // Increment the priority of the customer by 1 for each round they are not served
            customers[i]->priority += 1;
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
double** calculateDistanceMatrix(Bakery* bakeries, int bakeryCount, Customer** customers, int customerCount) {
    
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
    #pragma omp parallel for default(none) shared(matrix, customers, customerCount, bakeries, bakeryCount) schedule(dynamic)
    for (int i = 0; i < customerCount; i++) {
        customers[i]->distanceMatrixRow = i;
        for (int j = 0; j < bakeryCount; j++) {
            // Using the calculateDistance function we already wrote
            matrix[i][j] = calculateDistance(customers[i]->pos, bakeries[j].pos);
        }
    }
    // Return the fully computed distance matrix
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
void processCustomerTransitions(Customer** customers, int customerCount,int currentRound) {
    
    // Parallel loop over all customers
    #pragma omp parallel for default(none) shared(customers, customerCount, currentRound)
    for (int i = 0; i < customerCount; i++) {
        
        // Only process customers who just received their bread
        if (customers[i]->status == CUSTOMER_SERVED) {
            
            // Create a unique, thread-safe seed using the current time, thread ID, and customer ID
           unsigned int local_seed = (unsigned int)time(NULL) ^ omp_get_thread_num() ^ customers[i]->id ^ (unsigned int)currentRound;

            // Generate a random double between 0.0 and 1.0 using the POSIX rand_r function
            int randomInteger = rand_r(&local_seed);
            double randomProb = (double)randomInteger / RAND_MAX;

            // State Transition Logic
            if (randomProb < 0.2) {
                // 20% Chance: Customer departs the simulation permanently
                customers[i]->status = CUSTOMER_DEPARTED;
                customers[i]->demand = 0;
            } else {
                // 80% Chance: Customer stays and places a new order
                customers[i]->status = CUSTOMER_ACTIVE;
                
                // Generate a new random demand for bread (e.g., between 1 and 5 loaves)
                int newDemand = (rand_r(&local_seed) % 5) + 1;
                customers[i]->demand = newDemand;
                
                // Reset priority for the new order
                customers[i]->priority = 1;
            }
        }
    }
}

// Comparison function for qsort to sort customers in descending order based on their tempScore
int compareCustomersDesc(const void* a, const void* b) {
    // We need to cast the void pointers todouble pointers to access the tempScore field for comparison
    Customer* c1 = *(Customer**)a;
    Customer* c2 = *(Customer**)b;
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
void calculateCustomerScoresStage2(Customer** customers, int cCount, double avgVelocity, double avgCapacity) {
    #pragma omp parallel for default(none) shared(customers, cCount, avgVelocity, avgCapacity) schedule(dynamic)
    for (int i = 0; i < cCount; i++) {
        if (customers[i]->status != CUSTOMER_ACTIVE) {
            customers[i]->tempScore = -1.0;
            continue;
        }

        double d_min = customers[i]->closestBakeryDistance;
        double t_base = d_min / avgVelocity;
        if (t_base <= 0.0) t_base = 0.1; 

        double q_c = (double)customers[i]->demand;
        double n_trips = ceil(q_c / avgCapacity);
        double t_c = t_base * n_trips;

        customers[i]->tempScore = customers[i]->priority / t_c;
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
void assignDronesStage3(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound) {
    
    // Pre-compute distances
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

    // ==========================================
    // INITIALIZE ALL LOCKS (Drones, Bakeries, Customers)
    // ==========================================
    omp_lock_t* drone_locks = malloc(dCount * sizeof(omp_lock_t));
    omp_lock_t* bakery_locks = malloc(bCount * sizeof(omp_lock_t));
    omp_lock_t* customer_locks = malloc(cCount * sizeof(omp_lock_t));
    
    for (int d = 0; d < dCount; d++) omp_init_lock(&drone_locks[d]);
    for (int b = 0; b < bCount; b++) omp_init_lock(&bakery_locks[b]);
    for (int c = 0; c < cCount; c++) omp_init_lock(&customer_locks[c]);

    int matchMade = 1; 

    while (matchMade) {
        matchMade = 0; 

        // --- Phase 1: 100% Parallel Search ---
        #pragma omp parallel for default(none) shared(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, droneBakeryDist, proposals) schedule(dynamic)
        for (int c = 0; c < cCount; c++) {
            proposals[c].isValid = 0;
            proposals[c].bestTime = INFINITY;

            if (customers[c]->status != CUSTOMER_ACTIVE || customers[c]->demand == 0) continue;

            int matrixRow = customers[c]->distanceMatrixRow;

            for (int b = 0; b < bCount; b++) {
                if (bakeries[b].inventory == 0) continue;

                for (int d = 0; d < dCount; d++) {
                    if (drones[d].currentCustomer != NULL) continue;

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

        // --- Phase 2: 100% Parallel Commitment (NO SEQUENTIAL CODE) ---
        #pragma omp parallel for default(none) shared(customers, cCount, bakeries, drones, proposals, distanceMatrix, currentRound, matchMade, drone_locks, bakery_locks, customer_locks) 
        for (int c = 0; c < cCount; c++) {
            if (!proposals[c].isValid) continue;

            // 1. Lock Primary Customer
            omp_set_lock(&customer_locks[c]);
            if (customers[c]->status != CUSTOMER_ACTIVE || customers[c]->demand == 0) {
                omp_unset_lock(&customer_locks[c]);
                continue;
            }

            int dId = proposals[c].droneId;
            int bId = proposals[c].bakeryId;

            // 2. Lock Drone
            omp_set_lock(&drone_locks[dId]);
            if (drones[dId].currentCustomer == NULL) {
                
                // 3. Lock Bakery
                omp_set_lock(&bakery_locks[bId]);
                if (bakeries[bId].inventory > 0) {
                    
                    #pragma omp atomic write
                    matchMade = 1;

                    // Primary Assignment
                    int breadToTake = (customers[c]->demand < drones[dId].capacity) ? customers[c]->demand : drones[dId].capacity;
                    breadToTake = (breadToTake < bakeries[bId].inventory) ? breadToTake : bakeries[bId].inventory;

                    bakeries[bId].inventory -= breadToTake;
                    customers[c]->demand -= breadToTake;

                    double totalTime = proposals[c].bestTime;
                    Position finalPos = customers[c]->pos;
                    int remainingCap = drones[dId].capacity - breadToTake;

                    // PIGGYBACKING LOGIC
                    if (remainingCap > 0 && bakeries[bId].inventory > 0) {
                        for (int c2 = 0; c2 < cCount; c2++) {
                            if (c == c2) continue;

                            if (customers[c2]->status == CUSTOMER_ACTIVE && 
                                customers[c2]->demand > 0 && 
                                customers[c2]->demand <= remainingCap && 
                                customers[c2]->demand <= bakeries[bId].inventory) {
                                
                                double distC1toC2 = calculateDistance(finalPos, customers[c2]->pos);
                                
                                if (distC1toC2 < distanceMatrix[customers[c]->distanceMatrixRow][bId]) {
                                    
                                    // NON-BLOCKING LOCK: Only take C2 if no one else is currently locking them!
                                    if (omp_test_lock(&customer_locks[c2])) {
                                        if (customers[c2]->status == CUSTOMER_ACTIVE && customers[c2]->demand > 0) {
                                            
                                            int extraBread = customers[c2]->demand;
                                            bakeries[bId].inventory -= extraBread;
                                            customers[c2]->demand -= extraBread;
                                            breadToTake += extraBread;
                                            
                                            totalTime += calculateFlightTime(distC1toC2, drones[dId].velocity);
                                            finalPos = customers[c2]->pos; 
                                            
                                            if (customers[c2]->demand <= 0) {
                                                customers[c2]->status = CUSTOMER_SERVED;
                                            }
                                            omp_unset_lock(&customer_locks[c2]);
                                            break; // Stop looking for more piggybacks
                                        }
                                        omp_unset_lock(&customer_locks[c2]);
                                    }
                                }
                            }
                        }
                    }

                    // Finalize Drone
                    drones[dId].currentCustomer = customers[c]; 
                    drones[dId].availableAtRound = currentRound + (int)ceil(totalTime);
                    drones[dId].pos = finalPos; 
                    drones[dId].load = breadToTake; 

                    if (customers[c]->demand <= 0) {
                        customers[c]->status = CUSTOMER_SERVED;
                    }
                }
                omp_unset_lock(&bakery_locks[bId]);
            }
            omp_unset_lock(&drone_locks[dId]);
            omp_unset_lock(&customer_locks[c]);
        }
    }

    // CLEANUP LOCKS & MEMORY
    for (int d = 0; d < dCount; d++) omp_destroy_lock(&drone_locks[d]);
    for (int b = 0; b < bCount; b++) omp_destroy_lock(&bakery_locks[b]);
    for (int c = 0; c < cCount; c++) omp_destroy_lock(&customer_locks[c]);
    free(drone_locks);
    free(bakery_locks);
    free(customer_locks);
    
    free(proposals);
    for (int d = 0; d < dCount; d++) free(droneBakeryDist[d]);
    free(droneBakeryDist);
}

void parallelQuickSort(Customer** arr, int left, int right) {
    if (left >= right) return;

    // Threshold: For small arrays, the overhead of thread creation is slower than the sorting itself.
    // Therefore, we fall back to sequential quicksort. 1000 is an excellent magic number for performance.
    if (right - left < 1000) {
        qsort(arr + left, right - left + 1, sizeof(Customer*), compareCustomersDesc);
        return;
    }

    int i = left, j = right;
    
    // Pivot selection (the score of the middle element)
    double pivotScore = arr[left + (right - left) / 2]->tempScore;

    // Partitioning process (sorting in descending order: high scores to the left, low scores to the right)
    while (i <= j) {
        while (arr[i]->tempScore > pivotScore) i++;
        while (arr[j]->tempScore < pivotScore) j--;
        
        if (i <= j) {
            // Swap the pointers
            Customer* temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            i++;
            j--;
        }
    }

    // Split tasks to other threads (Divide and Conquer)
    #pragma omp task shared(arr)
    if (left < j) {
        parallelQuickSort(arr, left, j);
    }

    #pragma omp task shared(arr)
    if (i < right) {
        parallelQuickSort(arr, i, right);
    }
}

// Wrapper function that is called from the outside
void sortCustomersParallel(Customer** customers, int count) {
    // Open a parallel region that creates the Thread Pool
    #pragma omp parallel default(none) shared(customers, count)
    {
        // Define that only one thread (single) starts the initial recursion.
        // This thread will generate Tasks, and the other waiting cores will fetch them and start helping.
        #pragma omp single nowait
        {
            parallelQuickSort(customers, 0, count - 1);
        }
    }
}