// $Id: anynet.hpp 5188M 2012-11-04 00:02:16Z (local) $

/*
 Copyright (c) 2007-2012, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _ANYNET_HPP_
#define _ANYNET_HPP_

#include "network.hpp"
#include "routefunc.hpp"
#include "misc_utils.hpp"
#include <cassert>
#include <string>
#include <map>
#include <list>

class AnyNet : public Network {

    //by mbesta
    int _p;
    //int _k;
    
  
  string routing_table_file;
  string load_routing_file;
  
  
    string file_name;
  //associtation between  nodes and routers
  map<int, int > node_list;
  //[link type][src router][dest router]=(port, latency)
  vector<map<int,  map<int, pair<int,int> > > > router_list;
  //stores minimal routing information from every router to every node
  //[router][dest_node]=port
  vector<map<int, int> > routing_table;
  
  map<int, map<int, int> > channelsMap;
  map<int, int> badTrafficTable;
  
  map<int,map<int,int> > commonNeighbors;
  map<int,map<int,bool> > neighbors;
  
  bool verbose;

  void _ComputeSize( const Configuration &config );
  void _BuildNet( const Configuration &config );
  void readFile();
  void buildRoutingTable();
  void printRoutingTable();
  void saveRoutingTable();
  void loadRoutingTable();
  
  void route(int r_start);

  void findCommonNeighbors();
public:
  AnyNet( const Configuration &config, const string & name );
  ~AnyNet();

  int GetN( ) const{ return -1;}
  int GetK( ) const{ return -1;}

  static void RegisterRoutingFunctions();
  double Capacity( ) const {return -1;}
  void InsertRandomFaults( const Configuration &config ){}
  
  void buildBadTrafficPattern1NotRand();
  void buildBadTrafficPattern1();
  void buildBadTrafficPattern2();
  void buildBadTrafficPattern3();
  void buildBadTrafficPatternHeaviestEdgeEqual();
  void buildBadTrafficPatternPF();
  void buildGoodTrafficPatternPF();
  map<int,int>* getBadTrafficPattern() {return &badTrafficTable;}
    void printChannelsMap();

};

vector<int>* getRouterNeighbors(int r1, int r2);
int getAnyNodeAttachedToRouter(int routerID);
int getNodeAttachedToRouter(int routerID, int addID);
int getRandomNodeAttachedToRouter(int routerID);
vector<int>* getNodesAttached(int routerID);
bool isAttached(int node, int router);
bool areNeighbors(int r1, int r2, bool verbose);
int commonNeighbor(int r1, int r2, bool verbose);
void min_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugallnew_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall3hops_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall2hops_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalg3hops_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalg2hops_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalgnew_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugallnew2_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalg_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void val_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalg_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall2_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugalg2_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall3_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

void ugall4_pf_anynet( const Router *r, const Flit *f, int in_channel, 
		      OutputSet *outputs, bool inject );

int anynet_min_r2r_hopcnt(int src, int dest);
int anynet_min_r2n_hopcnt(int src, int dest);

#endif
