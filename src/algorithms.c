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
typedef struct {
    int bakeryId;
    int droneId;
    double bestTime;
    int isValid;
} Proposal;

// Assigns drones to customers, planning the route and updating inventory.
void assignDronesStage3(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound) {
    
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
            if (customers[c]->status != CUSTOMER_ACTIVE || customers[c]->demand == 0) continue;

            // Use the customer's stable row index to look up the correct distance matrix row,
            // since qsort may have reordered the customers array.
            int matrixRow = customers[c]->distanceMatrixRow;

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
                    
                    drones[dId].currentCustomer = customers[c];
                    drones[dId].currentBakeryId = bId; // Remember the pickup bakery for Stage 3.5
                    
                    // Calculate how much bread the drone takes
                    int breadToTake = (customers[c]->demand < drones[dId].capacity) ? customers[c]->demand : drones[dId].capacity;
                    if (breadToTake > bakeries[bId].inventory) breadToTake = bakeries[bId].inventory;
                    
                    // Update Bakery and Customer
                    bakeries[bId].inventory -= breadToTake;
                    customers[c]->demand -= breadToTake;
                    
                    // Update Drone status (Delivery Execution)
                    int timeTaken = (int)ceil(proposals[c].bestTime);
                    drones[dId].availableAtRound = currentRound + timeTaken;
                    drones[dId].pos = customers[c]->pos;
                    drones[dId].load = breadToTake;

                    if (customers[c]->demand == 0) {
                        customers[c]->status = CUSTOMER_SERVED;
                    }

                    /*printf("  -> Assigned Drone %d to Customer %d (Takes %d bread from Bakery %d, Arrives at round %d)\n",
                           drones[dId].id, customers[c]->id, breadToTake, bakeries[bId].id, drones[dId].availableAtRound);*/
                }
            }
        }
    }
    // Cleanup memory
    free(proposals);
    for (int d = 0; d < dCount; d++) free(droneBakeryDist[d]);
    free(droneBakeryDist);
}

/*
 * Stage 3.5 — Multi-Customer Trip Extension.
 *
 * Lets a single drone serve several customers on the same trip, when capacity
 * and bakery inventory allow it. This is the natural counterpart to the
 * order-splitting that assignDronesStage3 already supports: that function
 * splits ONE big order across multiple drones; this one packs SEVERAL small
 * orders onto the same drone.
 *
 * Internal proposal record. One slot per drone — droneId is implicit (the
 * array index), keeping the parallel write pattern lock-free.
 */
typedef struct {
    int extraCustomerIdx; // Index into customers[] for the proposed extra stop
    int breadAmount;      // How much bread the drone will pick up & deliver to that customer
    double detourTime;    // Extra time the trip takes (drone.velocity-scaled)
    Position newDronePos; // Drone's end-of-trip position after this extra stop
    int isValid;          // 1 if a viable extension was found
} ExtensionProposal;

void extendTripsMultiCustomer(Customer** customers, int cCount, Bakery* bakeries, int bCount,
                              Drone* drones, int dCount, int currentRound) {
    (void)bCount;       // bCount is only used for the assertion that currentBakeryId is in range; silence unused warn
    (void)currentRound; // Reserved for time-window / SLA-aware extensions (see Section 7 of the spec)

    ExtensionProposal* proposals = malloc(dCount * sizeof(ExtensionProposal));
    if (proposals == NULL) exit(1);

    int extensionMade = 1;

    // Loop until no drone can be extended further. Each iteration may add one extra
    // customer per busy drone; long routes are built up across iterations.
    while (extensionMade) {
        extensionMade = 0;

        // --- Phase A: Parallel Search ---
        //
        // SYNCHRONIZATION NOTE: This region only READS shared state (drones, bakeries,
        // customers) and WRITES exclusively to its own slot proposals[d]. The implicit
        // barrier at the end of the parallel-for guarantees Phase B sees all proposals
        // before any commitment happens. The same invariant as assignDronesStage3 —
        // do NOT merge the two phases.
        //
        #pragma omp parallel for default(none) \
            shared(drones, dCount, customers, cCount, bakeries, currentRound, proposals)
        for (int d = 0; d < dCount; d++) {
            proposals[d].isValid = 0;
            proposals[d].detourTime = INFINITY;

            // Skip drones that are not currently on a trip from a known bakery.
            // (idle drones, or drones whose trip already finished, have nothing to extend)
            if (drones[d].currentCustomer == NULL) continue;
            if (drones[d].currentBakeryId < 0) continue;

            int bId = drones[d].currentBakeryId;
            int spareCap = drones[d].capacity - drones[d].load;

            // Need both spare capacity on the drone and stock at the originating bakery.
            // The bakery check is a fast prefilter — Phase B re-checks before commit.
            if (spareCap <= 0) continue;
            if (bakeries[bId].inventory <= 0) continue;
            // ==========================================
            // NEW LOGIC: Calculate dynamic detour threshold
            // ==========================================
            // How long did it take to get from the bakery to where the drone currently is?
            double primaryTripDist = calculateDistance(bakeries[bId].pos, drones[d].pos);
            double primaryTripTime = primaryTripDist / drones[d].velocity;
            
            // Set K = 0.75. The detour cannot take more than 75% of the primary trip time.
            double maxDetourTime = 0.75 * primaryTripTime; 
            // ==========================================
            // Greedy nearest-neighbour: the cheapest extension is the active customer
            // closest to the drone's current end-of-trip position (drones[d].pos).
            for (int c = 0; c < cCount; c++) {
                Customer* cust = customers[c];
                if (cust->status != CUSTOMER_ACTIVE) continue;
                if (cust->demand <= 0) continue;
                if (cust == drones[d].currentCustomer) continue; // already this drone's primary

                double detourDist = calculateDistance(drones[d].pos, cust->pos);
                double detourTime = detourDist / drones[d].velocity;
                if (detourTime > maxDetourTime) {
                    continue; // Detour is too long relative to the base trip! Skip.
                }

                if (detourTime < proposals[d].detourTime) {
                    int amount = (cust->demand < spareCap) ? cust->demand : spareCap;
                    if (amount > bakeries[bId].inventory) amount = bakeries[bId].inventory;
                    if (amount <= 0) continue;

                    proposals[d].detourTime = detourTime;
                    proposals[d].extraCustomerIdx = c;
                    proposals[d].breadAmount = amount;
                    proposals[d].newDronePos = cust->pos;
                    proposals[d].isValid = 1;
                }
            }
        }

        // --- Phase B: Sequential Commitment ---
        //
        // Walk drones in order, committing valid extensions. Re-validate every
        // shared resource (bakery inventory, customer status, drone capacity)
        // because earlier commits in this same loop may have consumed them.
        for (int d = 0; d < dCount; d++) {
            if (!proposals[d].isValid) continue;

            int bId = drones[d].currentBakeryId;
            int cIdx = proposals[d].extraCustomerIdx;
            Customer* cust = customers[cIdx];

            // Re-check: another drone (lower-d in this same Phase B pass) may have
            // already grabbed this customer's last bread or drained the bakery.
            if (cust->status != CUSTOMER_ACTIVE) continue;
            if (cust->demand <= 0) continue;
            if (bakeries[bId].inventory <= 0) continue;

            int spareCap = drones[d].capacity - drones[d].load;
            if (spareCap <= 0) continue;

            int amount = proposals[d].breadAmount;
            if (amount > cust->demand)            amount = cust->demand;
            if (amount > spareCap)                amount = spareCap;
            if (amount > bakeries[bId].inventory) amount = bakeries[bId].inventory;
            if (amount <= 0) continue;

            // Commit: take more bread at the originating bakery, append the extra stop.
            // (Conceptually the drone picks up the extra bread before leaving the bakery,
            //  then delivers in-order along the route. The simulation state tracks only
            //  the final outcome, so we update inventories and end-positions directly.)
            bakeries[bId].inventory -= amount;
            cust->demand            -= amount;
            drones[d].load          += amount;
            drones[d].pos            = proposals[d].newDronePos;
            drones[d].availableAtRound += (int)ceil(proposals[d].detourTime);

            if (cust->demand == 0) {
                cust->status = CUSTOMER_SERVED;
            }

            extensionMade = 1;
        }
    }

    free(proposals);
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

    // --- THE FIX ---
    // 1. Move 'if' OUTSIDE the #pragma task to prevent "Phantom Tasks"
    // 2. Use 'firstprivate' instead of 'shared' to prevent "Use-After-Free" crashes
    
    if (left < j) {
        #pragma omp task firstprivate(arr, left, j)
        parallelQuickSort(arr, left, j);
    }

    if (i < right) {
        #pragma omp task firstprivate(arr, i, right)
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
