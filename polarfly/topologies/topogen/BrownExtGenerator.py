# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Create brown layout and extensions 
# Implemented based on the paper: Polarity graphs revisited 

# PARAMETERS
# q: size of Galois Field (q is a prime power) 
# r0: number of replications of cluster C0 (max. 1 replication for even 'q' - free i.e. no increase in degree)
# r1: number of replications of non-quadric cluster

# PRECONDITIONS
# q is a prime power 


from .TopologyGenerator import TopologyGenerator
from .validate_brown_ext import validate

import numpy as np


def is_quadric(adj, q):
    if (len(adj)>q):
        return False
    else:
        assert(len(adj)==q)
        return True


def clean_graph(graph):
    for v in range(len(graph)):
        graph[v].sort()
        tmp = [graph[v][0]]

        #remove all self-loop and multi-edges
        for u in graph[v]:
            if ((u != tmp[-1]) and (u != v)):
                tmp.append(u)

        graph[v]    = tmp

    return graph
    

def is_v1(graph, v, q):
    if (len(graph[v]) != q+1):
        return False
    for neigh in graph[v]:
        if (is_quadric(graph[neigh], q)):
            return True
    return False 

class BrownExtGenerator(TopologyGenerator):

    def __init(self):
        super(BrownExtGenerator,self).__init__()

    def layout_odd_q(self, brown_graph : [[int]]): 
        assert(self.q%2 != 0)
        num_v       = len(brown_graph)
    
        clusters    = [[] for _ in range(self.q+1)]  
        v_to_cl     = [-1 for _ in range(num_v)]
    
        #Create C0
        for v in range(num_v):
            if (is_quadric(brown_graph[v], self.q)):
                clusters[0].append(v)
                v_to_cl[v]  = 0
        assert(len(clusters[0])==self.q+1)
    
        #Create other clusters 
        v_sel   = clusters[0][0]
        cl_id   = 1
        cl_cntr = {} # center vertex of a cluster
        cl_adj  = {} # non-center neighbor in the same cluster

        for u in brown_graph[v_sel]:
            assert(v_to_cl[u]<0)
            #first vertex is the center
            clusters[cl_id].append(u)
            v_to_cl[u]      = cl_id
            cl_cntr[cl_id]  = u

            for neigh in brown_graph[u]:
                if (is_quadric(brown_graph[neigh], self.q)):
                    continue
                assert(v_to_cl[neigh]<0)
                clusters[cl_id].append(neigh)
                v_to_cl[neigh]  = cl_id
            
            if (len(clusters[cl_id]) != self.q):
                print("q = " + str(self.q) + ", cluster size = " + str(len(clusters[cl_id])))
            assert(len(clusters[cl_id])==self.q)

            cl_id   = cl_id + 1
            
        for v in range(len(brown_graph)):
            v_cl    = v_to_cl[v]
            assert(v_cl >= 0)
            if (v_cl == 0):
                continue
            if (v == cl_cntr[v_cl]):
                continue

            for neigh in brown_graph[v]:
                if (neigh == cl_cntr[v_cl]):
                    continue
                if (v_to_cl[neigh]==v_cl):
                    assert(v not in cl_adj) 
                    cl_adj[v]   = neigh
            
       
        return clusters, v_to_cl, cl_cntr


    def layout_even_q(self, brown_graph : [[int]]): 
        assert(self.q%2 == 0)
        num_v       = len(brown_graph)
    
        clusters    = [[] for _ in range(self.q+2)]  
        v_to_cl     = [-1 for _ in range(num_v)]

        #Find and assign quadrics
        cl_id   = 1
        cl_cntr = {} # center vertex of a cluster
        for v in range(num_v):
            if (is_quadric(brown_graph[v], self.q)):
                clusters[cl_id].append(v)
                v_to_cl[v]      = cl_id
                cl_cntr[cl_id]  = v
                cl_id           = cl_id + 1

        #Find center of centers
        for v in range(num_v):
            num_quadric_neigh   = 0
            for neigh in brown_graph[v]:
                if (is_quadric(brown_graph[neigh], self.q)):
                    num_quadric_neigh += 1
            if (num_quadric_neigh > 1):
                assert(num_quadric_neigh == self.q + 1)
                clusters[0].append(v)
                v_to_cl[v]  = 0
                
        assert(len(clusters[0])==1)
    
        #Create other clusters 
        for cl_id in range(1, len(clusters)):
            cntr    = cl_cntr[cl_id]
            assert(is_quadric(brown_graph[cntr], self.q))

            for v in brown_graph[cntr]:
                if(v_to_cl[v] < 0):
                    clusters[cl_id].append(v)
                    v_to_cl[v]  = cl_id

            assert(len(clusters[cl_id])==self.q)
            cl_id   = cl_id + 1
            
       
        return clusters, v_to_cl, cl_cntr




    def extend(self, brown_graph : [[int]], r0 : int, r1 : int) -> [[int]]:
        brown_graph = clean_graph(brown_graph) 

        clusters    = [[]]
        v_to_cl     = []
        cl_cntr     = {}
        replicas    = [set() for _ in range(len(brown_graph))] 
        original    = {}
        vertex_type = {}
        quadric_set = 0
        v1_set = 1
        v2_set = 2

        for i in range(len(brown_graph)):
            original[i] = i
            if (is_quadric(brown_graph[i], self.q)):
                vertex_type[i]  = quadric_set 
            elif (is_v1(brown_graph, i, self.q)):
                vertex_type[i]  = v1_set
            else:
                vertex_type[i]  = v2_set

        if (self.q%2 == 0):
            clusters, v_to_cl, cl_cntr = self.layout_even_q(brown_graph) 
        else:
            clusters, v_to_cl, cl_cntr = self.layout_odd_q(brown_graph) 

        #<NEW METHOD UPDATES> 
        clusters_not_connected = {}
        for i in range(len(brown_graph)):
            if ((vertex_type[i] == v1_set) and (i != cl_cntr[v_to_cl[i]])):
                clusters_not_connected[i]   = []
                clusters_connected    = [False for _ in range(len(clusters))]
                for neigh in brown_graph[i]:
                    clusters_connected[v_to_cl[neigh]] = True
                num_clusters_disconnected = 0 
                for j in range(len(clusters_connected)):
                    if (not clusters_connected[j]):
                        num_clusters_disconnected += 1
                        clusters_not_connected[i].append(j)
                if (num_clusters_disconnected != 2):
                    print(str(i) + " -> " + str(clusters_not_connected))
                assert(num_clusters_disconnected == 2)


        #can only deploy one extension method at a time
        #if (r1 > 0):
        #    r0 = 0

            
        num_v   = len(brown_graph)

        if (r1 > self.q):
            print("Currently support r1 <= q")
        assert(r1 <= self.q)

        quads           = []
        for v in range(len(brown_graph)):
            if (len(brown_graph[v]) == self.q):
                quads.append(v)
        if (len(quads) != self.q + 1):
            print("expected = " + str(self.q + 1) + ", num = " + str(len(quads)))
        assert(len(quads) == self.q + 1)
        quad_neigh_sets = []
        for v in quads:
            tmp = brown_graph[v][:]
            tmp.append(v)
            assert(len(tmp)==self.q + 1)
            quad_neigh_sets.append(tmp[:])


        ############################################################
        #Replicate C0 -> even q: center of centers, odd q: quadrics#
        ############################################################
        if ((self.q%2 == 0) and (r0 > 0)):
            r0  = 1
        for r in range(r0):
            num_ext = len(clusters[0])
            if (self.q%2 == 0):
                assert(num_ext == 1)
            else:
                assert(num_ext == self.q + 1)

            new_v   = [v+num_v for v in range(num_ext)]
            clusters.append(new_v)
            
            for v in new_v:
                v_to_cl.append(-1)
            for v in new_v:
                v_to_cl[v]  = len(clusters)-1
                brown_graph.append([])

            

            for i in range(num_ext):
                v       = clusters[0][i]
                v_rep   = clusters[-1][i]

                replicas[v].add(v_rep)
                original[v_rep] = v

                assert(v_rep >= num_v)
                assert(len(brown_graph[v_rep])==0)

                if (self.q%2 != 0): 
                    brown_graph[v].append(v_rep)
                    brown_graph[v_rep].append(v)
                
                for neigh in brown_graph[v]:
                    assert(neigh != v)
                    assert(v_to_cl[neigh] != 0)
                    if (neigh == v_rep): 
                        continue
                    brown_graph[v_rep].append(neigh)
                    brown_graph[neigh].append(v_rep)

                if (len(brown_graph[v_rep])!=len(brown_graph[v])):
                    print("mismatched neighborhood : v = " + str(len(brown_graph[v])) + ", vrep = " + str(len(brown_graph[v_rep])))
                    print(str(v) + " - " + str(brown_graph[v]))
                    print(str(v_rep) + " - " + str(brown_graph[v_rep]))
                assert(len(brown_graph[v_rep])==len(brown_graph[v]))
                

            num_v   = num_v + num_ext

        ############################################################
        ############################################################
        
        
        
        
        ############################################################
        #############Replicate other vertex subsets#################
        ############################################################
        #for r in range(r1):
        #    cl_id   = r #Round-robin
        #    num_ext = len(quad_neigh_sets[cl_id])
        #    assert(num_ext == self.q + 1)

        #    new_v   = [v+num_v for v in range(num_ext)]
        #    clusters.append(new_v)

        #    for v in new_v:
        #        v_to_cl.append(-1)
        #    for v in new_v:
        #        v_to_cl[v] = len(clusters)-1
        #        brown_graph.append([])

        #    v_to_rep    = {}
        #    for i in range(num_ext):
        #        v_to_rep[quad_neigh_sets[cl_id][i]] = clusters[-1][i]

        #    cntr                        = quads[cl_id]
        #    new_cntr                    = v_to_rep[cntr]
        #    cl_cntr[len(clusters)-1]    = new_cntr

        #    for i in range(num_ext):
        #        v       = quad_neigh_sets[cl_id][i]
        #        v_rep   = clusters[-1][i]
        #        replicas[v].add(v_rep)
        #        original[v_rep] = v

        #        assert(v_rep >= num_v)
        #        brown_graph[v].append(v_rep)
        #        brown_graph[v_rep].append(v)

        #        for replica in replicas[v]:
        #            if (replica == v_rep):
        #                continue
        #            cl_rep      = v_to_cl[replica]
        #            cntr_rep    = cl_cntr[cl_rep]
        #            brown_graph[v_rep].append(cntr_rep)
        #            brown_graph[cntr_rep].append(v_rep)
        #            brown_graph[replica].append(cntr)
        #            brown_graph[cntr].append(replica)
        #            
        #            #brown_graph[replica].append(v_rep)
        #            #brown_graph[v_rep].append(replica)


        #        for neigh in brown_graph[v]:
        #            assert(neigh != v)
        #            #already dealt with
        #            if ((neigh == v_rep) or (neigh in replicas[v])): 
        #                continue
        #            #neighbor in replicated set
        #            if (neigh in quad_neigh_sets[cl_id]):
        #                assert(neigh in v_to_rep)
        #                neigh_rep   = v_to_rep[neigh]
        #                brown_graph[v_rep].append(neigh_rep)
        #            else:
        #                brown_graph[v_rep].append(neigh)
        #                brown_graph[neigh].append(v_rep)

    
        #    num_v   = num_v + num_ext

            

        ############################################################
        ############################################################
        cluster_replicas = {}

        for r in range(r1):
            cl_id   = r + 1 #Round-robin
            cluster_replicas[cl_id]  = len(clusters) 
            num_ext = len(clusters[cl_id]) 
            assert(num_ext  == self.q)

            new_v   = [v+num_v for v in range(num_ext)]
            clusters.append(new_v)
            
              

            for v in new_v:
                v_to_cl.append(-1)
            for v in new_v:
                v_to_cl[v]  = len(clusters) - 1
                brown_graph.append([])

            v_to_rep    = {}
            for i in range(num_ext):
                v_to_rep[clusters[cl_id][i]]    = clusters[-1][i]

            new_cntr                    = v_to_rep[cl_cntr[cl_id]]
            cl_cntr[len(clusters)-1]    = new_cntr

            
            j   = 0
            for i in range(num_ext):
                v       = clusters[cl_id][i]    
                v_rep   = clusters[-1][i]
                assert(v_rep >= num_v)
                if (v_rep == new_cntr):
                    brown_graph[v].append(v_rep)
                    brown_graph[v_rep].append(v)
                assert(len(brown_graph[v]) <= self.q + 1 + 2*r0 + r + 2)


                for neigh in brown_graph[v]:
                    assert(neigh != v)
                    if ((neigh == v_rep)):
                        continue
                    if (v_to_cl[neigh] == cl_id):
                        neigh_rep   = v_to_rep[neigh]
                        brown_graph[v_rep].append(neigh_rep)
                    else:
                        brown_graph[v_rep].append(neigh)
                        brown_graph[neigh].append(v_rep)

                #find the cluster where v is not connected
                if ((vertex_type[v]==v1_set) and (v_rep != new_cntr)):
                    for cl_dir in clusters_not_connected[v]:
                        cntr_dir    = cl_cntr[cl_dir]
                        assert(vertex_type[cntr_dir] != quadric_set)
                        brown_graph[v_rep].append(cntr_dir)
                        brown_graph[cntr_dir].append(v_rep)
                        assert(len(brown_graph[cntr_dir]) <= self.q + 1 + 2*r0 + r + 2)
                        if (cl_dir in cluster_replicas):
                            cl_dir_rep  = cluster_replicas[cl_dir]
                            cntr_dir_rep= cl_cntr[cl_dir_rep] 
                            brown_graph[v_rep].append(cntr_dir_rep)
                            brown_graph[cntr_dir_rep].append(v_rep)
                            assert(cntr_dir_rep != cntr_dir)
                            assert(len(brown_graph[cntr_dir_rep]) <= self.q + 1 + 2*r0 + r + 2)
                        assert(len(brown_graph[v_rep]) <= self.q + 1 + 2*r0 + r + 2)
                        
                        
                     

                #assert(len(brown_graph[v_rep])==len(brown_graph[v]))
                assert(abs(len(brown_graph[v_rep])-len(brown_graph[v]) <= 4))
    
            num_v   = num_v + num_ext

        #print(clusters)
        #print(cl_cntr)


        assert(num_v == len(brown_graph))

        return clean_graph(brown_graph)


    def make(self, q : int, r0 : int, r1 : int) -> [[int]]:
        from .BrownGenerator import BrownGenerator

        self.q = q
        g   = BrownGenerator().make(q)

        return self.extend(g, r0, r1)


    def validate(self, topo : [[int]], q : int, r0 : int, r1 : int) -> bool:
        return validate(topo, q, r0, r1)

    def get_folder_path(self):
        return super(BrownExtGenerator, self).get_folder_path() + "BrownExt/"

    def get_file_name(self, q : int, r0 : int, r1 : int) -> str:
        return "BrownExt." + str(q) + "." + str(r0) + "." + str(r1)
