# Parallel-Drone-Delivery
Parallel Drone Delivery – Running Instructions

This project consists of three components:

C simulation engine (core logic)
Node.js backend (runs the simulation and serves data)
React frontend (visualization)

The frontend is a visualization of the C simulation, not a separate logic layer.

Requirements
GCC with OpenMP support
Node.js
npm
Step 1 – Compile the Simulation

From the project root:

cd ~/Parallel-Drone-Delivery
gcc src/main.c src/algorithms.c src/entities.c -lm -fopenmp -o simulation
Step 2 – Run Backend Server

Open a second terminal:

cd ~/Parallel-Drone-Delivery/server
node server.js

The server will run on:

http://localhost:3000
Step 3 – Run Frontend

Open a third terminal:

cd ~/Parallel-Drone-Delivery/frontend
npm install
npm run dev

Open the application:

http://localhost:5173
Usage
Select a simulation scenario (Mock 1–4)
Click Play to run the simulation
Use Pause or Next Round for manual control

Each selection triggers the backend to run:

./simulation --export mockX

which generates state.json. The frontend then visualizes this output round by round.

Notes
The frontend does not implement simulation logic; it only displays the state produced by the C program.
If changes are made to the C code, you must recompile before running again.

This setup ensures the behavior shown in the browser directly reflects the actual C-based simulation.
