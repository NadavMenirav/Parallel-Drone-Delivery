#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "entities.h" 

double calculateDistance(Position p1, Position p2);

double calculateFlightTime(double distance, double velocity);

void updateCustomerPriorities(Customer* customers, int customerCount);

// Calculates the priority-aware optimization score for a customer-drone assignment
double calculateCustomerScore(int priority, double estimatedTime);

#endif