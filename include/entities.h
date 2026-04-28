#ifndef ENTITIES_H
#define ENTITIES_H

#define MAX_ROUTE_STOPS 10 // Maximum stops a drone can chain together on one trip

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

// ALIGNED TO 64 BYTES TO PREVENT FALSE SHARING IN ARRAYS
typedef struct {
    int id;
    Position pos;
    int load;              // Physical bread inside the drone
    int plannedLoad;       // Accounting Ledger
    int capacity;
    double velocity;
    int availableAtRound;
    
    Customer* route[MAX_ROUTE_STOPS]; // The Static Route Queue
    int routeCount;                   // Total stops planned
    int currentRouteIdx;              // Which stop the drone is currently flying to
    
    int currentBakeryId;
} __attribute__((aligned(64))) Drone;

typedef struct {
    Position pos;
    Drone* drones;
    int droneCount;
} DroneBase;

#endif