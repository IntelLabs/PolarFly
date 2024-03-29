# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Alessandro Maissen, Kartik Lakhotia

from .BrownGenerator import BrownGenerator
from .BrownExtGenerator import BrownExtGenerator
from .PolarstarGenerator import PolarstarGenerator

from .Brown import Brown
from .BrownExt import BrownExt
from .Polarstar import Polarstar

toponames = {
    'BRO': lambda q = -1, N = -1: Brown(q,N),
    'BRO_EXT' : lambda q = -1, N =-1, r0 = -1, r1 = -1: BrownExt(q,r0,r1,N),
    'PS' : lambda d = -1, sg = 'max', N = -1 : Polarstar(d, sg)
    }
