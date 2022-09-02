# Author: Kartik Lakhotia 
# This script's purpose is to validate a created Brown graph

import networkx as nx
import random 

def max_ip_set(nx_graph):
    node_set = list(nx_graph.nodes)
    node_set.sort()
    ip_node = {}
    for i in node_set:
        ip_node[i] = True

    ip_set = []
    for i in range(len(node_set)):
        node = node_set[i]
        if (ip_node[node]):
            ip_set.append(node)
            for neigh in nx_graph.neighbors(node):
                ip_node[neigh] = False 


    return ip_set
        
        

def validate(graph : [[int]], q : int):

    print("--> Validating Brown(%d):" %q)
    # check sizes
    if len(graph) != q**2 + q + 1:
        print("     --> construction error: incorrect number of nodes")
        return 0

    # construct a nx graph (undirected, no multi-edges) for further validation
    nx_graph = nx.Graph()
    for i in range(len(graph)):
        nx_graph.add_node(i)
        for j in graph[i]:
            nx_graph.add_edge(i, j)


    # check if graph is connected
    if not nx.is_connected(nx_graph):
        print("     --> construction error: not conncected")
        return 0

    # check max degree
    max_degree = max(dict(nx_graph.degree).values())
    if max_degree != q+1:
         print("     --> construction error: incorrect degrees")
         return 0

    #check diameter
    diameter = nx.diameter(nx_graph)
    if diameter != 2:
        print("     --> construction error: incorrect diameter")
        return 0

    return 1

def generate_random_parameters(qmax: int, num_tests: int) -> [int]:
    q_candidates = [2, 3, 4, 5, 7, 8, 9, 11, 13, 16, 17, 19, 23, 25, 27, 29, 31, 32, 37, 41, 43, 47, 49, 
                    53, 59, 61, 64, 67, 71, 73, 79, 81, 83, 89, 97, 101, 103, 107, 109, 113, 121, 125, 127, 
                    128, 131, 137, 139, 149, 151, 157, 163, 167, 169, 173, 179, 181, 191, 193, 197, 199, 211, 
                        223, 227, 229, 233, 239, 241, 243, 251]
    params      = []
    for i in range(num_tests):
        qrnd    = random.randint(2, qmax)
        q       = qrnd
        if (q in q_candidates):
            q   = qrnd
        else:
            q = q_candidates[min(range(len(q_candidates)), key = lambda i: abs(q_candidates[i]-qrnd))]
        
        params.append(q)
    return params

def validate_brown():
    from .BrownGenerator import BrownGenerator
    
    params  = generate_random_parameters(64, 10)
    results = []
    
    for param in params:
        g = BrownGenerator().make(param)
        results.append(validate(g, param))
    
    if sum(results)==len(params):
        print("VALIDATION PASSED")
    else:
        print("VALIDATION NOT PASSED")
