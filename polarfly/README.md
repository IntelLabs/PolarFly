# PolarFly 
|![PolarFly Topology with 553 routers of radix 24 each (q=23).](polarfly.png)|
|:--:|
|**PolarFly Topology with 553 routers of radix 24 each (q=23). Black edges highlight a rack of PolarFly**.|


Members: Kartik Lakhotia, Macieh Besta, Laura Monroe, Kelly Isham, Patrick Iff, Torsten Hoefler, Fabrizio Petrini

## Introduction
PolarFly is a diameter-2 network topology based on the 
Erdos-Renyi family of polarity
graphs. <br />
PolarFly is the first known diameter-2
topology that asymptotically reaches the <br />
(optimal) Moore bound on the
number of nodes for a given network degree and diameter.<br /><br />


PolarFly is flexible -- offers large number of feasible degrees
including some of particular interest <br /> 
from a technological perspective, such as, 24, 32, 48, 62, 128. <br />
It has a modular and extensible design which allows incremental
network growth without rewiring.

|![Moore bound comparison of different diameter-2 topologies](moore_bound_comparison.JPG)|
|:--:|
|**Moore Bound Comparison of PolarFly and other diameter-2 topologies**|


## Directory Structure
- **booksim** - Network simulator by Jiang et al., modified for PolarFly and Slim Fly
- **topologies** - Generate network topologies (in adjacency list format)
- **scripts** - Script to generate simulation results (compile, generate topology and run injection rate sweeps) reported in the paper.

Each directory has READMEs with corresponding instructions.


## Requirements
- C++11 - for BookSim)
- lex, yacc (bison and flex packages)
- Python3.x - for topology generation and simulation sweeps
- NumPy, SciPy, SymPy, R (sqldf, ggplot2) - for topology generation
- joblib - to parallelize simulation sweeps


## References 

1. Nan Jiang, Daniel U. Becker, George Michelogiannakis, James Balfour, Brian Towles, John Kim and William J. Dally. "A Detailed and Flexible Cycle-Accurate Network-on-Chip Simulator." Proceedings of the 2013 IEEE International Symposium on Performance Analysis of Systems and Software, 2013 [Code](https://github.com/booksim/booksim2).

2. P. Erdos and A. Renyi. "On a problem in the theory of graphs." The Mathematical Gazette, 1963.

3. W. G. Brown. "On graphs that do not contain a Thomsen graph." Canadian Mathematical Bulletin, 1966.

4. M. Besta and T. Hoefler. "Slim Fly: A cost effective low-diameter network topology." Proceedings of the International Conference for igh performance computing, networking, storage and analysis, 2014.



