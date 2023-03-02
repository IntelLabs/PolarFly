# PolarFly and PolarStar Artifact 
[![DOI](https://zenodo.org/badge/528958685.svg)](https://zenodo.org/badge/latestdoi/528958685)

PolarFly is a diameter-2 network topology based on the Erdos-Renyi family of polarity
graphs [[1]](#1)[[2]](#2). <br />
Polarstar is a diameter-3 network topology based on star product of Erdos-Renyi graphs with
Paley or Inductive Quad graphs [[3]](#3). <br />

This repository is a containerized artifact for generation and analysis of PolarFly and Polarstar networks.


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

## References


<a id="1">[1]</a> 
Lakhotia, Kartik, Maciej Besta, Laura Monroe, Kelly Isham, Patrick Iff, Torsten Hoefler, and Fabrizio Petrini. 
__ Polarfly: A cost-effective and flexible low-diameter topology. __ 
arXiv preprint arXiv:2208.01695, 2022.


<a id="2">[2]</a> 
Lakhotia, Kartik, Maciej Besta, Laura Monroe, Kelly Isham, Patrick Iff, Torsten Hoefler, and Fabrizio Petrini. 
__ Polarfly: A cost-effective and flexible low-diameter topology. __ 
Proceedings of the International Conference on High Performance Computing, Networking, Storage and Analysis, 2022.


<a id="3">[3]</a> 
Lakhotia, Kartik, Laura Monroe, Kelly Isham, Maciej Besta, Nils Blach, Torsten Hoefler, and Fabrizio Petrini.
__ PolarStar: Expanding the Scalability Horizon of Diameter-3 Networks. __ 
arXiv preprint arXiv:2302.07217, 2023.
