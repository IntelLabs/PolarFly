# Script for network analysis 

Analyze network performance as a function of injection rate

- **simulate.py** - injection rate sweep 
- **clean.py** - cleaning simulation logs 

## Output 
A csv format file with different parameters
(latency, number of k-hop paths etc.) as a function
of injection rate. 


## Usage
```
python3 simulate.py -h
```

```
python3 simulate.py <topology> -h
```

For parallel (multithreaded) sweep, use `-m parallel` option.


## Dependencies
- Python3.x
- joblib - for parallel sweep 
- NumPy
- SciPy
- SymPy
- R (sqldf, ggplot2)


## Supported topologies
- PolarFly (with and without incremental extensions) 
- Dragonfly (balanced)
- Fat-Tree (three-stage variant) 



## Supported Traffic Patterns

- __uniform__ - uniform random
- __randperm__ - random permutation traffic. To emulate co-packaged setting, use `-rr_perm yes`. 
- __goodpf__ - perm2hop permutation traffic (for Polarfly only, without extensions)
- __badpf__ - perm1hop permutation traffic (for Polarfly only, without extensions)




## Supported Routing Functions

- __min__ - minpath routing
- __ugal__ - Universal Adaptive Routing based on local buffers
- **ugall4\_pf** - Adaptive Routing that restricts choice of intermediate routers to reduce total length (for Polarfly and its extensions only)
- **nca** - nearest common ancestor (for Fattree only) 


## Examples
```
python3 simulate.py polarfly -q 5 -rf min -t uniform -m sequential -o pf.csv -k 3 -vc 4 -buf 32

python3 simulate.py fattree -n 3 -rf nca -t uniform -m sequential -o dump.csv -k 2 -vc 4 -buf 32 

python3 simulate.py dragonfly -n 1 -rf ugal -t uniform -m sequential -o dump.csv -k 2 -vc 3 -buf 43 

python3 simulate.py slimfly -q 3 -rf min -t uniform -m sequential -o dump.csv -k 4 -vc 4 -buf 32
```




## Config Files
Following config files are present in `network_design/booksim/src/examples/`:

- __polarflyconfig__ - for polarfly
- __slimflyconfig__ - for slimfly
- __dragonflyconfig__ - for dragonfly
- __fattreeconfig__ - for fattrees 



## Cleaning simulation logs
```
python3 clean.py
```
