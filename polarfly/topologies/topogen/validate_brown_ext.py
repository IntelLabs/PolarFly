# Author: Kartik Lakhotia 
# This script's purpose is to validate extensions of Brown graph 

# PARAMETERS
# q: size of Galois Field (q is a prime power) 
# r0: number of replications of cluster C0 (max. 1 replication for even 'q' - free i.e. no increase in degree)
# r1: number of replications of non-quadric cluster

import networkx as nx
import random

def max_ip_set(nx_graph):
    node_set    = list(nx_graph.nodes)
    node_set.sort()
    ip_node = {}
    for i in node_set:
        ip_node[i]  = True

    ip_set  = []
    for i in range(len(node_set)):
        node = node_set[i]
        if (ip_node[node]):
            ip_set.append(node)
            for neigh in nx_graph.neighbors(node):
                ip_node[neigh] = False

    return ip_set


def validate(graph : [[int]], q : int, r0 : int, r1 : int):
    print("--> Validating Brown Extensions with q = " + str(q) + ", # C0 replications = " + str(r0) + ", # Ci replications = " + str(r1))


    #check size
    if (q%2 == 0):
        if (r0 > 0):
            r0 = 1
        exp_size    = q**2 + q + 1 + r0 + (q+1)*r1
    else:
        exp_size    = q**2 + q + 1 + (q+1)*r0 + q*r1
    if (len(graph) != exp_size):
        print(" --> construction error: incorrect number of nodes, expected = " + str(exp_size) + ", achieved = " + str(len(graph)))
        return 0

    # construct a nx graph (undirected, no multi-edges) for further evaluation
    nx_graph = nx.Graph()
    for i in range(len(graph)):
        nx_graph.add_node(i)
        for j in graph[i]:
            nx_graph.add_edge(i, j)

    # check if graph is connected
    if not nx.is_connected(nx_graph):
        print("     --> construnction error: not conncected")
        return 0

    # check max degree
    exp_max_deg = q+1
    if (q%2 == 0):
        exp_max_deg += r1
    else:
        exp_max_deg += 2*r0 + r1 + 1
    max_degree  = max(dict(nx_graph.degree).values())
    if max_degree > exp_max_deg:
         print("     --> construction error: incorrect degrees, max = " + str(max_degree) + ", expected = " + str(exp_max_deg))
         return 0

    # check min degree
    min_degree = min(dict(nx_graph.degree).values())
    if (min_degree + 2*r0 + 4 < max_degree):
        print("      --> construction error: imbalanced degrees, max = " + str(max_degree) + ", min = " + str(min_degree))
        return 0

    #check diameter
    exp_diam    = 2
    if (r1 > 0):
        exp_diam    += 1
    diameter    = nx.diameter(nx_graph)
    if diameter > exp_diam:
         print("     --> construction error: incorrect diameter")
         return 0


    #check average shortest path length
    apl = nx.average_shortest_path_length(nx_graph)
    if (apl >= 2):
         print("     --> construction error: incorrect average path length")
         return 0
        
    return 1


def generate_random_parameters(qmax: int, num_tests: int) -> [int]:


    q_candidates = [3, 5, 7, 9, 11, 13, 17, 19, 23, 25, 27, 29, 31, 37, 41, 43, 47, 49, 
                    53, 59, 61, 67, 71, 73, 79, 81, 83, 89, 97, 101, 103, 107, 109, 113, 121]

    params      = []
    
    for i in range(num_tests):
        qrnd    = random.randint(2, qmax)
        q       = qrnd
        if (q in q_candidates):
            q   = qrnd
        else:
            q = q_candidates[min(range(len(q_candidates)), key = lambda i: abs(q_candidates[i]-qrnd))]

        r0  = random.randint(1,4)
        r1  = random.randint(0,q)

        
        params.append([q, 0, 0])
        params.append([q, r0, 0])
        params.append([q, 0, r1])
        params.append([q, r0, r1])

    print(params)
    return params


def validate_brown_ext():
    from .BrownExtGenerator import BrownExtGenerator
    
    params  = generate_random_parameters(32, 20)
    results = []
    
    for param in params:
        g = BrownExtGenerator().make(param[0], param[1], param[2])
        results.append(validate(g, param[0], param[1], param[2]))
        
    
    if sum(results)==len(params):
        print("VALIDATION PASSED")
    else:
        print("VALIDATION NOT PASSED")
