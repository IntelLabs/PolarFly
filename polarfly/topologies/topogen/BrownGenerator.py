# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Kartik Lakhotia 
# Brown Graph and finite field generator

# PARAMETERS
# q: size of Galois Field (q is a prime power) 

# PRECONDITIONS
# q is a prime power 



from .TopologyGenerator import TopologyGenerator
from .validate_brown import validate


import numpy as np



def compute_coeffs(i, primePower, primeFactor):
    coeffs = [0 for j in range(primePower)]
    tmp = int(i)
    cid = 0
    while(tmp > 0):
        coeffs[cid] = tmp%primeFactor
        cid += 1
        tmp = int(tmp/primeFactor)
    return coeffs

def compute_index(coeffs, primePower, primeFactor):
    assert(len(coeffs) == primePower)
    cid = primePower-1
    index = 0
    while(cid >= 0):
        index = index*primeFactor + coeffs[cid]
        cid -= 1 
    return index

def least_quadratic_non_residue(p):
    if (p%4 == 3):
        return p-1
    else:
        isQuadRes = [False for i in range(p)]
        for i in range(int(p/2) + 1):
            isQuadRes[(i**2)%p] = True
        out = p-1
        for i in range(p):
            if not isQuadRes[i]:
                out = i
                break
        return out

def print_mat(X, nrows, ncols):
    print("[")
    for i in range(ncols):
        print(X[i])
    print("]")
    
            


#generate addition/multiplication matrices for the field
def field_gen(q):

    isPrimeNumber = True
    isPrimePower = True
    primeFactor = int(q)
    primePower = int(1)
    tmp = int(q)
    
    for i in range(2, q):
        if ((q%i) == 0):
            primeFactor = i
            tmp = int(tmp/i)
            while((tmp%i) == 0):
                primePower += 1
                tmp = int(tmp/i)
            if (tmp == 1):
                if (q != i):
                    isPrimeNumber = False
            else:
                isPrimePower = False
            break

    if not isPrimePower:
        print("q must be a prime power")
        exit(0)

    assert(q < 125)


    #print(str(q) + " = " + str(primeFactor) + "^" + str(primePower)) 

    add_mat = [[0]*q for i in range(q)]
    mul_mat = [[0]*q for i in range(q)]

    poly_primePower = [0 for i in range(primePower)]
    if not isPrimeNumber:
        # x^5 = x^2 + 1
        if (q==32):
            poly_primePower[0] = 1
            poly_primePower[2] = 1
        # x^n = x + 1
        elif ((primeFactor == 2) or (q==27)):
            poly_primePower[0] = 1
            poly_primePower[1] = 1
        # x^4 = -x - 2
        elif (q==81):
            poly_primePower[0] = 1
            poly_primePower[1] = 2
        # x^2 = r
        elif ((primeFactor%2 == 1) and (primePower == 2)):
            r = least_quadratic_non_residue(q) 
            poly_primePower[0] = r
            print("least quad non residue is " + str(r))
        else:
            print("Sorry! I don't know how to construct multiplication matrix for this field")
            exit(0)
            

    
    
    if (isPrimeNumber):
        assert(primePower == 1)
        for i in range(q):
            for j in range(q):
                add_mat[i][j] = (i+j)%q
                mul_mat[i][j] = (i*j)%q
    else:
        for i in range(q):
            coeffs_i = compute_coeffs(i, primePower, primeFactor)
            for j in range(q):
                coeffs_j = compute_coeffs(j, primePower, primeFactor)
                add_coeffs = [(sum(x)%primeFactor) for x in zip(coeffs_i, coeffs_j)]
                add_mat[i][j] = compute_index(add_coeffs, primePower, primeFactor)

                #multiply polynomials
                mul_coeffs_ext = [0 for i in range(2*primePower - 1)]
                for ext_i in range(len(mul_coeffs_ext)):
                    for ext_j in range(min(primePower, len(mul_coeffs_ext))):
                        for ext_k in range(min(primePower, len(mul_coeffs_ext))):
                            if ((ext_j + ext_k) > ext_i):
                                break
                            if ((ext_j + ext_k) == ext_i):
                                mul_coeffs_ext[ext_i] += coeffs_i[ext_j]*coeffs_j[ext_k]
                                mul_coeffs_ext[ext_i] = mul_coeffs_ext[ext_i] % primeFactor

                #Reduce higher powers in multiplication output
                #scan in reverse so that all coefficients greater than pimepower are accounted
                for ext_i in range(len(mul_coeffs_ext)-1, primePower-1, -1):
                    offset = ext_i - primePower
                    for ext_j in range(primePower):
                        mul_coeffs_ext[ext_j + offset] += mul_coeffs_ext[ext_i]*poly_primePower[ext_j]
                        mul_coeffs_ext[ext_j + offset] = mul_coeffs_ext[ext_j + offset] % primeFactor
                    mul_coeffs_ext[ext_i] = 0

                for ext_i in range(primePower, len(mul_coeffs_ext)):
                    assert(mul_coeffs_ext[ext_i] == 0)

                mul_coeffs = [0 for i in range(primePower)]
                for ext_i in range(primePower):
                    mul_coeffs[ext_i] = mul_coeffs_ext[ext_i]
                mul_mat[i][j] = compute_index(mul_coeffs, primePower, primeFactor)

    for i in range(q):
        for j in range(q):
            assert(add_mat[i][j] == add_mat[j][i])
            assert(mul_mat[i][j] == mul_mat[j][i])


    return add_mat, mul_mat
    


class BrownGenerator(TopologyGenerator):
    
    def __init(self):
        super(BrownGenerator,self).__init__()

    def vector_mul(self, point, a):
        out = np.zeros(3,dtype=int)
        for i in range(3):
            out[i] = self.mul_mat[a][point[i]]
        return out

    def gen_v(self, v):
        vs = []
        for i in range(2,self.q):
            v_tmp =  self.vector_mul(v,i)
            vs.append(tuple(v_tmp))
        return vs


    def vec_dp(self, v1, v2):
        dp = int(0)
        for i in range(3):
            prod = self.mul_mat[v1[i]][v2[i]]
            dp = self.add_mat[dp][prod]
        return dp
        
    def make(self, q : int):
        self.q = q 
        self.add_mat, self.mul_mat = field_gen(q) 
        V = q**2 + q + 1

        vectors = []
        vectors_1 = []
        node_map = {}
        graph = [[] for _ in range(V)]

        # get all 1-d subspace vector of PG(2,q)
        for d1 in range(q):
            for d2 in range(q):
                v = (d1,d2,1)
                vectors.append(v)
                vectors_1.append(v)

        for d1 in range(q):
            v = (d1, 1, 0)
            vectors.append(v)
            vectors_1.append(v)

        v = (1, 0, 0)
        vectors.append(v)
        vectors_1.append(v)




        for idx,v in enumerate(vectors):
            node_map[v] = idx

        # add connection between 1-d subspace vectors
        for idx,v in enumerate(vectors):
            for vv in vectors:
                dp = self.vec_dp(v, vv)
                if ((dp % self.q) == 0):
                    source = idx
                    dest = node_map[vv]
                    if dest != source:
                        graph[source].append(dest)

        return graph



    def validate(self, topo : [[int]], q : int) -> bool:
        return validate(topo,q)
    
    def get_folder_path(self):
        return super(BrownGenerator,self).get_folder_path() + "Browns/"

    def get_file_name(self, q : int) -> str:
        return "Brown." + str(q) + ".adj.txt"

