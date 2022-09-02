# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia

import re, string, sys, math, queue, copy, random

print("cmd to run : python3 <script> <input_file> <output_file> <#endpoints_per_router>")

f = open(sys.argv[1], 'r')
print(f)
mms = f.readlines()
f.close()

f_bs = open(sys.argv[2], 'w')
print(f_bs)

line_nr = 0

p = int(sys.argv[3])
print(p)
node_nr = 0

for line in mms:
  if line_nr == 0:
    line_nr = 1
    continue
  print (line)

  output_bs = "router " + str(line_nr-1) + " "

  line_nr = line_nr + 1

  for elem in line.split(' '):
    print (">>>>:" + elem)
    if elem != '\n':
      output_bs = output_bs + "router " + str(int(elem)) + " " 
      #output_bs = output_bs + "router " + str(int(elem)) + " " + sys.argv[4] + " " #adding latency 

  for n_id in range(0,p):
    output_bs = output_bs + "node " + str(node_nr) + " "
    node_nr = node_nr + 1

  output_bs = output_bs + "\n"
  f_bs.write(output_bs)

f_bs.close()
