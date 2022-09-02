#!/usr/bin/python

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

import sys
import re
import os
import string
import copy
import random
import math
import time
import argparse




#CREATE PARSER
parser  = argparse.ArgumentParser(
    description = 'run adaptive sweep over injection rate for a particular configuration',
    epilog      = 'booksim sweep'
)

subparser   = parser.add_subparsers(help="topology", dest='topo', required=True)
topology_parsers    = []

#Polarfly
parser_pf   = subparser.add_parser("polarfly", help="simulates polarfly")
parser_pf.add_argument("-q", dest='q', type=int, required=True, help="prime power")
parser_pf.add_argument("-r0", dest='r0', default=0, type=int, help="quadric cluster replication")
parser_pf.add_argument("-r1", dest='r1', default=0, type=int, help="non-quadric cluster replication")
parser_pf.add_argument('-rf', dest='rf', type=str, choices=['min', 'ugalgnew', 'ugallnew', 'ugall4_pf'], default="min", help='routing function')
parser_pf.add_argument('-t', dest='traffic', type=str, choices=['uniform', 'randperm', 'goodpf', 'badpf', 'tornado'], default="uniform", help='traffic pattern')
topology_parsers.append(parser_pf)

#Slimfly
parser_sf   = subparser.add_parser("slimfly", help="simulates slimfly")
parser_sf.add_argument("-q", dest='q', type=int, required=True, help="prime power")
parser_sf.add_argument('-rf', dest='rf', type=str, choices=['min', 'ugalgnew', 'ugallnew'], default="min", help='routing function')
parser_sf.add_argument('-t', dest='traffic', type=str, choices=['uniform', 'randperm', 'tornado'], default="uniform", help='traffic pattern')
topology_parsers.append(parser_sf)

#Dragonfly
parser_df   = subparser.add_parser("dragonfly", help="simulates dragonfly")
parser_df.add_argument("-n", dest='n', type=int, default=1, help="number of levels in supernode")
parser_df.add_argument('-rf', dest='rf', type=str, choices=['min', 'ugal'], default="min", help='routing function')
parser_df.add_argument('-t', dest='traffic', type=str, choices=['uniform', 'randperm', 'tornado'], default="uniform", help='traffic pattern')
topology_parsers.append(parser_df)

#Fattree
parser_ft   = subparser.add_parser("fattree", help="simulates fattree")
parser_ft.add_argument("-n", dest='n', type=int, default=3, help="depth")
parser_ft.add_argument('-rf', dest='rf', type=str, choices=['nca'], default="nca", help='routing function')
parser_ft.add_argument('-t', dest='traffic', type=str, choices=['uniform', 'randperm', 'tornado'], default="uniform", help='traffic pattern')
topology_parsers.append(parser_ft)

#Anynet
parser_any   = subparser.add_parser("anynet", help="simulate arbitrary topologies")
parser_any.add_argument("-i", dest='ipadj', type=str, required=True, help="input adj file")
parser_any.add_argument('-rf', dest='rf', type=str, choices=['min', 'ugalgnew', 'ugallnew', 'ugall4_pf'], default="min", help='routing function')
parser_any.add_argument('-t', dest='traffic', type=str, choices=['uniform', 'randperm', 'goodpf', 'badpf', 'tornado'], default="uniform", help='traffic pattern')
topology_parsers.append(parser_any)

for sub in topology_parsers:
    sub.add_argument("-m", dest='mode', choices=["sequential", "parallel"], default="sequential", help="sequential or parallel (multithreaded) simulations")
    sub.add_argument('-o', dest='opfile', type=str, required=True, help='output csv file')
    sub.add_argument('-rr_perm', dest='rr_permute', choices=['yes', 'no'], type=str, help='<yes> to enable router to router permutation')
    sub.add_argument('-k', required=True, dest='k', type=int, help='num endpoints per router')
    sub.add_argument('-vc', dest='vc', type=int, help='num vcs')
    sub.add_argument('-buf', dest='buf', type=int, help='buffer size')    
    sub.add_argument('-per', dest='period', type=int, help='sample period')
    sub.add_argument('-nums', dest='num_samples', type=int, help='number of samples')


args    = parser.parse_args()

if (args.mode=="parallel"):
    import multiprocessing
    from joblib import Parallel,delayed

#CREATE DIRECTORIES AND DEFINE PATHS
booksim_path    = '../booksim/src/' 
booksim_exec    = booksim_path + "booksim"
config_file     = booksim_path + "examples/" + args.topo + "config"
logs_path       = booksim_path + "/logs/"
config_file_path= booksim_path + "/sweep_examples/"
topogen_path    = '../topologies/tool.py generate '
pf_data_path    = './data/Browns/'
pf_ext_data_path= './data/BrownExt/'
sf_data_path    = './data/slimflies/'
bs_ext          = ".bs.anynet"

print("USING CONFIG FILE : " + str(config_file))

if (not os.path.isdir(logs_path)):
    os.mkdir(logs_path)
if (not os.path.isdir(config_file_path)):
    os.mkdir(config_file_path)

os.system("cd " + booksim_path + "; make")
if (not os.path.exists(booksim_exec)):
    print("Compilation Failed")
    exit(1)

#unique identifier in case multiple scripts are run simultaneously
timestamp       = time.time() 
logs_path       = logs_path + str(timestamp) + "/"
config_file_path= config_file_path + str(timestamp) + "/"

if (not os.path.isdir(logs_path)):
    os.mkdir(logs_path)
if (not os.path.isdir(config_file_path)):
    os.mkdir(config_file_path)

new_config_file = config_file_path + "anynet.config"
os.system("cp " + config_file + " " + new_config_file)
print(new_config_file)

unstable_str    = 'Simulation unstable'
inj_rate_str    = 'input_inj_rate'
deadlock_str    = 'WARNING: Possible network deadlock'



def check_params(args):
    if (args.topo == "polarfly"):
        assert(args.r0 <= args.q)
        assert(args.r1 <= args.q)
        assert(((args.traffic!="goodpf") and (args.traffic!="badpf")) or ((args.r0==0) and (args.r1==0)))

    if (args.topo == "dragonfly"):
        assert((args.rf!="ugal") or (args.vc==3))

    if (args.rr_permute == "yes"):
        assert((args.traffic!="goodpf") and (args.traffic!="badpf"))
        assert(args.topo!="fattree")



def translate_to_booksim(input_file, k):
    f   = open(input_file, 'r') 
    mms = f.readlines()
    f.close()
    
    bs_file = input_file + bs_ext 
    f_bs = open(bs_file, 'w')
    
    line_nr = 0
    
    p = k 
    node_nr = 0
    
    for line in mms:
      if line_nr == 0:
        line_nr = 1
        continue
    
      output_bs = "router " + str(line_nr-1) + " "
    
      line_nr = line_nr + 1
    
      for elem in line.split(' '):
        if elem != '\n':
          output_bs = output_bs + "router " + str(int(elem)) + " " 
    
      for n_id in range(0,p):
        output_bs = output_bs + "node " + str(node_nr) + " "
        node_nr = node_nr + 1
    
      output_bs = output_bs + "\n"
      f_bs.write(output_bs)
    
    f_bs.close()
    return bs_file



def generate_topology(args):
    cmd     = 'python3 ' + topogen_path 
    op_file = ''
    if (args.topo=="polarfly"):
        r0  = args.r0 #quadric replication
        r1  = args.r1 #non-quadric replication
        if (r0==0) and (r1==0):
            cmd     = cmd + " brown " + str(args.q)
            op_file = pf_data_path + "Brown." + str(args.q) + ".adj.txt"
        else:
            cmd = cmd + " brown_ext " + str(args.q) + " " + str(r0) + " " + str(r1) 
            op_file = pf_ext_data_path + "BrownExt." + str(args.q) + "." + str(r0) + "." + str(r1)
        os.system(cmd) 
        op_file = translate_to_booksim(op_file, args.k)

    elif (args.topo=="slimfly"):
        cmd     = cmd + " slimfly " + str(args.q) 
        os.system(cmd) 
        op_file = sf_data_path + "SlimFly." + str(args.q) + ".adj.txt"
        op_file = translate_to_booksim(op_file, args.k)

    elif (args.topo=="anynet"):
        op_file = translate_to_booksim(args.ipadj, args.k)

    else:
        print("no need to generate dragonfly and fat tree adjacency lists")
            
    return op_file
            



def create_cmd(inj_rate_, network_file, config_file, logfile, args):
    inj_rate = inj_rate_ if inj_rate_ > 0 else 0.01
    cmd = booksim_exec + " " +  config_file + " injection_rate=" + str(inj_rate)
    if (network_file!=''):
        cmd = cmd + " network_file=" + network_file
    else:
        assert((args.topo=="dragonfly") or (args.topo=="fattree"))
        cmd = cmd + " topology=" + args.topo
        if (args.topo=="dragonfly"):
            cmd = cmd + "new"
    if args.rf is not None:
        cmd = cmd + " routing_function=" + args.rf
    if args.k is not None:
        cmd = cmd + " k=" + str(args.k)
    if args.vc is not None:
        cmd = cmd + " num_vcs=" + str(args.vc)
    if args.buf is not None:
        cmd = cmd + " vc_buf_size=" + str(args.buf)
    if args.period is not None:
        cmd = cmd + " sample_period=" + str(args.period)
    if args.num_samples is not None:
        cmd = cmd + " max_samples=" + str(args.num_samples)
    if args.traffic is not None:
        cmd = cmd + " traffic=" + args.traffic 
    if args.rr_permute is not None:
        cmd = cmd + " rr_permute=" + args.rr_permute
    cmd = cmd + " > " + logfile 
    return cmd

def seq_sim(inj_rate, network_file, args):
    logfile = logs_path + "log_" + str(inj_rate)
    cmd = create_cmd(inj_rate, network_file, new_config_file, logfile, args) 
    print(cmd)
    os.system(cmd) 


def run_sim_s(min_rate, max_rate, incr, network_file, args):
    num_steps   = int((max_rate - min_rate + 0.000001)/incr) + 1
    inj_rates   = []
    for i in range(num_steps):
        inj_rate = min_rate + incr*i
        if (inj_rate <= max_rate):
            inj_rates.append(min_rate + incr*i)
    
    for i in range(num_steps):
        inj_rate    = min_rate + incr*i
        if (inj_rate <= max_rate):
            seq_sim(inj_rate, network_file, args)
        failed = False
        logfile = logs_path + "log_" + str(inj_rate)
        if (inj_rate <= max_rate): 
            with open(logfile) as f:
                for line in f.readlines():
                    index = line.find(unstable_str)
                    if (index != -1):
                        print("failed on inj rate = " + str(inj_rate))
                        failed = True
                        break
        f.close()
        if (failed):
            break

    return inj_rate


def run_sim_p(min_rate, max_rate, incr, network_file, args):
    num_steps   = int((max_rate - min_rate + 0.000001)/incr) + 1
    inj_rates   = []
    for i in range(num_steps):
        inj_rate = min_rate + incr*i
        if (inj_rate <= max_rate):
            inj_rates.append(min_rate + incr*i)
    #parallelize this
    num_cores   = multiprocessing.cpu_count()
    poolObj     = multiprocessing.Pool()
    processed   = Parallel(n_jobs=min(num_cores, num_steps))(delayed(seq_sim)(inj_rate, network_file, args) for inj_rate in inj_rates)
    
    for i in range(num_steps):
        inj_rate    = min_rate + incr*i
        failed = False
        logfile = logs_path + "log_" + str(inj_rate)
        if (inj_rate <= max_rate): 
            with open(logfile) as f:
                for line in f.readlines():
                    index = line.find(unstable_str)
                    if (index != -1):
                        print("failed on inj rate = " + str(inj_rate))
                        failed = True
                        break
        f.close()
        if (failed):
            break

    return inj_rate


def read_sim(min_rate, max_rate, incr, key_strs, parameter_values):
    num_steps   = int((max_rate - min_rate + 0.000001)/incr) + 1
    i = 0
    for i in range(num_steps):
        inj_rate    = min_rate + incr*i
        if (inj_rate > max_rate):
            continue
        logfile     = logs_path + "log_" + str(inj_rate)
        parameter_values[inj_rate_str].append(inj_rate)

        failed = False
        deadlock = False

        with open(logfile) as f:
            for line in f.readlines():
                index = line.find(deadlock_str)
                if (index != -1):
                    for key in key_strs:
                        parameter_values[key].append('-')
                    deadlock = True
                    break
        f.close()
        if (deadlock):
            return

        if (not deadlock):
            with open(logfile) as f:
                for line in f.readlines():
                    index = line.find(unstable_str)
                    if (index != -1):
                        for key in key_strs:
                            parameter_values[key].append('-')
                        failed = True
                        break
            f.close()

            if(not failed):
                with open(logfile) as f:
                    for line in f.readlines():
                        for key in key_strs:
                            index   = line.find(key_strs[key])
                            if (index != -1):
                                value   = line.split("=")[1].split("(")[0].strip()
                                parameter_values[key].append(value)
                                assert(len(parameter_values[key]) == len(parameter_values[inj_rate_str]))
                                break
                f.close()

        exp_len = len(parameter_values[inj_rate_str])
        for key in key_strs:
            if (len(parameter_values[key]) < exp_len):
                parameter_values[key].append('-nf')

        if(failed):
            break
    

#CHECK PARAMETERS
print("--> CHECKING PARAMETERS")
check_params(args)


#STRINGS TO MINE FROM BOOKSIM OUTPUT
result  = []
key_strs= {}

key_strs['min_latency_str'] = 'Overall minimum packet latency'
key_strs['avg_latency_str'] = 'Overall average packet latency'
key_strs['max_latency_str'] = 'Overall maximum packet latency'
key_strs['min_inj_str']     = 'Overall minimum injected packet rate'
key_strs['avg_inj_str']     = 'Overall average injected packet rate'
key_strs['max_inj_str']     = 'Overall maximum injected packet rate'
key_strs['num_0_hops']      = 'Number of 0-hop paths'
key_strs['num_1_hops']      = 'Number of 1-hop paths'
key_strs['num_2_hops']      = 'Number of 2-hop paths'
key_strs['num_3_hops']      = 'Number of 3-hop paths'
key_strs['num_4_hops']      = 'Number of 4-hop paths'
key_strs['num_5_hops']      = 'Number of 5-hop paths'

parameter_values = {}
parameter_values[inj_rate_str] = []
for key in key_strs:
    parameter_values[key] = []

#CREATE TOPOLOGY FOR BOOKSIM INPUT
print("--> GENERATING TOPOLOGY USING TOPOGEN")
network_file    = generate_topology(args)


#COARSE GRANULARITY SIMULATIONS
print("--> RUNNING COARSE GRANULARITY SIMULATIONS")
coarse_incr = 0.1
min_inj_rate= 0.0 
max_inj_rate= 1.0
run_sim = run_sim_s
if (args.mode=="parallel"):
    run_sim = run_sim_p 

fine_max_inj_rate   = run_sim(min_inj_rate, max_inj_rate, coarse_incr, network_file, args) 
read_sim(min_inj_rate, max_inj_rate, coarse_incr, key_strs, parameter_values)

#FINE GRANULARITY SIMULATIONS
print("--> RUNNING FINE GRANULARITY SIMULATIONS NEAR SATURATION")
fine_incr           = 0.01
fine_min_inj_rate   = fine_max_inj_rate - 2*coarse_incr + fine_incr
if (fine_min_inj_rate <= 0):
    fine_min_inj_rate = fine_incr
fine_max_inj_rate   = fine_max_inj_rate - fine_incr
max_inj_rate        = run_sim(fine_min_inj_rate, fine_max_inj_rate, fine_incr, network_file, args)
read_sim(fine_min_inj_rate, fine_max_inj_rate, fine_incr, key_strs, parameter_values)

#CLEAR LOG FILES
#os.system("rm -rf " + logs_path)
#os.system("rm -rf " + config_file_path) 

#DUMP OUTPUT
print("--> WRITING OUTPUT")
opfile  = args.opfile 
f       = open(opfile, "w")

#print(parameter_values)
f.write("log id = " + str(timestamp) + "\n")
num_vals    = len(parameter_values[inj_rate_str])
for key in parameter_values:
    f.write(key + ", ")
f.write("\n")
for i in range(num_vals):
    inj_rate    = parameter_values[inj_rate_str][i]
    if (inj_rate > max_inj_rate):
        continue
    
    for key in parameter_values:
        f.write(str(parameter_values[key][i]))
        f.write(", ")
    f.write("\n")

