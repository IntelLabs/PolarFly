# Tool generating different topologies

This tool can generate different topologies, compute path diverity measures on supported topologies and plot them.


## Supported topologies
- n-dimensional hypercube
- k-ary n-dimensional torus
- k-ary n-flat (Flattened Butterfly)
- h-MLFM (Multi-Layer Full-Mesh)
- k-OFT (Two-Level k-Orthogonal Fat-Tree)
- Jellyfish (r-regular)
- regular HyperX
- Dragonfly (balanced)
- Xpander
- Fat-Tree (three-stage variant) 
- 2x oversubscribed Fat-Tree
- Slim Fly
- Polar Fly (*brown* graphs)
- Incremental extensions of Polarfly (*brown_ext*) 

## Supported measures
- shortest path length
- shortest path multiplicity
- count of edge disjoint paths
- path interference

## Supported plots
- histogram of shortest path length
- histogram of shortest path multiplicity
- histogram of count of edge disjoint paths
- 2-dimentional map of low connectivity pairs
- histogram of path interference
- detailed view of path interference

## Usage
```
python3 tool.py -h
```

## Dependencies
- NumPy
- SciPy
- SymPy
- R (sqldf, ggplot2)
- NetworkX (only used to validate topologies)
