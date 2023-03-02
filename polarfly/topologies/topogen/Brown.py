# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Create brown graph 

# PARAMETERS
# q: size of Galois Field (q is a prime power) 

# PRECONDITIONS
# q is a prime power 


from .Topology import Topology
from .BrownGenerator import BrownGenerator
import numpy as np
import math
class Brown(Topology):
    
    """
    Fields:
        Public: 
            q: size of Gallois Field
            p: hosts per router
            nr: network radix of router 
            r: total radix of a router
            R: total number of routers
            N: total number of endnodes
            edge: number that indicates routers with endnodes (the first edge routers in topo have endnodes)
            alpha: where q = 2^(2*a-1)
            sigma: where sigma = 2^a
            name: name of topology (default := DEL)
        Private:
            __topo: holds None or the topology in adjacency list

    Methods: 
        get_topo(): return the topoology in adjacency list
    """

    def __init__(self, q = -1, N = -1):
        
        """
        Parameters:
            q: size of Gallois Field

        """
        q_candidates = [2, 3, 4, 5, 7, 8, 9, 11, 13, 16, 17, 19, 23, 25, 27, 29, 31, 32, 37, 41, 43, 47, 49, 
                        53, 59, 61, 64, 67, 71, 73, 79, 81, 83, 89, 97, 101, 103, 107, 109, 113, 121, 125, 127, 
                        128, 131, 137, 139, 149, 151, 157, 163, 167, 169, 173, 179, 181, 191, 193, 197, 199, 211, 
                        223, 227, 229, 233, 239, 241, 243, 251]



        if(q == -1 and N != -1):
            if N >= 65000:
                self.q = 251
            else:
                desired_q = sqrt(N) 
                self.q = q_candidates[min(range(len(q_candidates)), key = lambda i: abs(q_candidates[i]-desired_q))]
        elif(q != -1 and N == -1):
            if (q in q_candidates):
                self.q = q
            else:
                print("specified q is not a prime power. Adjusting")
                self.q = q_candidates[min(range(len(q_candidates)), key = lambda i: abs(q_candidates[i]-q))]
        else:
            raise Exception("invalid combination of arguments in constructor")

        self.nr = self.q+1
        self.p = int(self.nr/2)
        self.r = self.p + self.nr
        self.R = (self.q**2+ self.q + 1)
        self.N = self.p*self.R
        self.edge = self.R
        self.name = 'BRO'

        # private fields
        self.__topo = None

    def get_topo(self):
        if self.__topo is none:
            self.__topo = BrownGenerator().make(self.q)
        return self.__topo
    
