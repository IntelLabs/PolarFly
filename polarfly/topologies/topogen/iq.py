from .paley import *

def baseGraph(q):
    nx_graph    = nx.Graph()
    phi         = {}
    A           = []
    fA          = []
    A           = [0,1,2,3]
    fA          = [4,5,6,7] 
    for i in range(8):
        nx_graph.add_node(i)
    nx_graph.add_edge(0,7)
    nx_graph.add_edge(4,7)
    nx_graph.add_edge(0,1)
    nx_graph.add_edge(4,1)
    nx_graph.add_edge(0,2)
    nx_graph.add_edge(4,2)
    nx_graph.add_edge(1,6)
    nx_graph.add_edge(5,6)
    nx_graph.add_edge(5,3)
    nx_graph.add_edge(5,7)
    nx_graph.add_edge(2,3)
    nx_graph.add_edge(6,3)
    if (q%4 == 0):
        A.append(8)
        fA.append(9)
        nx_graph.add_node(8)
        nx_graph.add_node(9)
        nx_graph.add_edge(8,0)
        nx_graph.add_edge(8,4)
        nx_graph.add_edge(8,1)
        nx_graph.add_edge(8,5)
        nx_graph.add_edge(9,2)
        nx_graph.add_edge(9,6)
        nx_graph.add_edge(9,3)
        nx_graph.add_edge(9,7)
    for i in range(len(A)):
        phi[A[i]]   = fA[i]
        phi[fA[i]]  = A[i]

    return nx_graph, A, fA, phi

def bdfGen(q):
    assert(((q % 4)==0) or ((q % 4 == 3)))
    nx_graph, A, fA, phi    = baseGraph(q)
    d   = 4 if ((q % 4) == 0) else 3
    n   = 2*d + 2
    incr= 4
    while(d < q):
        incr_graph, incrA, incrFA, incrPhi  = baseGraph(incr-1)
        incrV   = list(incr_graph.nodes)
        for i in incrV:
            nx_graph.add_node(n + i)
        newE    = incr_graph.edges()
        for e in newE:
            nx_graph.add_edge(n + e[0], n + e[1])
        assert(len(incrA)%2 == 0)
        connA   = len(incrA)//2
        for i in range(connA):
            u   = incrA[i] + n
            fu  = incrPhi[incrA[i]] + n
            phi[u] = fu
            phi[fu] = u   
            for v in A:
                nx_graph.add_edge(u, v)
                nx_graph.add_edge(fu, v)

            u   = incrA[i + connA] + n
            fu  = incrPhi[incrA[i + connA]] + n
            phi[u] = fu
            phi[fu] = u   
            for v in fA:
                nx_graph.add_edge(u, v)
                nx_graph.add_edge(fu, v)

        for i in incrA:
            A.append(i + n)
        for i in incrFA:
            fA.append(i + n)

        d += 4
        n += 8
        


    return nx_graph, phi
            
