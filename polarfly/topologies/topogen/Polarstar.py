# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Create Polarstar graph 

# PARAMETERS
# d: network radix 
# sg: supernode type - bdf

# PRECONDITIONS
# d >= 2 

from .Topology import Topology
from .PolarstarGenerator import PolarstarGenerator
import numpy as np
import math 

class Polarstar(Topology):
    
    """
    Fields:
        Public:
            d:  network radix
            sg: subnode topology - "iq", "paley" or "max"  
            p: hosts per router
            nr: network radix of router 
            r: total radix of a router
            R: total number of routers
            N: total number of endnodes
            edge: number that indicates routers with endnodes (the first edge routers in topo have endnodes)
            name: name of topology (default := DEL)
    Methods: 
        get_topo(): return the topoology in adjacency list

    """
    
    def __init__(self, d : int=-1, sg : str='max', N : int=-1):
        """
        Parameters:
            d: network radix
            sg: supernode graph
            N: total number of endnodes
        """
        if (d==-1 and N!=-1):
            self.d  = int(pow(9*N, 0.25))
            self.sg = 'max'
        elif (d!=-1 and N==-1):
            self.d  = d
            self.sg = sg
        else:
            raise Exception("invalid combination of arguments in constructor")

        self.nr = self.d
        self.p  = int(self.nr)//3
        self.r = self.p + self.nr
        self.R  = self.config(self.d, self.sg) 
        self.N  = self.p*self.R
        self.edge = self.R
        self.name = 'PS'

        # private fields
        self.__topo = None

    def get_topo(self):
        if self.__topo is none:
            self.__topo = PolarstarGenerator().make(self.d, self.sg)
        return self.__topo
        
    def config(d, sg):
        scale   = 0
        jnrType = "iq"
        superq  = 0
        jnrq    = 0
        jnrVMax = 0
        pfVMax  = 0
        for pfd in range(2, d):
            jnrd    = d - pfd
            if (jnrd < 3):
                continue
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
    
        return scale 
