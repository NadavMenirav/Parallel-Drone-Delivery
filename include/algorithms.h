#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "entities.h"

double calculateDistance(Position p1, Position p2);
double calculateFlightTime(double distance, double velocity);
void updateCustomerPriorities(Customer** customers, int customerCount);
double calculateCustomerScore(int priority, double estimatedTime);
double** calculateDistanceMatrix(Bakery* bakeries, int bakeryCount, Customer** customers, int customerCount);
void freeDistanceMatrix(double** matrix, int customerCount);
void processCustomerTransitions(Customer** customers, int customerCount, int currentRound);
void calculateDroneAverages(Drone* drones, int droneCount, double* avgVelocity, double* avgCapacity);
void calculateCustomerScoresStage2(Customer** customers, int cCount, double avgVelocity, double avgCapacity);

void rebuildCustomerLedger(Customer** customers, int cCount, Drone* drones, int dCount);
int assignDronesStage3(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound);
int extendTripsMultiCustomer(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, int currentRound);

void sortCustomersParallel(Customer** customers, int count);

#endif