# Distributed MPC Swarm Controller, Gossip Trajectory Sharing, and Flight Modes

## Context
Autonomous swarm coordination requires a distributed control architecture that scales without a single point of failure (SPOF) on the ground station or a central drone. Quadrotor multicopters have physical actuator lag and rotational inertia, meaning commanded accelerations cannot be realized instantaneously. Furthermore, collision avoidance in dense swarms typically suffers from high computational complexity if each drone must estimate the future intentions of all other agents. A scalable, robust, and computationally feasible distributed controller is required to support multi-mode swarm flight (e.g. Hold, Takeoff, Formation, Cohesion, Land, Joystick Control).

## Decision
1. **Distributed Model Predictive Control (DMPC)**:
   - Each UAV independently solves a local MPC optimization problem generating velocity setpoints and acceleration feed-forward values streamed to PX4 Offboard mode (`SET_POSITION_TARGET_LOCAL_NED`).
   - **Actuator Lag in State Space Model**: The multicopter dynamic model in the discrete state transition matrix $A$ incorporates a first-order lag for acceleration response ($\dot{a} = -\frac{1}{\tau} a + \frac{1}{\tau} a_{cmd}$, resulting in $A_{accel} = e^{-\Delta t / \tau} < 1$) rather than assuming instantaneous acceleration transition ($A_{accel} = 1$).
2. **Leaderless P2P Mesh and Gossip-Based State/Trajectory Sharing**:
   - **Leaderless Topology**: The swarm operates as a strictly leaderless peer-to-peer (P2P) mesh with no central coordinator and no dynamic leader election (`REQ-CORE-023`–`REQ-CORE-029`). Every UAV acts as an equal peer; membership tracking, mode synchronization (`REQ-CORE-041`), sub-swarm `Split`/`Merge` updates (`REQ-CORE-043`, `REQ-CORE-045`), and trajectory sharing propagate peer-to-peer via gossip over ROS 2 (DDS). When a peer disconnects or enters fail-safe, neighbors prune it from their local neighbor table and continue operating without leader re-election (`REQ-CORE-026`, `REQ-CORE-053`).
   - **Trajectory Sharing for Collision Avoidance**: UAVs broadcast their predicted $N$-step trajectory horizon ($X_i^* = [x_i(k), \dots, x_i(k+N)]$) to neighbors via gossip/peer-to-peer communication (`REQ-CORE-094`).
   - Neighboring UAVs consume these received predicted trajectory steps directly as fixed external obstacle constraints in their local MPC optimization (`REQ-CORE-095`), eliminating the need to estimate neighbor intent and drastically reducing local solver overhead.
3. **Parametric Mode-Dependent Cost Function**:
   - The MPC objective function is parameterized dynamically according to the active swarm flight mode:
     $$J = J_{tracking} + w_{form}(mode) \cdot J_{formation} + w_{cohesion}(mode) \cdot J_{cohesion} + w_{coll} \cdot J_{collision} + J_{control}$$
   - In **Formation** mode, $w_{form} > 0$ penalizing deviations from graph-defined edge distances ($\mathcal{G} = (\mathcal{V}, \mathcal{E})$).
   - In **Proximity / Cohesion** mode, $w_{form} = 0$ while $w_{cohesion} > 0$, maintaining inter-agent distance bounds without enforcing rigid geometric slot assignments.
4. **Phased Operational Scope**:
   - **Phase 1 (Baseline)**: Single unified swarm where all members synchronize to a single active mode. Primary control interface is **Joystick Control** (teleoperating swarm velocity vectors), alongside autonomous `Hold`, `Takeoff`, `Land`, `Formation`, and `Cohesion`.
   - **Phase 2 (Advanced)**: Leaderless dynamic sub-swarm partitioning (`Split`/`Merge`), mission-level autonomous path planning, area scan, and Consensus-Based Bundle Algorithm (CBBA) for distributed multi-task allocation.
5. **Synchronous Fault-Tolerant Takeoff Execution**:
   - Takeoff commands execute synchronously without a master or leader node through a coordinated future timestamp barrier ($T_{sync} = T_{broadcast} + \Delta T_{margin}$).
   - During the margin window, each UAV independently aligns clocks (via ROS 2 / DDS) and validates local pre-arm/sensor conditions.
   - At $T_{sync}$, all ready vehicles initiate vertical climb simultaneously while strictly locking horizontal position ($w_{drift} \gg 0$) to eliminate lateral collision risk.
   - Vehicles failing pre-conditions remain grounded and broadcast fault telemetry; airborne UAVs dynamically prune grounded agents from the active formation graph (`REQ-CORE-053`).

## Why
Decoupling the swarm into a leaderless distributed MPC framework with gossip-based state and trajectory sharing guarantees collision-free flight without single points of failure or leader-election stalls, while keeping computation light enough for onboard or companion execution. Modeling first-order acceleration lag prevents aggressive overshooting inherent to multicopter inertia. Parametric cost weighting enables smooth runtime mode transitions without changing the underlying optimization structure.
