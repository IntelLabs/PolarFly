# PolarFly Artifact 
PolarFly is a diameter-2 network topology based on the Erdos-Renyi family of polarity
graphs. <br />
This repository is a containerized artifact for generation and analysis of PolarFly networks.


## Directory Structure
- Source code, dependencies and instructions to run simulations are in **polarfly** directory.
- **Dockerfile.polarfly** is the Dockerfile for the container.
- **run_script.sh** is a script to build and run the docker image.

## Container
It is advised to run the simulations directly on a server/CPU
to reduce execution time of simulations.

For ease of reproducibility, we provide a container image
which launches an Ubuntu kernel with all dependencies pre-installed.

To run the container image, execute
```
./run_script.sh
```
For appropriate permissions, you may have to run the script with root privileges.

## Requirements
- Docker (tested on server version 20.10.14)
