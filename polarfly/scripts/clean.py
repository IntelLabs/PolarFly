#!/usr/bin/python

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

import sys
import re
import os
import time

booksim_path    = '../booksim/src/' 
os.system("cd " + booksim_path + "; make clean")
os.system("rm -r " + booksim_path + "logs/*")
os.system("rm -r " + booksim_path + "sweep_examples/*")
