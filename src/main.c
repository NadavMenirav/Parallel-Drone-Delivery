#include "../include/entities.h"
#include "../include/algorithms.h"

#include <omp.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <string.h>

void produceBread(Bakery* bakeries, int bakeryCount);
void updateDrones(Drone* drones, int droneCount, int currentRound, Bakery* bakeries, int bakeryCount);
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer*** customers, int* cCount, int mockType);
void findClosestBakery(Bakery* bakeries, int bCount, Customer** customers, int cCount, double** distanceMatrix);
Bakery* closestBakeryToDrone(Bakery* bakeries, int bakeryCount, const Drone* drone, double* closestDistance);
void idleDroneRepositioning(Bakery* bakeries, int bakeryCount, Drone* drone);

void produceBread(Bakery* bakeries, const int bakeryCount) {
    #pragma omp parallel for default(none) shared(bakeries, bakeryCount)
    for (int i = 0; i < bakeryCount; i++) {
        const int randomInteger = rand_r(&bakeries[i].seed);
        const double random = (double) randomInteger / RAND_MAX;

        int j;
        for (j = 0; j < bakeries[i].ruleCount; j++) {
            if (bakeries[i].cumulativeProb[j].probability >= random) break;
        }
        if (j == bakeries[i].ruleCount) j = bakeries[i].ruleCount - 1;

        const int breadProduction = bakeries[i].cumulativeProb[j].breadCount;
        const int newInventory = bakeries[i].inventory + breadProduction;
        bakeries[i].inventory = (newInventory < bakeries[i].capacity) ? newInventory : bakeries[i].capacity;
    }
}

void updateDrones(Drone* drones, const int droneCount, const int currentRound, Bakery* bakeries, const int bakeryCount) {
    #pragma omp parallel for default(none) shared(drones, droneCount, currentRound, bakeries, bakeryCount)
    for (int i = 0; i < droneCount; i++) {
        Drone* drone = &drones[i];

        // Leg 1: Fly to the Bakery First!
        if (drone->currentBakeryId != -1) {
            Bakery* bakery = &bakeries[drone->currentBakeryId];
            double distance = calculateDistance(drone->pos, bakery->pos);

            if (distance <= drone->velocity || distance < 0.0001) {
                drone->pos = bakery->pos;
                
                // PHYSICAL TRANSFER FROM BAKERY TO DRONE
                bakery->inventory -= drone->plannedLoad;
                bakery->reservedInventory -= drone->plannedLoad;
                drone->load = drone->plannedLoad;
                
                drone->currentBakeryId = -1; // Done with bakery leg
            } else {
                double ratio = drone->velocity / distance;
                drone->pos.x += (bakery->pos.x - drone->pos.x) * ratio;
                drone->pos.y += (bakery->pos.y - drone->pos.y) * ratio;
            }
            continue; // Wait until next round to fly to customer
        }

        // Leg 2 & 3: Fly to Customers
        if (drone->currentCustomer != NULL) {
            Customer* customer = drone->currentCustomer;
            Position target = customer->pos;
            double distance = calculateDistance(drone->pos, target);

            if (distance <= drone->velocity || distance < 0.0001) {
                drone->pos = target;

                // PHYSICAL TRANSFER FROM DRONE TO CUSTOMER
                int dropAmount = (drone->load > customer->demand) ? customer->demand : drone->load;
                customer->demand -= dropAmount;
                customer->reservedDemand -= dropAmount;
                drone->load -= dropAmount;

                if (customer->demand <= 0) {
                    customer->demand = 0;
                    customer->status = CUSTOMER_SERVED;
                }

                // Multi-stop routing
                if (drone->secondaryCustomer != NULL) {
                    drone->currentCustomer = drone->secondaryCustomer;
                    drone->secondaryCustomer = NULL;
                    continue; 
                }

                drone->currentCustomer = NULL;
                drone->plannedLoad = 0;
                drone->availableAtRound = currentRound;

                idleDroneRepositioning(bakeries, bakeryCount, drone);
                continue;
            }

            // Still flying
            double ratio = drone->velocity / distance;
            if (ratio > 1.0) ratio = 1.0;
            drone->pos.x += (target.x - drone->pos.x) * ratio;
            drone->pos.y += (target.y - drone->pos.y) * ratio;
            continue;
        }

        // Idle drone moves slowly toward closest bakery
        if (drone->availableAtRound <= currentRound) {
            drone->availableAtRound = currentRound;
            drone->currentCustomer = NULL;
            drone->secondaryCustomer = NULL;
            drone->currentBakeryId = -1;
            drone->load = 0;
            drone->plannedLoad = 0;
            idleDroneRepositioning(bakeries, bakeryCount, drone);
        }
    }
}

Bakery* closestBakeryToDrone(Bakery* bakeries, const int bakeryCount, const Drone* drone, double* closestDistance) {
    double closest = DBL_MAX, currentDistance = DBL_MAX;
    Bakery* closestBakery = NULL, *currentBakery = NULL;

    for (int i = 0; i < bakeryCount; i++) {
        currentBakery = &bakeries[i];
        currentDistance = calculateDistance(currentBakery->pos, drone->pos);
        if (currentDistance < closest) {
            closest = currentDistance;
            closestBakery = currentBakery;
        }
    }
    if (closestDistance) *closestDistance = closest;
    return closestBakery;
}

void idleDroneRepositioning(Bakery* bakeries, const int bakeryCount, Drone* drone) {
    double closestDistance = 0.0;
    const Bakery* closestBakery = closestBakeryToDrone(bakeries, bakeryCount, drone, &closestDistance);
    if (closestBakery == NULL || closestDistance < 0) return;

    double ratio = drone->velocity / closestDistance;
    ratio = ratio <= 1 ? ratio : 1;

    const double dx = closestBakery->pos.x - drone->pos.x;
    const double dy = closestBakery->pos.y - drone->pos.y;

    drone->pos.x += dx * ratio;
    drone->pos.y += dy * ratio;
}

void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount,
                    Customer*** customers, int* cCount, int mockType) {

    *bCount = 1;
    *dCount = 2;
    *cCount = 2;

    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones = (Drone*) malloc(sizeof(Drone) * (*dCount));
    *customers = (Customer**) malloc(sizeof(Customer*) * (*cCount));

    if (*bakeries == NULL || *drones == NULL || *customers == NULL) {
        exit(1);
    }

    (*bakeries)[0].id = 1;
    (*bakeries)[0].pos.x = 100.0;
    (*bakeries)[0].pos.y = 100.0;
    (*bakeries)[0].inventory = 0;
    (*bakeries)[0].capacity = 100;
    (*bakeries)[0].seed = 42;
    (*bakeries)[0].ruleCount = 1;

    (*bakeries)[0].cumulativeProb = (ProductionRule*) malloc(sizeof(ProductionRule));
    if ((*bakeries)[0].cumulativeProb == NULL) {
        exit(1);
    }

    (*bakeries)[0].cumulativeProb[0].probability = 1.0;
    (*bakeries)[0].cumulativeProb[0].breadCount = 8;

    double droneStart[2][2] = {
        {20.0, 20.0},
        {180.0, 20.0}
    };

    double droneVelocity[2] = {8.0, 8.0};
    int droneCapacity[2] = {5, 5};

    double customerPositions[2][2] = {
        {30.0, 185.0},
        {175.0, 180.0}
    };

    int customerDemand[2];

    if (mockType == 1) {
        customerDemand[0] = 4;
        customerDemand[1] = 4;

    } else if (mockType == 2) {
        customerDemand[0] = 9;
        customerDemand[1] = 2;

    } else {
        // Mock 3: one strong drone should serve both customers.
        customerDemand[0] = 2;
        customerDemand[1] = 2;

        customerPositions[0][0] = 125.0;
        customerPositions[0][1] = 165.0;
        customerPositions[1][0] = 145.0;
        customerPositions[1][1] = 175.0;

        // Good drone: starts near bakery, fast, enough capacity for both.
        droneStart[0][0] = 100.0;
        droneStart[0][1] = 100.0;
        droneVelocity[0] = 10.0;
        droneCapacity[0] = 5;

        // Bad drone: far and very slow.
        droneStart[1][0] = 10.0;
        droneStart[1][1] = 10.0;
        droneVelocity[1] = 0.5;
        droneCapacity[1] = 5;
    }

    for (int i = 0; i < *dCount; i++) {
        (*drones)[i].id = i + 1;
        (*drones)[i].pos.x = droneStart[i][0];
        (*drones)[i].pos.y = droneStart[i][1];
        (*drones)[i].velocity = droneVelocity[i];
        (*drones)[i].capacity = droneCapacity[i];
        (*drones)[i].load = 0;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
        (*drones)[i].currentBakeryId = -1;
    }

    for (int i = 0; i < *cCount; i++) {
        (*customers)[i] = (Customer*) malloc(sizeof(Customer));
        if ((*customers)[i] == NULL) {
            exit(1);
        }

        (*customers)[i]->id = i + 1;
        (*customers)[i]->pos.x = customerPositions[i][0];
        (*customers)[i]->pos.y = customerPositions[i][1];
        (*customers)[i]->priority = 1;
        (*customers)[i]->status = CUSTOMER_ACTIVE;
        (*customers)[i]->demand = customerDemand[i];
        (*customers)[i]->reservedDemand = 0;
        (*customers)[i]->closestBakeryDistance = DBL_MAX;
        (*customers)[i]->tempScore = -1.0;
        (*customers)[i]->distanceMatrixRow = -1;
    }
}
void initSystemStress(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer*** customers, int* cCount) {
    *bCount = 500;
    *dCount = 50;
    *cCount = 10000;

    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones   = (Drone*)  malloc(sizeof(Drone)  * (*dCount));
    *customers = (Customer**) malloc(sizeof(Customer*) * (*cCount));
    if (*bakeries == NULL || *drones == NULL || *customers == NULL) exit(1);

    typedef struct { double x, y; int capacity; int rules[3][2]; } BakeryDef;
    BakeryDef bDefs[] = {
        {  20.0,  30.0, 150, { {5, 30}, {12, 80}, {25, 100} } },
        { 180.0,  20.0,  80, { {3, 40}, { 8, 85}, {15, 100} } },
        { 100.0, 100.0, 200, { {8, 25}, {15, 70}, {30, 100} } },
        {  40.0, 180.0,  60, { {2, 50}, { 6, 90}, {10, 100} } },
        { 160.0, 170.0, 100, { {4, 35}, {10, 75}, {18, 100} } },
    };

    for (int i = 0; i < *bCount; i++) {
        int bIdx = i % 5;
        (*bakeries)[i].id = i + 1;
        (*bakeries)[i].pos.x = bDefs[bIdx].x;
        (*bakeries)[i].pos.y = bDefs[bIdx].y;
        (*bakeries)[i].inventory = 0;
        (*bakeries)[i].reservedInventory = 0;
        (*bakeries)[i].capacity = bDefs[bIdx].capacity;
        (*bakeries)[i].seed = 12345 + i;
        (*bakeries)[i].ruleCount = 3;

        (*bakeries)[i].cumulativeProb = (ProductionRule*)malloc(3 * sizeof(ProductionRule));
        if ((*bakeries)[i].cumulativeProb == NULL) exit(1);

        for (int r = 0; r < 3; r++) {
            (*bakeries)[i].cumulativeProb[r].breadCount = bDefs[bIdx].rules[r][0];
            (*bakeries)[i].cumulativeProb[r].probability = bDefs[bIdx].rules[r][1] / 100.0;
        }
    }

    typedef struct { double x, y, velocity; int capacity; } DroneDef;
    DroneDef dDefs[] = {
        {  20.0,  50.0, 3.0, 4 }, {  20.0,  50.0, 8.0, 8 }, {  25.0,  55.0, 2.5, 5 },
        {  15.0,  45.0, 5.0, 3 }, {  20.0,  50.0, 11.0, 6 }, { 160.0, 160.0, 3.5, 3 },
        { 160.0, 160.0, 1.8, 7 }, { 155.0, 165.0, 2.2, 6 }, { 165.0, 155.0, 7.8, 4 },
        { 160.0, 160.0, 5.2, 10 }
    };

    for (int i = 0; i < *dCount; i++) {
        int dIdx = i % 10;
        (*drones)[i].id = i + 1;
        (*drones)[i].pos.x = dDefs[dIdx].x;
        (*drones)[i].pos.y = dDefs[dIdx].y;
        (*drones)[i].velocity = dDefs[dIdx].velocity;
        (*drones)[i].capacity = dDefs[dIdx].capacity;
        (*drones)[i].load = 0;
        (*drones)[i].plannedLoad = 0;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
        (*drones)[i].secondaryCustomer = NULL;
        (*drones)[i].currentBakeryId = -1;
    }

    typedef struct { double x, y; int demand; } CustomerDef;
    CustomerDef cDefs[] = {
        {  10.0,  15.0,  3 }, {  35.0,  25.0,  5 }, {  90.0,  85.0,  4 }, { 110.0, 115.0,  7 },
        { 105.0,  90.0,  2 }, { 190.0,   5.0,  6 }, { 195.0,  15.0,  8 }, {  15.0, 190.0,  4 },
        {  30.0, 195.0, 10 }, {  50.0, 175.0,  3 }, {  70.0,  50.0,  5 }, { 130.0,  60.0,  6 },
        {  60.0, 130.0,  4 }, { 140.0, 130.0,  3 }, {   5.0, 100.0,  9 }, { 195.0, 100.0,  7 },
        { 100.0,   5.0,  2 }, { 100.0, 195.0,  5 }, { 170.0, 185.0,  8 }, { 150.0, 190.0,  6 }
    };

    srand(9999);
    for (int i = 0; i < *cCount; i++) {
        int cIdx = i % 20;
        (*customers)[i] = (Customer*) malloc(sizeof(Customer));
        if ((*customers)[i] == NULL) exit(1);

        (*customers)[i]->id = i + 1;
        (*customers)[i]->pos.x = cDefs[cIdx].x + ((rand() % 10) - 5);
        (*customers)[i]->pos.y = cDefs[cIdx].y + ((rand() % 10) - 5);
        (*customers)[i]->priority = 1;
        (*customers)[i]->status = CUSTOMER_ACTIVE;
        (*customers)[i]->demand = cDefs[cIdx].demand;
        (*customers)[i]->reservedDemand = 0;
        (*customers)[i]->closestBakeryDistance = DBL_MAX;
        (*customers)[i]->tempScore = -1.0;
        (*customers)[i]->distanceMatrixRow = -1;
    }
}

void findClosestBakery(Bakery* bakeries, const int bCount, Customer** customers, const int cCount, double** distanceMatrix) {
    #pragma omp parallel for default(none) shared(cCount, bCount, distanceMatrix, bakeries, customers)
    for (int i = 0; i < cCount; i++) {
        customers[i]->closestBakeryDistance = DBL_MAX;
        int matrixRow = customers[i]->distanceMatrixRow;
        for (int j = 0; j < bCount; j++) {
            if (distanceMatrix[matrixRow][j] < customers[i]->closestBakeryDistance) {
                customers[i]->closestBakeryDistance = distanceMatrix[matrixRow][j];
            }
        }
    }
}

int main(int argc, char* argv[]) {

    // =========================================================================
    // UI EXPORT MODE
    // =========================================================================
    if (argc > 1 && strcmp(argv[1], "--export") == 0) {
        int maxRounds = 100;

        omp_set_dynamic(0);
        omp_set_num_threads(omp_get_max_threads());

        Bakery* bakeries;
        Drone* drones;
        Customer** customers;
        int bCount, dCount, cCount;

        int mockType = 1;

        if (argc > 2) {
            if (strcmp(argv[2], "mock1") == 0) mockType = 1;
            else if (strcmp(argv[2], "mock2") == 0) mockType = 2;
            else if (strcmp(argv[2], "mock3") == 0) mockType = 3;
        }

        initSystemMock(&bakeries, &bCount, &drones, &dCount, &customers, &cCount, mockType);

        double** distanceMatrix = calculateDistanceMatrix(bakeries, bCount, customers, cCount);
        if (distanceMatrix == NULL) exit(1);

        findClosestBakery(bakeries, bCount, customers, cCount, distanceMatrix);

        FILE* jsonFile = fopen("state.json", "w");
        if (jsonFile) fprintf(jsonFile, "[\n");

        int t = 1;
        while (t <= maxRounds) {
            produceBread(bakeries, bCount);
            updateDrones(drones, dCount, t, bakeries, bCount);

            double avgVelocity = 0.0, avgCapacity = 0.0;
            calculateDroneAverages(drones, dCount, &avgVelocity, &avgCapacity);
            calculateCustomerScoresStage2(customers, cCount, avgVelocity, avgCapacity);
            sortCustomersParallel(customers, cCount);

            assignDronesStage3(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, t);
            extendTripsMultiCustomer(customers, cCount, bakeries, bCount, drones, dCount, t);

            updateCustomerPriorities(customers, cCount);
            processCustomerTransitions(customers, cCount, t);

            if (jsonFile != NULL) {
                if (t > 1) fprintf(jsonFile, ",\n");
                fprintf(jsonFile, "  {\n    \"round\": %d,\n", t);

                fprintf(jsonFile, "    \"bakeries\": [\n");
                for (int b = 0; b < bCount; b++) {
                    fprintf(jsonFile, "      {\"id\": %d, \"pos\": {\"x\": %f, \"y\": %f}, \"inventory\": %d, \"reserved\": %d, \"capacity\": %d}%s\n",
                        bakeries[b].id, bakeries[b].pos.x, bakeries[b].pos.y, bakeries[b].inventory, bakeries[b].reservedInventory, bakeries[b].capacity,
                        (b == bCount - 1) ? "" : ",");
                }
                fprintf(jsonFile, "    ],\n");

                fprintf(jsonFile, "    \"customers\": [\n");
                for (int c = 0; c < cCount; c++) {
                    fprintf(jsonFile, "      {\"id\": %d, \"pos\": {\"x\": %f, \"y\": %f}, \"demand\": %d, \"reserved\": %d, \"priority\": %d, \"status\": \"%s\"}%s\n",
                        customers[c]->id, customers[c]->pos.x, customers[c]->pos.y, customers[c]->demand, customers[c]->reservedDemand, customers[c]->priority,
                        (customers[c]->status == CUSTOMER_ACTIVE) ? "CUSTOMER_ACTIVE" :
                        (customers[c]->status == CUSTOMER_SERVED) ? "CUSTOMER_SERVED" : "CUSTOMER_DEPARTED",
                        (c == cCount - 1) ? "" : ",");
                }
                fprintf(jsonFile, "    ],\n");

                fprintf(jsonFile, "    \"drones\": [\n");
                for (int d = 0; d < dCount; d++) {
                    fprintf(jsonFile,
                        "      {\"id\": %d, \"pos\": {\"x\": %f, \"y\": %f}, "
                        "\"load\": %d, \"capacity\": %d, \"availableAtRound\": %d, "
                        "\"status\": \"%s\", \"targetCustomerId\": %d, \"currentBakeryId\": %d}%s\n",
                        drones[d].id,
                        drones[d].pos.x,
                        drones[d].pos.y,
                        drones[d].load, // This is now 100% physical!
                        drones[d].capacity,
                        drones[d].availableAtRound,
                        drones[d].currentCustomer == NULL ? "IDLE" : "DELIVERING",
                        drones[d].currentCustomer == NULL ? -1 : drones[d].currentCustomer->id,
                        drones[d].currentBakeryId,
                        (d == dCount - 1) ? "" : ","
                    );
                }
                fprintf(jsonFile, "    ]\n  }");
            }

            int allDeparted = 1;
            for (int j = 0; j < cCount; j++) {
                if (customers[j]->status != CUSTOMER_DEPARTED) {
                    allDeparted = 0;
                    break;
                }
            }
            if (allDeparted) break;

            t++;
        }

        if (jsonFile != NULL) {
            fprintf(jsonFile, "\n]\n");
            fclose(jsonFile);
        }

        freeDistanceMatrix(distanceMatrix, cCount);
        for (int b = 0; b < bCount; b++) free(bakeries[b].cumulativeProb);
        free(bakeries);
        free(drones);
        for (int c = 0; c < cCount; c++) free(customers[c]);
        free(customers);

        return 0;
    }

    // =========================================================================
    // STRESS BENCHMARK MODE
    // =========================================================================
    printf("==================================================\n");
    printf("   DRONE DELIVERY SIMULATION - STRESS BENCHMARK   \n");
    printf("==================================================\n");

    int threadCounts[] = {1, 6, 12};
    int numTests = sizeof(threadCounts) / sizeof(threadCounts[0]);
    int maxRounds = 10;
    printf("+---------+--------------+----------+\n");
    printf("| Threads | Time (sec)   | Speedup  |\n");
    printf("+---------+--------------+----------+\n");

    double baseTime = 0.0;

    for (int i = 0; i < numTests; i++) {
        int threads = threadCounts[i];

        omp_set_dynamic(0);
        omp_set_num_threads(threads);

        Bakery* bakeries;
        Drone* drones;
        Customer** customers;
        int bCount, dCount, cCount;

        initSystemStress(&bakeries, &bCount, &drones, &dCount, &customers, &cCount);

        double** distanceMatrix = calculateDistanceMatrix(bakeries, bCount, customers, cCount);
        if (distanceMatrix == NULL) {
            printf("Failed to allocate distance matrix!\n");
            exit(1);
        }

        findClosestBakery(bakeries, bCount, customers, cCount, distanceMatrix);

        double startTime = omp_get_wtime();

        int t = 1;
        while (t <= maxRounds) {
            produceBread(bakeries, bCount);
            updateDrones(drones, dCount, t, bakeries, bCount);

            double avgVelocity = 0.0, avgCapacity = 0.0;
            calculateDroneAverages(drones, dCount, &avgVelocity, &avgCapacity);
            calculateCustomerScoresStage2(customers, cCount, avgVelocity, avgCapacity);

            sortCustomersParallel(customers, cCount);

            assignDronesStage3(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, t);
            extendTripsMultiCustomer(customers, cCount, bakeries, bCount, drones, dCount, t);

            updateCustomerPriorities(customers, cCount);
            processCustomerTransitions(customers, cCount, t);

            int allDeparted = 1;
            for (int j = 0; j < cCount; j++) {
                if (customers[j]->status != CUSTOMER_DEPARTED) {
                    allDeparted = 0;
                    break;
                }
            }
            if (allDeparted) break;

            t++;
        }

        double elapsed = omp_get_wtime() - startTime;

        if (i == 0) baseTime = elapsed;
        double speedup = baseTime / elapsed;

        printf("| %-7d | %-12.4f | %-8.2fx |\n", threads, elapsed, speedup);

        freeDistanceMatrix(distanceMatrix, cCount);
        for (int b = 0; b < bCount; b++) free(bakeries[b].cumulativeProb);
        free(bakeries);
        free(drones);
        for (int c = 0; c < cCount; c++) free(customers[c]);
        free(customers);
    }

    printf("+---------+--------------+----------+\n");
    printf("Benchmark Completed Successfully.\n");

    return 0;
}