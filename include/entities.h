#ifndef ENTITIES_H
#define ENTITIES_H

typedef struct {
    double x;
    double y;
} Position;

typedef enum {
    CUSTOMER_ACTIVE,
    CUSTOMER_SERVED,
    CUSTOMER_DEPARTED
} CustomerStatus;

typedef struct {
    int probability;
    int breadCount;
} ProductionRule;

typedef struct {
    int id;
    Position pos;
    int inventory;
    int reservedInventory; // Accounting Ledger
    int capacity;
    int ruleCount;
    unsigned int seed;
    ProductionRule* cumulativeProb;
} Bakery;

typedef struct {
    int id;
    Position pos;
    int demand;
    int reservedDemand;    // Accounting Ledger
    int priority;
    CustomerStatus status;
    double closestBakeryDistance;
    double tempScore;
    int distanceMatrixRow;
} Customer;

typedef struct {
    int id;
    Position pos;
    int load;              // Physical bread inside the drone
    int plannedLoad;       // Accounting Ledger
    int capacity;
    double velocity;
    int availableAtRound;
    Customer* currentCustomer;
    Customer* secondaryCustomer; // For physical multi-stop VRP routes
    int currentBakeryId;
} Drone;

typedef struct {
    Position pos;
    Drone* drones;
    int droneCount;
} DroneBase;

#endif