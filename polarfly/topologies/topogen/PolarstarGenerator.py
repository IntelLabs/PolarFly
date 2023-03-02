# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Create Polarstar graph 

# PARAMETERS
# d: network radix 

# PRECONDITIONS
# d >= 2 


from .TopologyGenerator import TopologyGenerator
from .validate_polarstar import validate
from .BrownGenerator import BrownGenerator
from .iq import *
from .paley import *




class PolarstarGenerator(TopologyGenerator):
    
    def __init(self):
        super(PolarStarGenerator, self).__init__()

    def config(self, d, sg):
        scale   = 0
        jnrType = "iq"
        superq  = 0
        jnrq    = 0
        jnrVMax = 0
        pfVMax  = 0
        for pfd in range(2, d):
            jnrd    = d - pfd
            pfq     = pfd - 1
            pfV     = pfq*pfq + pfq + 1
            if (not is_power_of_prime(pfq)):
                continue 
    
            #can construct paley graph
            paleyq  = jnrd*2 + 1
            if (sg != "iq" and is_power_of_prime(paleyq) and (paleyq%4 == 1)):
                paleyV  = paleyq
                V       = paleyV*pfV
                if (V > scale):
                    scale   = V
                    jnrType = "paley" 
                    superq  = pfq
                    jnrq    = paleyq
                    jnrVMax = paleyV
                    pfVMax  = pfV
            elif (sg != "paley"):
                bdfq    = jnrd
                if (bdfq < 3):
                    continue
                if ((bdfq % 4 == 0) or (bdfq % 4 == 3)):
                    bdfV    = 2*bdfq + 2 
                    V       = bdfV*pfV
                    if (V > scale):
                        scale   = V
                        jnrType = "iq"
                        superq  = pfq
                        jnrq    = bdfq
                        jnrVMax = bdfV
                        pfVMax  = pfV
    
        print("max scale at degree " + str(d) + " is " + str(scale) + " vertices")
        print("Joiner graph = " + jnrType + ", joiner q = " + str(jnrq) + ", PF q = " + str(superq))
        print("SuperNodeSize = " + str(jnrVMax))
        print("Num SuperNodes: " + str(pfVMax))
    
        return superq, jnrq, jnrType


    def starProdGen(self, pfq, jnrq, jnrType):
        nx_jnr  = nx.Graph()
        assert(jnrType=="paley" or jnrType=="iq")
        if (jnrType == "paley"):
            nx_jnr, phi  = payleyGen(jnrq)
        else:
            nx_jnr, phi  = bdfGen(jnrq) 

        pf_graph= BrownGenerator().make(pfq)
        nx_pf   = nx.Graph()
        nx_pf.add_nodes_from([i for i in range(len(pf_graph))])
        for i in range(len(pf_graph)):
            for j in pf_graph[i]:
                nx_pf.add_edge(i, j)
    
        nx_graph= nx.Graph()
    
        superV  = nx_pf.number_of_nodes()
        intraV  = nx_jnr.number_of_nodes()
        V       = superV*intraV
    
        for i in range(superV):
            nx_graph.add_nodes_from([(i*intraV+j) for j in range(0, intraV)])
            for j in range(intraV):
                u       = i*intraV + j
                neigh   = [n for n in nx_jnr.neighbors(j)]
                for n in neigh:
                    v   = i*intraV + n
                    nx_graph.add_edge(u, v)
    
        #add polarfly self-loops
        numQuadrics = 0
        isQuadric   = [False for i in range(superV)]
        for i in range(superV):
            neigh       = [n for n in nx_pf.neighbors(i)]
            isQuadric[i]= (len(neigh)==pfq)
            if (isQuadric[i]):
               nx_pf.add_edge(i, i)
            numQuadrics += 1
                         
    
    
        for i in range(superV):
            neigh   = [n for n in nx_pf.neighbors(i)]
            for n in neigh:
                if (i < n):
                    for j in range(intraV):
                        u   = i*intraV + j
                        v   = n*intraV + phi[j]
                        if ((i==n) and (u>v)):
                            continue
                        nx_graph.add_edge(u, v)
                if (i == n):
                    conn    = [False for j in range(intraV)]
                    for j in range(intraV):
                        u   = i*intraV + j
                        v   = n*intraV + phi[j]
                        if (u==v):
                            continue
                        if (conn[j] or conn[phi[j]]):
                            continue
                        else:
                            nx_graph.add_edge(u, v)
                            conn[j]         = True
                            conn[phi[j]]    = True
    
        if (jnrType == "iq"):
            expectedV   = (2*jnrq + 2)*(pfq*pfq + pfq + 1)
            obtainedV   = len(list(nx_graph.nodes))
            assert(expectedV == obtainedV)
            
    
        return nx_graph, nx_pf, nx_jnr, phi 

    def make(self, d : int, sg : str):
        self.psd            = d
        self.sg             = sg
        pfq, jnrq, jnrType  = self.config(self.psd, self.sg)
        g, pfg, jnrg, phi   = self.starProdGen(pfq, jnrq, jnrType)
        V                   = g.number_of_nodes()
        graph               = [[] for _ in range(V)]
        for i in range(V):
            neighs          = [n for n in g.neighbors(i)]
            for neigh in neighs:
                graph[i].append(neigh)
        return graph

    def validate(self, topo : [[int]], d : int, sg : str) -> bool:
        return validate(topo, d)

    def get_folder_path(self):
        return super(PolarstarGenerator,self).get_folder_path() + "PolarStars/"

    def get_file_name(self, d : int, sg : str) -> str:
        return "Polarstar." + str(d) + "." + sg + ".adj.txt" 

