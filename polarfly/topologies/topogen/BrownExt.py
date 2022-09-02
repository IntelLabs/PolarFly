# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Create brown graph extensions 

# PARAMETERS
# q: size of Galois Field (q is a prime power) 
# r0: number of replications of cluster C0
# r1: number of replications of non-quadric cluster

# PRECONDITIONS
# q is a prime power 


from .Topology import Topology
from .BrownExtGenerator import BrownExtGenerator
import numpy as np
import math

class BrownExt(Topology):

    def __init__(self, q = -1, r0 = 0, r1 = 0, N = -1):
    
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

        self.r0     = r0
        self.r1     = r1

        self.nr     = self.q + 1 + 2*self.r0 + self.r1
        self.p      = int(self.nr/2)
        self.r      = self.p + self.nr
        self.R      = (self.q**2 + self.q + 1) + (self.q + 1)*self.r0 + self.q*self.r1
        self.N      = self.p*self.R
        self.edge   = self.R
        self.name   = "BRO_EXT"

        #private fields
        self.__topo = None


    def get_topo(self):
        if self.__topo is None:
            self.__topo = BrownExtGenerator().make(self.q)
        return self.__topo
    
