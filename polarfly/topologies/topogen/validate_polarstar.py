# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Validate Polarstar 

import networkx as nx
import random

def validate(graph: [[int]], d : int, sg : str):
    print("--> Validating Polarstar(%d):", d)

    print("number of nodes = " + str(len(graph)))

    #construct a nx graph (undirected, no multi-edges) for validation
    nx_graph    = nx.Graph()
    for i in range(len(graph)):
        nx_graph.add_node(i)
        for j in graph[i]:
            nx_graph.add_edge(i, j)

        
    # check if graph is connected
    if not nx.is_connected(nx_graph):
        print("     --> construction error: not connected")
        return 0

    #check max degree
    max_degree  = max(dict(nx_graph.degree).values())
    print("max degree = " + str(max_degree))
    if max_degree != d:
        print("     --> construction error: incorrect degrees")
        return 0

    #check diameter
    diameter    = nx.diameter(nx_graph)
    print("diameter = " + str(diameter))
    if (diameter > 3):
        print("     --> construction error: incorrect diameter")
        return 0


    return 1
   

def validate_polarstar():
    from .PolarstarGenerator import PolarstarGenerator

    num_tests   = 20
    dmax        = 32
    gType       = ['iq', 'paley', 'max']
    params      = []
    results     = []    


    for i in range(num_tests):
        d       = random.randint(8, dmax)
        for sg in gType:
            g       = PolarstarGenerator().make(d, sg) 
            params.append(str(d) + " " + sg)
            results.append(validate(g, d, sg))

    if sum(results)==num_tests*len(gType):
        print("VALIDATION PASSED")
    else:
        print("VALIDATION NOT PASSED")

