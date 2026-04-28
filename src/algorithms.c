#include "../include/algorithms.h"
#include <math.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

double calculateDistance(Position p1, Position p2) {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    return sqrt((dx * dx) + (dy * dy));
}

double calculateFlightTime(double distance, double velocity) {
    if (velocity <= 0.0) return INFINITY;
    return distance / velocity;
}

void updateCustomerPriorities(Customer** customers, int customerCount) {
    #pragma omp parallel for
    for (int i = 0; i < customerCount; i++) {
        if (customers[i]->status == CUSTOMER_ACTIVE) {
            customers[i]->priority += 1;
        }
    }
}

double calculateCustomerScore(int priority, double estimatedTime) {
    if (estimatedTime <= 0.0) return INFINITY;
    return (double)priority / estimatedTime;
}

double** calculateDistanceMatrix(Bakery* bakeries, int bakeryCount, Customer** customers, int customerCount) {
    double** matrix = (double**)malloc(customerCount * sizeof(double*));
    if (matrix == NULL) return NULL;

    for (int i = 0; i < customerCount; i++) {
        matrix[i] = (double*)malloc(bakeryCount * sizeof(double));
        if (matrix[i] == NULL) exit(1);
    }

    #pragma omp parallel for default(none) shared(matrix, customers, customerCount, bakeries, bakeryCount) schedule(dynamic)
    for (int i = 0; i < customerCount; i++) {
        customers[i]->distanceMatrixRow = i;
        for (int j = 0; j < bakeryCount; j++) {
            matrix[i][j] = calculateDistance(customers[i]->pos, bakeries[j].pos);
        }
    }
    return matrix;
}

void freeDistanceMatrix(double** matrix, int customerCount) {
    if (matrix == NULL) return;
    for (int i = 0; i < customerCount; i++) free(matrix[i]);
    free(matrix);
}

void processCustomerTransitions(Customer** customers, int customerCount, int currentRound) {
    #pragma omp parallel for default(none) shared(customers, customerCount, currentRound)
    for (int i = 0; i < customerCount; i++) {
        if (customers[i]->status == CUSTOMER_SERVED) {
            unsigned int local_seed = (unsigned int)time(NULL) ^ omp_get_thread_num() ^ customers[i]->id ^ (unsigned int)currentRound;
            int randomInteger = rand_r(&local_seed);
            double randomProb = (double)randomInteger / RAND_MAX;

            if (randomProb < 0.5) {
                customers[i]->status = CUSTOMER_DEPARTED;
                customers[i]->demand = 0;
                customers[i]->reservedDemand = 0;
            } else {
                customers[i]->status = CUSTOMER_ACTIVE;
                customers[i]->demand = (rand_r(&local_seed) % 5) + 1; 
                customers[i]->reservedDemand = 0; 
                customers[i]->priority = 1;       
            }
        }
    }
}

int compareCustomersDesc(const void* a, const void* b) {
    Customer* c1 = *(Customer**)a;
    Customer* c2 = *(Customer**)b;
    if (c1->tempScore < c2->tempScore) return 1;
    if (c1->tempScore > c2->tempScore) return -1;
    return 0;
}

void calculateDroneAverages(Drone* drones, int droneCount, double* avgVelocity, double* avgCapacity) {
    double sumVel = 0.0, sumCap = 0.0;
    #pragma omp parallel for default(none) shared(drones, droneCount) reduction(+:sumVel, sumCap)
    for (int i = 0; i < droneCount; i++) {
        sumVel += drones[i].velocity;
        sumCap += drones[i].capacity;
    }
    *avgVelocity = sumVel / droneCount;
    *avgCapacity = sumCap / droneCount;
}

void calculateCustomerScoresStage2(Customer** customers, int cCount, double avgVelocity, double avgCapacity) {
    #pragma omp parallel for default(none) shared(customers, cCount, avgVelocity, avgCapacity) schedule(dynamic)
    for (int i = 0; i < cCount; i++) {
        if (customers[i]->status != CUSTOMER_ACTIVE || customers[i]->demand <= 0) {
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

// ====================================================================
// THE REACTIVE LEDGER
// Prevents permanent ghost locks by recalculating reality every round
// ====================================================================
void rebuildCustomerLedger(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount) {
    // 1. Wipe everything clean
    for (int c = 0; c < cCount; c++) customers[c]->reservedDemand = 0;
    for (int b = 0; b < bCount; b++) bakeries[b].reservedInventory = 0;
    
    // 2. Find max fleet velocity
    double maxVel = 0.1;
    for (int d = 0; d < dCount; d++) {
        if (drones[d].velocity > maxVel) maxVel = drones[d].velocity;
    }
    
    // 3. Rebuild based on actual drone physical reality
    for (int d = 0; d < dCount; d++) {
        if (drones[d].routeCount == 0) continue;
        
        // Bakeries MUST track inventory for all drones, otherwise slow drones steal ghost bread
        if (drones[d].currentBakeryId != -1) {
            bakeries[drones[d].currentBakeryId].reservedInventory += drones[d].plannedLoad;
        }
        
        // BACKUP CHECK: Slow drones carry bread, but stay entirely off the customer books!
        if (drones[d].velocity < maxVel * 0.5) continue; 
        
        // Fast drones securely lock their customers
        int breadAvailable = (drones[d].currentBakeryId != -1) ? drones[d].plannedLoad : drones[d].load;
        for (int i = drones[d].currentRouteIdx; i < drones[d].routeCount; i++) {
            Customer* cust = drones[d].route[i];
            if (cust->status != CUSTOMER_ACTIVE) continue;
            
            int needed = cust->demand - cust->reservedDemand;
            if (needed <= 0) continue;
            
            int allocate = (breadAvailable > needed) ? needed : breadAvailable;
            cust->reservedDemand += allocate;
            breadAvailable -= allocate;
            if (breadAvailable <= 0) break;
        }
    }
}

typedef struct {
    int bakeryId;
    int droneId;
    double bestTime;
    int isValid;
} __attribute__((aligned(64))) Proposal;

int assignDronesStage3(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound) {
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

    int matchMade = 0;

    #pragma omp parallel for default(none) shared(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, droneBakeryDist, proposals)
    for (int c = 0; c < cCount; c++) {
        proposals[c].isValid = 0;
        proposals[c].bestTime = INFINITY;

        int remainingNeed = customers[c]->demand - customers[c]->reservedDemand;
        if (customers[c]->status != CUSTOMER_ACTIVE || remainingNeed <= 0) continue;

        int matrixRow = customers[c]->distanceMatrixRow;

        for (int b = 0; b < bCount; b++) {
            int availableBread = bakeries[b].inventory - bakeries[b].reservedInventory;
            if (availableBread <= 0) continue;

            for (int d = 0; d < dCount; d++) {
                if (drones[d].routeCount > 0) continue; 
                if (drones[d].plannedLoad > 0) continue; 

                double flightTime = (droneBakeryDist[d][b] + distanceMatrix[matrixRow][b]) / drones[d].velocity;

                if (flightTime < proposals[c].bestTime) {
                    proposals[c].bestTime = flightTime;
                    proposals[c].bakeryId = b;
                    proposals[c].droneId = d;
                    proposals[c].isValid = 1;
                }
            }
        }
    }

    double maxVel = 0.1;
    for (int i = 0; i < dCount; i++) if (drones[i].velocity > maxVel) maxVel = drones[i].velocity;

    int bestC = -1;
    double minTime = INFINITY;

    for (int c = 0; c < cCount; c++) {
        if (!proposals[c].isValid) continue;

        int dId = proposals[c].droneId;
        int bId = proposals[c].bakeryId;
        
        int remainingNeed = customers[c]->demand - customers[c]->reservedDemand;
        int availableBread = bakeries[bId].inventory - bakeries[bId].reservedInventory;

        if (drones[dId].routeCount == 0 && drones[dId].plannedLoad == 0 &&
            availableBread > 0 && customers[c]->status == CUSTOMER_ACTIVE && remainingNeed > 0) {

            if (proposals[c].bestTime < minTime) {
                minTime = proposals[c].bestTime;
                bestC = c;
            }
        }
    }

    if (bestC != -1) {
        int c = bestC;
        int dId = proposals[c].droneId;
        int bId = proposals[c].bakeryId;
        
        int remainingNeed = customers[c]->demand - customers[c]->reservedDemand;
        int availableBread = bakeries[bId].inventory - bakeries[bId].reservedInventory;

        int breadToTake = remainingNeed;
        if (breadToTake > drones[dId].capacity) breadToTake = drones[dId].capacity;
        if (breadToTake > availableBread) breadToTake = availableBread;

        if (breadToTake > 0) {
            drones[dId].route[0] = customers[c];
            drones[dId].routeCount = 1;
            drones[dId].currentRouteIdx = 0;
            drones[dId].currentBakeryId = bId;

            bakeries[bId].reservedInventory += breadToTake;
            drones[dId].plannedLoad = breadToTake;

            int isBackup = (drones[dId].velocity < maxVel * 0.5);
            if (!isBackup) {
                customers[c]->reservedDemand += breadToTake; // Reserve for this round's while-loop
            }

            int timeTaken = (int)ceil(proposals[c].bestTime);
            if (timeTaken < 1) timeTaken = 1;
            drones[dId].availableAtRound = currentRound + timeTaken;

            matchMade = 1;
        }
    }

    free(proposals);
    for (int d = 0; d < dCount; d++) free(droneBakeryDist[d]);
    free(droneBakeryDist);
    return matchMade;
}

typedef struct {
    int extraCustomerIdx;
    int breadAmount;
    double detourTime;
    int isValid;
} __attribute__((aligned(64))) ExtensionProposal;

int extendTripsMultiCustomer(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, int currentRound) {
    (void)bCount; (void)currentRound;

    ExtensionProposal* proposals = malloc(dCount * sizeof(ExtensionProposal));
    if (proposals == NULL) exit(1);

    int extensionMade = 0;
    double maxVel = 0.1;
    for (int i = 0; i < dCount; i++) if (drones[i].velocity > maxVel) maxVel = drones[i].velocity;

    #pragma omp parallel for default(none) shared(drones, dCount, customers, cCount, bakeries, maxVel, proposals)
    for (int d = 0; d < dCount; d++) {
        proposals[d].isValid = 0;
        proposals[d].detourTime = INFINITY;

        if (drones[d].routeCount == 0 || drones[d].routeCount >= MAX_ROUTE_STOPS) continue; 
        if (drones[d].currentBakeryId < 0) continue; 

        // CRITICAL FIX: Backups are banned from multi-stop routes! They are too unpredictable.
        int isBackup = (drones[d].velocity < maxVel * 0.3);
        if (isBackup) continue;

        int bId = drones[d].currentBakeryId;
        int spareCap = drones[d].capacity - drones[d].plannedLoad;
        if (spareCap <= 0) continue;
        
        int availableBread = bakeries[bId].inventory - bakeries[bId].reservedInventory;
        if (availableBread <= 0) continue;

        Customer* primaryCust = drones[d].route[0];
        double primaryTripDist = calculateDistance(bakeries[bId].pos, primaryCust->pos);
        double primaryTripTime = primaryTripDist / drones[d].velocity;
        if (primaryTripTime <= 0.0) continue;
        double maxDetourTime = 0.75 * primaryTripTime;

        Customer* lastCust = drones[d].route[drones[d].routeCount - 1];

        for (int c = 0; c < cCount; c++) {
            Customer* cust = customers[c];
            
            int remainingNeed = cust->demand - cust->reservedDemand;
            if (cust->status != CUSTOMER_ACTIVE || remainingNeed <= 0) continue;
            
            int alreadyInRoute = 0;
            for(int r = 0; r < drones[d].routeCount; r++) {
                if (drones[d].route[r] == cust) alreadyInRoute = 1;
            }
            if (alreadyInRoute) continue;

            double detourDist = calculateDistance(lastCust->pos, cust->pos);
            double detourTime = detourDist / drones[d].velocity;
            if (detourTime > maxDetourTime) continue;

            if (detourTime < proposals[d].detourTime) {
                int amount = (remainingNeed < spareCap) ? remainingNeed : spareCap;
                if (amount > availableBread) amount = availableBread;
                if (amount <= 0) continue;

                proposals[d].detourTime = detourTime;
                proposals[d].extraCustomerIdx = c;
                proposals[d].breadAmount = amount;
                proposals[d].isValid = 1;
            }
        }
    }

    for (int d = 0; d < dCount; d++) {
        if (!proposals[d].isValid) continue;

        int bId = drones[d].currentBakeryId;
        int cIdx = proposals[d].extraCustomerIdx;
        Customer* cust = customers[cIdx];

        int remainingNeed = cust->demand - cust->reservedDemand;
        int availableBread = bakeries[bId].inventory - bakeries[bId].reservedInventory;
        int spareCap = drones[d].capacity - drones[d].plannedLoad;

        if (cust->status != CUSTOMER_ACTIVE || remainingNeed <= 0 || availableBread <= 0 || spareCap <= 0) continue;

        int amount = proposals[d].breadAmount;
        if (amount > remainingNeed)  amount = remainingNeed;
        if (amount > spareCap)       amount = spareCap;
        if (amount > availableBread) amount = availableBread;
        if (amount <= 0) continue;

        bakeries[bId].reservedInventory += amount;
        cust->reservedDemand += amount; // We know it's not a backup because they were skipped!
        drones[d].plannedLoad += amount;
        
        drones[d].route[drones[d].routeCount] = cust;
        drones[d].routeCount++;
        drones[d].availableAtRound += (int)ceil(proposals[d].detourTime);
        extensionMade = 1;
    }
    free(proposals);
    return extensionMade;
}

void parallelQuickSort(Customer** arr, int left, int right) {
    if (left >= right) return;
    if (right - left < 1000) {
        qsort(arr + left, right - left + 1, sizeof(Customer*), compareCustomersDesc);
        return;
    }

    int i = left, j = right;
    double pivotScore = arr[left + (right - left) / 2]->tempScore;

    while (i <= j) {
        while (arr[i]->tempScore > pivotScore) i++;
        while (arr[j]->tempScore < pivotScore) j--;
        if (i <= j) {
            Customer* temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            i++; j--;
        }
    }

    if (left < j) {
        #pragma omp task firstprivate(arr, left, j)
        parallelQuickSort(arr, left, j);
    }
    if (i < right) {
        #pragma omp task firstprivate(arr, i, right)
        parallelQuickSort(arr, i, right);
    }
}

void sortCustomersParallel(Customer** customers, int count) {
    #pragma omp parallel default(none) shared(customers, count)
    {
        #pragma omp single nowait
        parallelQuickSort(customers, 0, count - 1);
    }
}