//ad/ $Id: anynet.cpp 5188M 2012-11-04 00:04:02Z (local) $

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

/*anynet
 *
 *The 
 *
 */

#include "anynet.hpp"
#include <fstream>
#include <sstream>
#include <limits>
#include <algorithm>
#include <string>


//this is a hack, I can't easily get the routing talbe out of the network
map<int, int>* global_routing_table;
map<int, int >* global_node_list;
vector<map<int,  map<int, pair<int,int> > > >* global_router_map;
vector<Router *>* global_router_list;
map<int, int>* global_bad_traffic_table;
map<int,map<int,int>>* global_common_neighbors;
map<int,map<int,bool>>* global_neighbors; //map that tels whether a router is a neighbor or not

//this is also a hack...
int global_adaptive_threshold;
int global_buffer_size;

int gP_anynet;

bool verboseRouting = false;

AnyNet::AnyNet( const Configuration &config, const string & name )
  :  Network( config, name ){
    
  router_list.resize(2);
  
  verbose = true;
  _ComputeSize( config );
  _Alloc( );
  _BuildNet( config );
  computeSizePowerOfTwo();
  
  
  global_routing_table = &routing_table[0];
  global_node_list = &node_list;
  global_router_map = &router_list;
  global_router_list = &_routers;
  global_bad_traffic_table = &badTrafficTable;
  
  findCommonNeighbors();
  
  global_common_neighbors = &commonNeighbors;
  global_neighbors = &neighbors;
  global_adaptive_threshold = config.GetInt("adaptive_threshold");
  global_buffer_size = config.GetInt("vc_buf_size");
  
  //printRoutingTable();
  cout << endl;
  
  if(!load_routing_file.compare("no")) {
      //cout << "Saving routing table..." << endl;
      //saveRoutingTable();
  }
  cout << "Saving routing table..." << endl;
  saveRoutingTable();
  
  _p = config.GetInt( "k" );
  gP_anynet = _p;
  

  assert(_size * _p == _nodes);

  //buildBadTrafficPattern2();
  //buildBadTrafficPatternHeaviestEdgeEqual();
  string pattern_name = config.GetStr("traffic");
  if(pattern_name == "badpf")  
    buildBadTrafficPatternPF();
  else if (pattern_name == "goodpf")
    buildGoodTrafficPatternPF();
    
}

AnyNet::~AnyNet(){
  for(int i = 0; i < 2; ++i) {
    for(map<int, map<int, pair<int,int> > >::iterator iter = router_list[i].begin();
	iter != router_list[i].end();
	++iter) {
      iter->second.clear();
    }
  }
}

void AnyNet::findCommonNeighbors() {
    for(int i = 0; i < _routers.size(); i++) {
        int r1 = _routers[i]->GetID();
        for(int j = 0; j < _routers.size(); j++) {
            int r2 = _routers[j]->GetID();
            
            assert(i == r1);
            assert(j == r2);
            
            commonNeighbors[r1][r2] = -1;
            neighbors[r1][r2] = false;
        }
    }
    
    for(int i = 0; i < _routers.size(); i++) {
        for(int j = 0; j < _routers.size(); j++) {
            
            int r1 = _routers[i]->GetID();
            int r2 = _routers[j]->GetID();
            
            if(r1 == r2) {
                continue;
            }
            
            assert(i == r1);
            assert(j == r2);
            
            if(areNeighbors(r1,r2,false)) {
                neighbors[r1][r2] = true;
                neighbors[r2][r1] = true;
            }
            
            map<int, pair<int,int> >::const_iterator iter1;
            map<int, pair<int,int> >::const_iterator iter2;

            for(iter1 = (*global_router_map)[1][r1].begin(); iter1 != (*global_router_map)[1][r1].end(); iter1++) {
                int finish = false;
                for(iter2 = (*global_router_map)[1][r2].begin(); iter2 != (*global_router_map)[1][r2].end(); iter2++) {
                    if(iter1->first == iter2->first) {

                        assert(areNeighbors(iter1->first,r1,false));
                        assert(areNeighbors(iter1->first,r2,false));
                        commonNeighbors[r1][r2] = iter1->first;
                        commonNeighbors[r2][r1] = iter1->first;
                        assert(iter1->first >= 0);
                        finish = true;
                        break;
                    }
                }
                if(finish) {
                    break;
                }
            }
           
        }
    }
}


void AnyNet::_ComputeSize( const Configuration &config ){
  file_name = config.GetStr("network_file");
  if(file_name==""){
    cout<<"No network file name provided"<<endl;
    exit(-1);
  }
  
  routing_table_file = config.GetStr("routing_table_file");
    if(routing_table_file==""){
    cout<<"No routing table file name provided"<<endl;
    exit(-1);
  }
  
  load_routing_file = config.GetStr("load_routing_file");
    if(routing_table_file==""){
    cout<<"Decide whether to load the routing file or not"<<endl;
    exit(-1);
  }
  
  //parse the network description file
  readFile();

  _channels =0;
  cout<<"========================Network File Parsed=================\n";
  verbose = true;
  if(verbose) {
  
    cout<<"******************node listing**********************\n";
      map<int,  int >::iterator iter;
      for(iter = node_list.begin(); iter!=node_list.end(); iter++){
        cout<<"Node "<<iter->first;
        cout<<"\tRouter "<<iter->second<<endl;
      }

      map<int,   map<int, pair<int,int> > >::iterator iter3;
      cout<<"\n****************router to node listing*************\n";
      for(iter3 = router_list[0].begin(); iter3!=router_list[0].end(); iter3++){
        cout<<"Router "<<iter3->first<<endl;
        map<int, pair<int,int> >::iterator iter2;
        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          cout<<"\t Node "<<iter2->first<<" lat "<<iter2->second.second<<endl;
        }
      }

      cout<<"\n*****************router to router listing************\n";
      for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        cout<<"Router "<<iter3->first<<endl;
        map<int, pair<int,int> >::iterator iter2;
        if(iter3->second.size() == 0){
          cout<<"Caution Router "<<iter3->first
              <<" is not connected to any other Router\n"<<endl;
        }
        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          cout<<"\t Router "<<iter2->first<<" lat "<<iter2->second.second<<endl;
          _channels++;
          
          channelsMap[iter3->first][iter2->first] = 0;
          
        }
      }
  }
  
  else {
      cout<<"******************node listing**********************\n";
      map<int,  int >::iterator iter;
      for(iter = node_list.begin(); iter!=node_list.end(); iter++){
      }

      map<int,   map<int, pair<int,int> > >::iterator iter3;
      cout<<"\n****************router to node listing*************\n";
      for(iter3 = router_list[0].begin(); iter3!=router_list[0].end(); iter3++){
        map<int, pair<int,int> >::iterator iter2;
        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
        }
      }

      cout<<"\n*****************router to router listing************\n";
      for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
      
        map<int, pair<int,int> >::iterator iter2;
        if(iter3->second.size() == 0){
          cout<<"Caution Router "<<iter3->first
              <<" is not connected to any other Router\n"<<endl;
        }
        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          _channels++;
          
          channelsMap[iter3->first][iter2->first] = 0;
        }
      }
  }
  
  verbose = true;
  _size = router_list[1].size();
  _nodes = node_list.size();


/*
  const int ilp_slim_noc = 1;
  if(ilp_slim_noc) {

    IloEnv env;
  try {
    IloModel model(env);
    IloNumVarArray vars(env);
    IloExpr obj_fun(env);

     IloNumArray startVal(env);
     int** positions = NULL;

    int variant_start = -1;
    string f_pos_name = "";

          ifstream inFile_s;
          inFile_s.open("start_variant.config");
          inFile_s >> variant_start >> f_pos_name;
          inFile_s.close(); 

    int N_r = _size;

    if(variant_start == 1) {
      ifstream f(f_pos_name);
      string line;

      positions = new int*[N_r];
      for(int i = 0; i < N_r; ++i) {
        positions[i] = new int[2];
      }

      int line_nr = 0;
      int positions_cnt = 0;
      while(getline(f, line)) {
        istringstream is( line );
        int n;
        int cnt = 0;
        while( is >> n ) {
          cout << n << "-";
          if(cnt > 0) {
            positions[line_nr][cnt-1] = n;
            ++positions_cnt;
          }
          ++cnt;
        }
        cout << endl;

        ++line_nr;
        assert(cnt == 3);
      }
      f.close();

      // print to test


      cout << " ============= TESTING initial positions" << endl;
      for(int i = 0; i < N_r; ++i) {
        for(int j = 0; j < 2; ++j) {
            cout << positions[i][j] << " ";
        }
        cout << endl;
      }
      cout << " ============= DONE" << endl;

      assert(line_nr == N_r);
      assert(positions_cnt == 2*N_r);
    }

    int N_e = 0;

    int base_vars_cnt = 0;

     cout<<"\n>>> ILP *****************router to router listing************\n";
      map<int,   map<int, pair<int,int> > >::iterator iter3;

      int r_cnt = 0;
      for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        cout<<"Router "<<iter3->first<<endl;

        if(iter3->second.size() == 0) assert(false);

        if(N_r == 18) {
          vars.add(IloNumVar(env, 0, 5, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][0]);
          }

          vars.add(IloNumVar(env, 0, 5, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][0]);
          }
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 50) {
          vars.add(IloNumVar(env, 0, 4, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][0]);
          }
          vars.add(IloNumVar(env, 0, 9, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][1]);
          }

          //vars.add(IloNumVar(env, 0, 8, ILOINT));
          //vars.add(IloNumVar(env, 0, 18, ILOINT));

          //vars.add(IloNumVar(env, 0, 9, ILOINT));
          //vars.add(IloNumVar(env, 0, 4, ILOINT));

          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 128) {
          vars.add(IloNumVar(env, 0, 7, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][0]);
          }

          vars.add(IloNumVar(env, 0, 15, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][1]);
          }
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 162) {
          vars.add(IloNumVar(env, 0, 17, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][0]);
          }

          vars.add(IloNumVar(env, 0, 17, ILOINT));
          if(variant_start == 1) {
            startVal.add(positions[r_cnt][1]);
          }

          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else {
          vars.add(IloNumVar(env, 0, N_r, ILOINT));
          vars.add(IloNumVar(env, 0, N_r, ILOINT));
        }

        base_vars_cnt += 2;

        ++N_e;
        ++r_cnt;
      }


          int variant_ilp = -1;

          ifstream inFile;
          inFile.open("ilp_variant.config");
          inFile >> variant_ilp;
          inFile.close();


if(variant_ilp == 1) {

      int extended_vars_cnt = 0;
     int tester = 0;

     for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        map<int, pair<int,int> >::iterator iter2;
      
        int r1 = iter3->first;
        
        for(iter2 = iter3->second.begin();
            iter2!=iter3->second.end();
            iter2++){

          int r2 = iter2->first;    

          //obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          vars.add(IloNumVar(env, ILOINT));
          vars.add(IloNumVar(env, ILOINT));
          ++tester;
          extended_vars_cnt += 2;

          if(variant_start == 1) {
            startVal.add(2*N_r);
            startVal.add(2*N_r);
          }
        }
      } 

      assert(base_vars_cnt == 2*N_e);
      assert(extended_vars_cnt == 2*tester);


    for(int i = 0; i < N_r; ++i) {
      for(int j = i+1; j < N_r; ++j) {
          int r1 = i;
          int r2 = j;
          model.add(vars[2*r1] - vars[2*r2] != 0 || vars[2*r1 + 1] - vars[2*r2 + 1] != 0);
          assert(2*r1 + 1 < base_vars_cnt);
          assert(2*r2 + 1 < base_vars_cnt);
      }
    }

    int cnt = 0;

    for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        map<int, pair<int,int> >::iterator iter2;

        int r1 = iter3->first;

        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          cout<<"\t Router "<<iter2->first<<endl;

          int r2 = iter2->first;          

          //obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          obj_fun += vars[base_vars_cnt + cnt] + vars[base_vars_cnt + cnt + 1];  

          //vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          model.add(vars[2*r1] - vars[2*r2] <= vars[base_vars_cnt + cnt]);
          model.add(vars[2*r1] - vars[2*r2] >= -vars[base_vars_cnt + cnt]);

          model.add(vars[2*r1 + 1] - vars[2*r2 + 1] <= vars[base_vars_cnt + cnt + 1]);
          model.add(vars[2*r1 + 1] - vars[2*r2 + 1] >= -vars[base_vars_cnt + cnt + 1]);

          cnt += 2;

          
          if(selection == 1) {
            obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            //model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 2) {
            obj_fun += -vars[2*r1] + vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 3) {
            obj_fun += vars[2*r1] - vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else if(selection == 4) {
            obj_fun += -vars[2*r1] + vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else {
            assert(false);
          }
          
        }
      }

}
else if(variant_ilp == 2) {

    for(int i = 0; i < N_r; ++i) {
      for(int j = i+1; j < N_r; ++j) {
          int r1 = i;
          int r2 = j;
          model.add(vars[2*r1] - vars[2*r2] != 0 || vars[2*r1 + 1] - vars[2*r2 + 1] != 0);
          assert(2*r1 + 1 < base_vars_cnt);
          assert(2*r2 + 1 < base_vars_cnt);
      }
    }


      for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        map<int, pair<int,int> >::iterator iter2;

        int r1 = iter3->first;

        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          cout<<"\t Router "<<iter2->first<<endl;

          int r2 = iter2->first;          

          obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          //vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          
          if(selection == 1) {
            obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            //model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 2) {
            obj_fun += -vars[2*r1] + vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 3) {
            obj_fun += vars[2*r1] - vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else if(selection == 4) {
            obj_fun += -vars[2*r1] + vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else {
            assert(false);
          }
          
        }
      }

      model.add(obj_fun >= 1);

       if(N_r == 18) {
          vars.add(IloNumVar(env, 0, 2, ILOINT));
          vars.add(IloNumVar(env, 0, 5, ILOINT));
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 50) {
          vars.add(IloNumVar(env, 0, 4, ILOINT));
          vars.add(IloNumVar(env, 0, 9, ILOINT));

          //vars.add(IloNumVar(env, 0, 8, ILOINT));
          //vars.add(IloNumVar(env, 0, 18, ILOINT));

          //vars.add(IloNumVar(env, 0, 9, ILOINT));
          //vars.add(IloNumVar(env, 0, 4, ILOINT));
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 128) {
          vars.add(IloNumVar(env, 0, 7, ILOINT));
          vars.add(IloNumVar(env, 0, 15, ILOINT));
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else if(N_r == 162) {
          vars.add(IloNumVar(env, 0, 8, ILOINT));
          vars.add(IloNumVar(env, 0, 17, ILOINT));
          //cout << " ----------------------------------------- DLACZEGO NIE ODPOWIADASZ " << endl;
        }
        else {
          vars.add(IloNumVar(env, 0, N_r, ILOINT));
          vars.add(IloNumVar(env, 0, N_r, ILOINT));
        }



}
else if(variant_ilp == 3) {

          int variant_3 = -1;

          ifstream inFile_3;
          inFile_3.open("ilp_variant_3.config");
          inFile_3 >> variant_3;
          inFile_3.close();



    for(int i = 0; i < N_r; ++i) {
      for(int j = i+1; j < N_r; ++j) {
          int r1 = i;
          int r2 = j;
          model.add(vars[2*r1] - vars[2*r2] != 0 || vars[2*r1 + 1] - vars[2*r2 + 1] != 0);
          assert(2*r1 + 1 < base_vars_cnt);
          assert(2*r2 + 1 < base_vars_cnt);
      }
    }


      for(iter3 = router_list[1].begin(); iter3!=router_list[1].end(); iter3++){
        map<int, pair<int,int> >::iterator iter2;

        int r1 = iter3->first;

        for(iter2 = iter3->second.begin(); 
            iter2!=iter3->second.end(); 
            iter2++){
          cout<<"\t Router "<<iter2->first<<endl;

          int r2 = iter2->first;          

          obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          //vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];

          if(selection == 1) {
            obj_fun += vars[2*r1] - vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            //model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 2) {
            obj_fun += -vars[2*r1] + vars[2*r2] + vars[2*r1 + 1] - vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] >= 0);
          }
          else if(selection == 3) {
            obj_fun += vars[2*r1] - vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] >= 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else if(selection == 4) {
            obj_fun += -vars[2*r1] + vars[2*r2] - vars[2*r1 + 1] + vars[2*r2 + 1];
            model.add(vars[2*r1] - vars[2*r2] < 0 && vars[2*r1 + 1] - vars[2*r2 + 1] < 0);
          }
          else {
            assert(false);
          }
        }
      }

}
else {
  assert(false);
}
    
  
  cerr << "Adding the function to the model... ";
      model.add(IloMinimize(env, obj_fun));
      //model.add(IloMaximize(env, obj_fun));
      cerr << "... done" << endl;

      cerr << "Constructing the cplex ";
      IloCplex cplex(model);
      cerr << "... done" << endl;

      if(variant_start == 1) {
        cplex.addMIPStart(vars, startVal);
      }

      cerr << "Solving ";
      int ret_val = cplex.solve();
      cerr << "... done" << endl;

      if ( !ret_val ) {
        env.error() << "Failed to optimize LP." << endl;
        throw(-1);
      }

      cerr << "Constructing return vals ";
      IloNumArray vals(env);
      cerr << "... done" << endl;

      env.out() << "Solution status = " << cplex.getStatus() << endl;
      env.out() << "Solution value = " << cplex.getObjValue() << endl;
      cplex.getValues(vals, vars);
      env.out() << "Values = " << vals << endl;

      cerr << "####################### TESTING VALUES " << endl;
      for(int i = 0; i < N_r; ++i) {
        cerr << i << " " << vals[2*i] << " " << vals[2*i + 1] << endl;
        //cerr << "R " << i << " (x): " << vals[2*i] << " (y): " << endl;
//        cerr << "R " << i << " (y): " << vals[2*i + 1] << endl;
      }
      cerr << "####################### DONE" << endl;









  }
  catch (IloException& e) {
    cerr << "Concert exception caught: " << e << endl;
  }
  catch (...) {
    cerr << "Unknown exception caught" << endl;
  }
  env.end();
  }
*/

 //exit(-1);
}

//calculate the hop count between src and estination (router - router)
int anynet_min_r2r_hopcnt(int src, int dest) {
  int hopcnt = -1;
  
  map<int,   map<int, pair<int,int> > >::const_iterator riter = (*global_router_map)[1].find(src);
  map<int, pair<int,int> >::const_iterator rriter;


  bool directNeighbor = false;
  for(rriter = riter->second.begin();rriter!=riter->second.end(); rriter++){
      if(rriter->first == dest) {
          directNeighbor = true;
      }
  }
  
  hopcnt = (directNeighbor == true) ? 1 : 2;
  
  return hopcnt;  
}

//calculate the hop count between src and estination (router - node)
int anynet_min_r2n_hopcnt(int src, int dest) {
  int hopcnt = -1;
  
  int rID = (*global_node_list)[dest];
  hopcnt = anynet_min_r2r_hopcnt(src, rID);
  
  return hopcnt;  
}

void AnyNet::_BuildNet( const Configuration &config ){
  
//I need to keep track the output ports for each router during build
  int * outport = (int*)malloc(sizeof(int)*_size);
  for(int i = 0; i<_size; i++){outport[i] = 0;}

  cout<<"==========================Node to Router =====================\n";
  //adding the injection/ejection chanenls first
  map<int,   map<int, pair<int,int> > >::iterator niter;
  for(niter = router_list[0].begin(); niter!=router_list[0].end(); niter++){
    map<int,   map<int, pair<int,int> > >::iterator riter = router_list[1].find(niter->first);
    //calculate radix
    int radix = niter->second.size()+riter->second.size();
    int node = niter->first;
    cout<<"router "<<node<<" radix "<<radix<<endl;
    //decalre the routers 
    ostringstream router_name;
    router_name << "router";
    router_name << "_" <<  node ;
    _routers[node] = Router::NewRouter( config, this, router_name.str( ), 
    					node, radix, radix );
    _timed_modules.push_back(_routers[node]);
    //add injeciton ejection channels
    map<int, pair<int,int> >::iterator nniter;
    for(nniter = niter->second.begin();nniter!=niter->second.end(); nniter++){
      int link = nniter->first;
      //add the outport port assined to the map
      (niter->second)[link].first = outport[node];
      outport[node]++;
      cout<<"\t connected to node "<<link<<" at outport "<<nniter->second.first
	  <<" lat "<<nniter->second.second<<endl;
      _inject[link]->SetLatency(nniter->second.second);
      _inject_cred[link]->SetLatency(nniter->second.second);
      _eject[link]->SetLatency(nniter->second.second);
      _eject_cred[link]->SetLatency(nniter->second.second);

      _routers[node]->AddInputChannel( _inject[link], _inject_cred[link] );
      _routers[node]->AddOutputChannel( _eject[link], _eject_cred[link] );
    }

  }

  cout<<"==========================Router to Router =====================\n";
  //add inter router channels
  //since there is no way to systematically number the channels we just start from 0
  //the map, is a mapping of output->input
  int channel_count = 0; 
  for(niter = router_list[0].begin(); niter!=router_list[0].end(); niter++){
    map<int,   map<int, pair<int,int> > >::iterator riter = router_list[1].find(niter->first);
    int node = niter->first;
    map<int, pair<int,int> >::iterator rriter;
    cout<<"router "<<node<<endl;
    for(rriter = riter->second.begin();rriter!=riter->second.end(); rriter++){
      int other_node = rriter->first;
      int link = channel_count;
      //add the outport port assined to the map
      (riter->second)[other_node].first = outport[node];
      outport[node]++;
      cout<<"\t connected to router "<<other_node<<" using link "<<link <<" at outport "<<rriter->second.first<<" lat "<<rriter->second.second<<endl;

      _chan[link]->SetLatency(rriter->second.second);
      _chan_cred[link]->SetLatency(rriter->second.second);

      _routers[node]->AddOutputChannel( _chan[link], _chan_cred[link] );
      _routers[other_node]->AddInputChannel( _chan[link], _chan_cred[link]);
      channel_count++;
    }
  }


  if(!load_routing_file.compare("yes")) {
      loadRoutingTable();
  }
  else {
      buildRoutingTable();
  }
  verbose = false;
  
}


void AnyNet::RegisterRoutingFunctions() {
  gRoutingFunctionMap["min_anynet"] = &min_anynet;
  gRoutingFunctionMap["val_anynet"] = &val_anynet;
  gRoutingFunctionMap["ugallnew_anynet"] = &ugallnew_anynet;
  gRoutingFunctionMap["ugall3hops_anynet"] = &ugall3hops_anynet;
  gRoutingFunctionMap["ugall2hops_anynet"] = &ugall2hops_anynet;
  gRoutingFunctionMap["ugalgnew_anynet"] = &ugalgnew_anynet;
  gRoutingFunctionMap["ugalg3hops_anynet"] = &ugalg3hops_anynet;
  gRoutingFunctionMap["ugalg2hops_anynet"] = &ugalg2hops_anynet;
  gRoutingFunctionMap["ugalg_anynet"] = &ugalg_anynet;
  gRoutingFunctionMap["ugall_pf_anynet"] = &ugall_pf_anynet;
  gRoutingFunctionMap["ugalg_pf_anynet"] = &ugalg_pf_anynet;
  gRoutingFunctionMap["ugall2_pf_anynet"] = &ugall2_pf_anynet;
  gRoutingFunctionMap["ugalg2_pf_anynet"] = &ugalg2_pf_anynet;
  gRoutingFunctionMap["ugall3_pf_anynet"] = &ugall3_pf_anynet;
  gRoutingFunctionMap["ugall4_pf_anynet"] = &ugall4_pf_anynet;
}

void min_anynet( const Router *r, const Flit *f, int in_channel, 
		 OutputSet *outputs, bool inject ){
    
  int out_port=-1;
  if(!inject){
    assert(global_routing_table[r->GetID()].count(f->dest)!=0);
    out_port=global_routing_table[r->GetID()][f->dest];
  }

  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }

  outputs->Clear( );

  outputs->AddRange( out_port , vcBegin, vcEnd );
}

bool isAttached(int node, int router) {
        if(global_node_list->find(node) != global_node_list->end() && global_node_list->find(node)->second == router) {
            return true;
        }
        else {
            return false;
        }
}


int commonNeighbor(int r1, int r2, bool verbose) {
    
   
    
    map<int, pair<int,int> >::const_iterator iter1;
    map<int, pair<int,int> >::const_iterator iter2;
    
    for(iter1 = (*global_router_map)[1][r1].begin(); iter1 != (*global_router_map)[1][r1].end(); iter1++) {
        for(iter2 = (*global_router_map)[1][r2].begin(); iter2 != (*global_router_map)[1][r2].end(); iter2++) {
            if(iter1->first == iter2->first) {
                
                if(verbose) {cout << "[commonNeighbor] router " << r1 << " <---> " << iter1->first << " <---> " << r2 << endl;}
                return iter1->first;
            }
        }
    }
    return -1;
}

int getAnyNodeAttachedToRouter(int routerID) {
    
    int nodeID = (*global_router_map)[0][routerID].begin()->first;
    assert(isAttached(nodeID, routerID) == true);
    return nodeID;
}

int getRandomNodeAttachedToRouter(int routerID) {
    int nodeID = (*global_router_map)[0][routerID].begin()->first;
    int add = rand() % gP_anynet;
    
    //cout << "rid " << routerID << endl;
    //cout << "nid " << nodeID;
    //cout << "add " << add;
    
    nodeID += add;
    assert(isAttached(nodeID, routerID) == true);
    return nodeID;
}

int getRandomRouterAttachedToRouter(int routerID) {
    int idx = rand() % (*global_router_map)[1][routerID].size();
	int i = 0;
	for(auto rid : (*global_router_map)[1][routerID]){
		if(i == idx){
			return rid.first;
		}
		i++;
	}	
}

int getNodeAttachedToRouter(int routerID, int addID) {
    
    int nodeID = (*global_router_map)[0][routerID].begin()->first;
    nodeID += addID;
    assert(isAttached(nodeID, routerID) == true);
    return nodeID;
}

vector<int>* getNodesAttached(int routerID) {
    vector<int>* nodes = new vector<int>();
    for(int i = 0; i < global_node_list->size(); i++) {
        if((*global_node_list)[i] == routerID) {
            nodes->push_back(i);
        }
    }
    return nodes;
}


//Basic adaptive routign algorithm for the anynet
void val_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
  //need x VCs for deadlock freedom

    
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
  
  outputs->Clear( );
  if(inject) {
    //int inject_vc= rand() % gNumVCs;
    outputs->AddRange(-1, vcBegin, vcEnd);
    //outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }
  
  
  
  int out_vc = 0;
  int out_port = -1;
  int dest  = f->dest;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int debug = f->watch;
    
    
  if(in_channel < gP_anynet) {
      f->ph = 0;
      int intermediateNode = rand() % global_node_list->size();
      
      while (isAttached(intermediateNode,sourceRouterID) || isAttached(intermediateNode,destRouterID)) {
        intermediateNode = rand() % global_node_list->size();
      }
      
      f->intm = intermediateNode;
      out_port = inject ? -1 : global_routing_table[r->GetID()].find(f->intm)->second;
      
      /*int intermRouterID = (*global_node_list)[f->intm];
      int pathHops = anynet_min_r2n_hopcnt(currentRouterID,f->intm) + anynet_min_r2n_hopcnt(intermRouterID,f->dest);
      int minPathHops = anynet_min_r2n_hopcnt(currentRouterID,f->dest);
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " originally : node " << f->src << " (router " << (*global_node_list)[f->src] <<
                ") ---> node " << f->dest << " (router " << (*global_node_list)[f->dest] << 
                "), path hops: " << minPathHops << "\t\t|   alternative path via node " << f->intm <<
                " (router " << intermRouterID << "), path hops: " << pathHops << endl;
      }*/
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->intm)->second;
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " at router " << currentRouterID << ", before the intermediate router " << endl; 
      }
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " at router " << currentRouterID << ", (the intermediate router) " << endl; 
      }
      
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){

      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " at router " << currentRouterID << ", before the dest router " << destRouterID << endl; 
      }
  }
  else {
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " at router " << currentRouterID << ", at the dest router " << destRouterID << endl; 
      }
  }

  //out_vc = f->ph;

  //outputs->AddRange( out_port, out_vc, out_vc );
    outputs->AddRange(out_port, vcBegin, vcEnd);
}

//Basic adaptive routign algorithm for the anynet
void ugall_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
  //need x VCs for deadlock freedom

    
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
  
  outputs->Clear( );
  if(inject) {
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  
  int out_port = -1;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int adaptiveThreshold = 30;
    
    if(in_channel < gP_anynet) {
      f->ph = 0;
      int intermediateNode = rand() % global_node_list->size();
      
      while (isAttached(intermediateNode,sourceRouterID) || isAttached(intermediateNode,destRouterID)) {
        intermediateNode = rand() % global_node_list->size();
      }
      int intermRouterID = (*global_node_list)[intermediateNode];
      
      int nonminPathHops = anynet_min_r2n_hopcnt(currentRouterID,intermediateNode) + anynet_min_r2n_hopcnt(intermRouterID,f->dest);
      int nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
      int nonminQueueSize = max((*global_router_list)[currentRouterID]->GetUsedCredit(nonMinOutput), 0);
      
      int minPathHops = anynet_min_r2n_hopcnt(currentRouterID,f->dest);
      int minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
      int minQueueSize = max((*global_router_list)[currentRouterID]->GetUsedCredit(minOutput), 0) ; 
      
      if ((minPathHops * minQueueSize ) <= (nonminPathHops * nonminQueueSize) + adaptiveThreshold ) {	
          f->ph = 1;
          out_port = minOutput;
          //cout << "HOPS: " << minPathHops << ", QUEUE SIZE: " << minPathHops * minQueueSize  << endl;
      }
      else {
          f->intm = intermediateNode;
          out_port = nonMinOutput;
          //cout << "HOPS: " << nonminPathHops << ", QUEUE SIZE: " << nonminPathHops * nonminQueueSize  << endl;
      }
      
      /*cout << "---------------------------------------------" << endl;
        cout << ">>>>>>>> NONMIN: " << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < queuesChosen.size(); m++) {
            cout << queuesChosen[m] << " ";
        }
        cout << ", sum: " << chosenMinQueueSize << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < routersChosen.size(); m++) {
            cout << routersChosen[m] << " ";
        }
        cout << ", hops: " << chosenHopsNr << endl;
        
        cout << ">>>>>>>> MIN: " << endl;
        cout << "(" << ((neighbors == true) ? "neighbors" : "not neighbors") << ")" << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < minQueues.size(); m++) {
            cout << minQueues[m] << " ";
        }
        cout << ", sum: " << minQueueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < minRouters.size(); m++) {
            cout << minRouters[m] << " ";
        }
        cout << ", hops: " << minPathHops << endl;
        
        cout << ">>> chosen: " << ((chosen == 0) ? " minimum " : " nonminimum ") << endl;*/
      
      if(verboseRouting) {
        cout << "Flit " << f->id << " originally : node " << f->src << " (router " << (*global_node_list)[f->src] <<
                ") ---> node " << f->dest << " (router " << (*global_node_list)[f->dest] << 
                "), path hops: " << minPathHops << "\t\t|   alternative path via node " << f->intm <<
                " (router " << intermRouterID << "), path hops: " << nonminPathHops << endl;
      }
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->intm)->second;
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
      //f->ph = 2;
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else {
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }

  outputs->AddRange(out_port, vcBegin, vcEnd);
}









//Basic adaptive routign algorithm for the anynet
void ugall3hops_anynet( const Router *r, const Flit *f, int in_channel, 
		OutputSet *outputs, bool inject )
{
	//assert(gNumVCs==2);

	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST ) {
		vcBegin = gReadReqBeginVC;
		vcEnd   = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd   = gWriteReqEndVC;
	} else if ( f->type ==  Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd   = gReadReplyEndVC;
	} else if ( f->type ==  Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd   = gWriteReplyEndVC;
	}

	/*outputs->Clear( );
		if(inject) {
		int inject_vc= rand() % gNumVCs;
		outputs->AddRange(-1, inject_vc, inject_vc);
		return;
		}*/

	outputs->Clear( );
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}

	int out_vc = 0;

	int out_port = -1;
	int currentRouterID =  r->GetID(); 
	int sourceRouterID = (*global_node_list)[f->src];
	int destRouterID = (*global_node_list)[f->dest];

	int adaptiveThreshold = -90;

	if(in_channel < gP_anynet) {
		//cout << "---------------------------------------------" << endl;
		f->ph = 0;

		assert(currentRouterID == sourceRouterID);

		int chosenMinOutput = -1;
		int chosenMinQueueSize = numeric_limits<int>::max(); 
		int chosenIntNode = -1;
		int chosenHopsNr = numeric_limits<int>::max();

		//vector<int> queuesChosen;
		//vector<int> routersChosen;
		vector<int> selectedRouters;

		int attemptsNr = 4;
		int i = 0;
		while(i < attemptsNr) {

			int rid = rand() % global_router_list->size();

			int srcIntermRouterID = 1;
			int intermDestRouterID = 1;

			int intermediateNode = -1;
			int intermRouterID = -1;

			intermediateNode = getRandomNodeAttachedToRouter(rid);
			intermRouterID = (*global_node_list)[intermediateNode];

			assert(intermRouterID == rid);

			srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
			intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

			////if(rid == sourceRouterID || rid == destRouterID) 
			////while(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
			while(1) {
				////continue;
				rid = rand() % global_router_list->size();

				if(rid == sourceRouterID || rid == destRouterID) {
					continue;
				}	

				//vector<int> queues;
				//vector<int> routers;

				//routers.push_back(sourceRouterID);

				intermediateNode = getRandomNodeAttachedToRouter(rid);
				intermRouterID = (*global_node_list)[intermediateNode];

				assert(intermRouterID == rid);

				srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
				intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

////				cout << "src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
				////if(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
				if(!(*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) { // || srcIntermRouterID == intermDestRouterID) {
					continue;
				}
				else if((*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) {
					srcIntermRouterID = -1;
					break;
				}
				else if(!(*global_neighbors)[sourceRouterID][rid] && (*global_neighbors)[rid][destRouterID]) {
					intermDestRouterID = -1;
					break;
				}
				else {
					srcIntermRouterID = -1;
					intermDestRouterID = -1;
					break;
				}
			}

			//printf("src: %d ----> %d ----> %d (rid) ----> %d ----> dest %d\n", sourceRouterID, srcIntermRouterID, rid, intermDestRouterID, destRouterID);
			////cout << "FINAL: src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
			/***bool repeat = false;
			for(int m = 0; m < selectedRouters.size(); m++) {
				if(selectedRouters[m] == rid) {
					repeat = true;
					break;
				}
			}
			if(repeat) {
				continue;
			}*/


			selectedRouters.push_back(rid);
			i++;




			/****if((*global_neighbors)[sourceRouterID][intermRouterID] && srcIntermRouterID > 0) {
				int choice = rand() % 2;
				if(choice == 0) {
					srcIntermRouterID = -1;
				}
			}
			if((*global_neighbors)[intermRouterID][destRouterID] && intermDestRouterID > 0) {
				int choice = rand() % 2;
				if(choice == 0) {
					intermDestRouterID = -1;
				}
			}*/

			int nonminPathHops = 0;
			int nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
			int nonminQueueSize = max((*global_router_list)[currentRouterID]->GetUsedCredit(nonMinOutput), 0);

			int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
			int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

			if (srcIntermNodeID >= 0 && intermDestNodeID >= 0) {
				nonminPathHops = 4;
				//TODO: make the code better
				printf(">>>>>>>>>>> src: %d ----> %d ----> %d (rid) ----> %d ----> dest %d\n", sourceRouterID, srcIntermRouterID, rid, intermDestRouterID, destRouterID);
				assert (1 == 2);
			}
			else if (srcIntermNodeID >= 0 && intermDestNodeID < 0) {
				nonminPathHops = 3;
			}
			else if (srcIntermNodeID < 0 && intermDestNodeID >= 0) {
				nonminPathHops = 3;
			}
			else if (srcIntermNodeID < 0 && intermDestNodeID < 0) {
				nonminPathHops = 2;
			}
			else {
				assert(1 == 2);
			}

			if(i == 1) {
				chosenMinQueueSize = nonminQueueSize; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				chosenMinOutput = nonMinOutput;
			}

			if(nonminPathHops * nonminQueueSize < chosenHopsNr * chosenMinQueueSize) {
				chosenMinQueueSize = nonminQueueSize; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				chosenMinOutput = nonMinOutput;
				//queuesChosen = queues;
				//routersChosen = routers;
			}

			/*  
					cout << ">>>>>>>> NONMIN: " << endl;
					cout << ">>> queues: ";
					for(int m = 0; m < queues.size(); m++) {
					cout << queues[m] << " ";
					}
					cout << ", sum: " << nonMinimumQueSizeSum << endl;
					cout << ">>> routers: ";
					for(int m = 0; m < routers.size(); m++) {
					cout << routers[m] << " ";
					}
					cout << ", hops: " << nonminPathHops << endl;*/

			/*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
				(nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
				(nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) */

		}

		int minPathHops = 0;
		int minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
		int minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);

		bool neighbors = true;

		//vector<int> minQueues;
		//vector<int> minRouters;

		//minRouters.push_back(sourceRouterID);

		assert(currentRouterID == sourceRouterID);

		if(sourceRouterID == destRouterID) {
			minPathHops = 0;
		}
		else if((*global_neighbors)[sourceRouterID][destRouterID]) {
			minPathHops = 1;
		}
		else {
			neighbors = false;
			minPathHops = 2;
		}

		int chosen = 0;
		if ((minPathHops * minQueueSize) <= (chosenHopsNr * chosenMinQueueSize) + adaptiveThreshold ) {	
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			f->intm = chosenIntNode;
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
			chosen = 1;
		}  
		////cout << ">>>>>>>>>> LIL <<<<<<<<<<<<<<<<" << endl;
		}
		else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
		}
		else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
			//f->ph = 2;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		out_vc = f->ph;

		//outputs->AddRange(out_port, out_vc, out_vc);
		outputs->AddRange(out_port, vcBegin, vcEnd);

		////cout << ">>>>>>>>>> KUKU <<<<<<<<<<<<<<<<" << endl;
	}












//Basic adaptive routign algorithm for the anynet
void ugall2hops_anynet( const Router *r, const Flit *f, int in_channel, 
		OutputSet *outputs, bool inject )
{
	//assert(gNumVCs==2);

	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST ) {
		vcBegin = gReadReqBeginVC;
		vcEnd   = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd   = gWriteReqEndVC;
	} else if ( f->type ==  Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd   = gReadReplyEndVC;
	} else if ( f->type ==  Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd   = gWriteReplyEndVC;
	}

	/*outputs->Clear( );
		if(inject) {
		int inject_vc= rand() % gNumVCs;
		outputs->AddRange(-1, inject_vc, inject_vc);
		return;
		}*/

	outputs->Clear( );
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}

	int out_vc = 0;

	int out_port = -1;
	int currentRouterID =  r->GetID(); 
	int sourceRouterID = (*global_node_list)[f->src];
	int destRouterID = (*global_node_list)[f->dest];

	int adaptiveThreshold = 0;

	if(in_channel < gP_anynet) {
		//cout << "---------------------------------------------" << endl;
		f->ph = 0;

		assert(currentRouterID == sourceRouterID);

		int chosenMinOutput = -1;
		int chosenMinQueueSize = numeric_limits<int>::max(); 
		int chosenIntNode = -1;
		int chosenHopsNr = numeric_limits<int>::max();

		//vector<int> queuesChosen;
		//vector<int> routersChosen;
		vector<int> selectedRouters;

		int attemptsNr = 4;
		int i = 0;
		while(i < attemptsNr) {

			int rid = rand() % global_router_list->size();

			int srcIntermRouterID = 1;
			int intermDestRouterID = 1;

			int intermediateNode = -1;
			int intermRouterID = -1;

			intermediateNode = getRandomNodeAttachedToRouter(rid);
			intermRouterID = (*global_node_list)[intermediateNode];

			assert(intermRouterID == rid);

			srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
			intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

			////if(rid == sourceRouterID || rid == destRouterID) 
			////while(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
			while(1) {
				////continue;
				rid = rand() % global_router_list->size();

				if(rid == sourceRouterID || rid == destRouterID) {
					continue;
				}	

				//vector<int> queues;
				//vector<int> routers;

				//routers.push_back(sourceRouterID);

				intermediateNode = getRandomNodeAttachedToRouter(rid);
				intermRouterID = (*global_node_list)[intermediateNode];

				assert(intermRouterID == rid);

				srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
				intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

				cout << "src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
				////if(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
				if(!(*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) { // || srcIntermRouterID == intermDestRouterID) {
					continue;
				}
				else if((*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) {
					//srcIntermRouterID = -1;
					continue;
				}
				else if(!(*global_neighbors)[sourceRouterID][rid] && (*global_neighbors)[rid][destRouterID]) {
					//intermDestRouterID = -1;
					continue;
				}
				else {
					srcIntermRouterID = -1;
					intermDestRouterID = -1;
					break;
				}
			}

			//printf("src: %d ----> %d ----> %d (rid) ----> %d ----> dest %d\n", sourceRouterID, srcIntermRouterID, rid, intermDestRouterID, destRouterID);
			cout << "FINAL: src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
			/***bool repeat = false;
			for(int m = 0; m < selectedRouters.size(); m++) {
				if(selectedRouters[m] == rid) {
					repeat = true;
					break;
				}
			}
			if(repeat) {
				continue;
			}*/


			selectedRouters.push_back(rid);
			i++;




			/****if((*global_neighbors)[sourceRouterID][intermRouterID] && srcIntermRouterID > 0) {
				int choice = rand() % 2;
				if(choice == 0) {
					srcIntermRouterID = -1;
				}
			}
			if((*global_neighbors)[intermRouterID][destRouterID] && intermDestRouterID > 0) {
				int choice = rand() % 2;
				if(choice == 0) {
					intermDestRouterID = -1;
				}
			}*/

			int nonminPathHops = 0;
			int nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
			int nonminQueueSize = max((*global_router_list)[currentRouterID]->GetUsedCredit(nonMinOutput), 0);

			int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
			int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

			if (srcIntermNodeID >= 0 && intermDestNodeID >= 0) {
				nonminPathHops = 4;
				//TODO: make the code better
				printf(">>>>>>>>>>> src: %d ----> %d ----> %d (rid) ----> %d ----> dest %d\n", sourceRouterID, srcIntermRouterID, rid, intermDestRouterID, destRouterID);
				assert (1 == 2);
			}
			else if (srcIntermNodeID >= 0 && intermDestNodeID < 0) {
				nonminPathHops = 3;
			}
			else if (srcIntermNodeID < 0 && intermDestNodeID >= 0) {
				nonminPathHops = 3;
			}
			else if (srcIntermNodeID < 0 && intermDestNodeID < 0) {
				nonminPathHops = 2;
			}
			else {
				assert(1 == 2);
			}

			if(i == 1) {
				chosenMinQueueSize = nonminQueueSize; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				chosenMinOutput = nonMinOutput;
			}

			if(nonminPathHops * nonminQueueSize < chosenHopsNr * chosenMinQueueSize) {
				chosenMinQueueSize = nonminQueueSize; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				chosenMinOutput = nonMinOutput;
				//queuesChosen = queues;
				//routersChosen = routers;
			}

			/*  
					cout << ">>>>>>>> NONMIN: " << endl;
					cout << ">>> queues: ";
					for(int m = 0; m < queues.size(); m++) {
					cout << queues[m] << " ";
					}
					cout << ", sum: " << nonMinimumQueSizeSum << endl;
					cout << ">>> routers: ";
					for(int m = 0; m < routers.size(); m++) {
					cout << routers[m] << " ";
					}
					cout << ", hops: " << nonminPathHops << endl;*/

			/*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
				(nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
				(nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) */

		}

		int minPathHops = 0;
		int minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
		int minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);

		bool neighbors = true;

		//vector<int> minQueues;
		//vector<int> minRouters;

		//minRouters.push_back(sourceRouterID);

		assert(currentRouterID == sourceRouterID);

		if(sourceRouterID == destRouterID) {
			minPathHops = 0;
		}
		else if((*global_neighbors)[sourceRouterID][destRouterID]) {
			minPathHops = 1;
		}
		else {
			neighbors = false;
			minPathHops = 2;
		}

		int chosen = 0;
		if ((minPathHops * minQueueSize) <= (chosenHopsNr * chosenMinQueueSize) + adaptiveThreshold ) {	
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			f->intm = chosenIntNode;
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
			chosen = 1;
		}  
		////cout << ">>>>>>>>>> LIL <<<<<<<<<<<<<<<<" << endl;
		}
		else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
		}
		else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
			//f->ph = 2;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		out_vc = f->ph;

		//outputs->AddRange(out_port, out_vc, out_vc);
		outputs->AddRange(out_port, vcBegin, vcEnd);

		////cout << ">>>>>>>>>> KUKU <<<<<<<<<<<<<<<<" << endl;
	}








// Adaptive routing for PolarFly
void ugall_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Threshold for using VAL path
	int adaptiveThreshold = global_adaptive_threshold;
	// Number of different VAL paths examined 
	int attemptsNr = 4;
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
		// Try different VAL paths, store best choice	
		int chosen_val_i_nid = -1;
		int chosen_val_hops= numeric_limits<int>::max();
		int chosen_val_queue_size = numeric_limits<int>::max(); 
		bool valid_val_path_found = false;
		// Build list of neighbors.
		// TODO: 	This should be done only once at the beginning but creating a global
		//			neighbourhood list yields invalid pointers and I can't see why...	
		vector<int> my_neighbors;
		for(int rid = 0; rid < (*global_router_list).size(); rid++){
			if ((*global_neighbors)[c_rid][rid]){
				my_neighbors.push_back(rid);
			}
		}
		// Use 4 attempts to select a next-hop router unequal to source and destination
		int i = 0;
		while(i < attemptsNr) {
			// Increment attempt number
			i++;
			// Select random router
			int i_rid = my_neighbors[rand() % my_neighbors.size()];	
			// Abort if we selected source or destination				
			if(i_rid == s_rid || i_rid == d_rid) {
				continue;
			}
			// Avoid selecting an intermediate hop whose path to destination contains the current hop
			// Only works reliable for diam-2 PF
			if(((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end())
				and ((*global_common_neighbors)[i_rid][d_rid] == c_rid)){
				continue;
			}
			// Get a node that is attached to the selected router
			int i_nid = getRandomNodeAttachedToRouter(i_rid);
			// Quality metrics for VAL path
			int val_hops = ((*global_neighbors)[i_rid][d_rid]) ? 2 : (((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()) ? 3 : 4) ;
			int val_out = global_routing_table[c_rid].find(i_nid)->second;
			int val_queue_size = max((*global_router_list)[c_rid]->GetUsedCredit(val_out), 0);
			// Chose this router if it is the first try or if it is better than the previous choice
			if(i == 1 || (val_hops * val_queue_size < chosen_val_hops * chosen_val_queue_size)){
				chosen_val_i_nid = i_nid;
				chosen_val_hops = val_hops; 
				chosen_val_queue_size = val_queue_size;
				valid_val_path_found = true;
			}
		}
		int min_hops = ((s_rid == d_rid) ? 0 : ((*global_neighbors)[s_rid][d_rid] ? 1 : (((*global_common_neighbors)[s_rid].find(d_rid) != (*global_common_neighbors)[s_rid].end()) ? 2 : 3)));
		int min_out = global_routing_table[s_rid].find(f->dest)->second;
		int min_queue_size = max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
		// Use min path
		if ((not valid_val_path_found) || ((min_hops * min_queue_size) <= (chosen_val_hops * chosen_val_queue_size + adaptiveThreshold ))) {	
			f->ph = 1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
		// Use VAL path
		else {
			f->intm = chosen_val_i_nid;
			out_port = global_routing_table[c_rid].find(f->intm)->second;
		}	
	}
	// This is not the first router: We use min routing
	else{
		out_port = global_routing_table[c_rid].find(f->dest)->second;
	}
	outputs->AddRange(out_port, vcBegin, vcEnd);
}


// Adaptive routing for PolarFly
void ugalg_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Number of different VAL paths examined 
	int attemptsNr = 4;
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
		// Try different VAL paths, store best choice	
		int chosen_val_i_nid = -1;
		int chosen_val_hops = numeric_limits<int>::max();
		int chosen_val_queue_size = numeric_limits<int>::max(); 
		// Build list of neighbors.
		// TODO: 	This should be done only once at the beginning but creating a global
		//			neighbourhood list yields invalid pointers and I can't see why...	
		vector<int> my_neighbors;
		for(int rid = 0; rid < (*global_router_list).size(); rid++){
			if ((*global_neighbors)[c_rid][rid]){
				my_neighbors.push_back(rid);
			}
		}
		// Use 4 attempts to select a next-hop router unequal to source and destination
		int i = 0;
		while(i < attemptsNr) {
			// Select random router
			int i_rid = my_neighbors[rand() % my_neighbors.size()];	
			// Abort if we selected source or destination				
			if(i_rid == s_rid|| i_rid == d_rid) {
				continue;
			}
			// Increment attempt number
			i++;
			// Get a node that is attached to the selected router
			int i_nid = getRandomNodeAttachedToRouter(i_rid);
			// Quality metrics for VAL path
			int val_hops = -1;
			int val_out = global_routing_table[c_rid].find(i_nid)->second;
			int val_queue_size = max((*global_router_list)[c_rid]->GetUsedCredit(val_out), 0);
			// If this is a path of length 2 (intermediate node connected to destination)
			if ((*global_neighbors)[i_rid][d_rid]){
				val_hops = 2;
				// Intermediate -> Destination
				int d_nid = getRandomNodeAttachedToRouter(d_rid);
				val_out = global_routing_table[i_rid].find(d_nid)->second;
				val_queue_size += max((*global_router_list)[i_rid]->GetUsedCredit(val_out), 0);
			}
			// If this is a path of length 3 (intermediate node not connected to destination)
			else{
				val_hops = 3;
				// Intermediate -> Intermediate 2
				int i2_rid = (*global_common_neighbors)[i_rid][d_rid];
				int i2_nid = getRandomNodeAttachedToRouter(i2_rid);
				val_out = global_routing_table[i_rid].find(i2_nid)->second;
				val_queue_size += max((*global_router_list)[i_rid]->GetUsedCredit(val_out), 0);
				// Intermediate 2 -> Destination
				int d_nid = getRandomNodeAttachedToRouter(d_rid);
				val_out = global_routing_table[i2_rid].find(d_nid)->second;
				val_queue_size += max((*global_router_list)[i2_rid]->GetUsedCredit(val_out), 0);
			}
			// Chose this router if it is the first try or if it is better than the previous choice
			if(i == 1 || (val_queue_size < chosen_val_queue_size)){
				chosen_val_i_nid = i_nid;
				chosen_val_hops = val_hops; 
				chosen_val_queue_size = val_queue_size;
			}
		}
		int min_hops = ((s_rid == d_rid) ? 0 : ((*global_neighbors)[s_rid][d_rid] ? 1 : 2));
		int min_queue_size = 0;
		if(s_rid != d_rid){
			int min_out = global_routing_table[s_rid].find(f->dest)->second;
			min_queue_size += max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
			if(not ((*global_neighbors)[s_rid][d_rid])){
				int i_rid = (*global_common_neighbors)[s_rid][d_rid];
				int d_nid = getRandomNodeAttachedToRouter(d_rid);
				min_out = global_routing_table[i_rid].find(d_nid)->second;
				min_queue_size += max((*global_router_list)[i_rid]->GetUsedCredit(min_out), 0);
			}
		}
		// Use min path
		if (min_queue_size <= chosen_val_queue_size) {	
			f->ph = 1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
		// Use VAL path
		else {
			f->intm = chosen_val_i_nid;
			out_port = global_routing_table[c_rid].find(f->intm)->second;
		}	
	}
	// This is not the first router: We use min routing
	else{
		out_port = global_routing_table[c_rid].find(f->dest)->second;
	}
	outputs->AddRange(out_port, vcBegin, vcEnd);
}

// Adaptive routing for PolarFly
void ugall2_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Threshold for using VAL path
	int adaptiveThreshold = global_adaptive_threshold;
	// Number of different VAL paths examined 
	int attemptsNr = 4;
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
		// Try different VAL paths, store best choice	
		int chosen_val_i_nid = -1;
		int chosen_val_hops= numeric_limits<int>::max();
		int chosen_val_queue_size = numeric_limits<int>::max(); 
		bool valid_val_path_found = false;
		// Use 4 attempts to select a next-hop router unequal to source and destination
		int i = 0;
		while(i < attemptsNr) {
			// Increment attempt number
			i++;
			// Select random router
			int i_rid = rand() % global_router_list->size();
			// Abort if we selected source or destination				
			if(i_rid == s_rid|| i_rid == d_rid) {
				continue;
			}
			// Avoid selecting an intermediate hop whose path to destination contains the current hop
			// Only works reliable for diam-2 PF
			if(((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end())
				and ((*global_common_neighbors)[i_rid][d_rid] == c_rid)){
				continue;
			}
			// Get a node that is attached to the selected router
			int i_nid = getRandomNodeAttachedToRouter(i_rid);
			// Quality metrics for VAL path
			int val_hops = 2;
			if(not ((*global_neighbors)[s_rid][i_rid])){
				val_hops += 1;
			}
			if(not ((*global_neighbors)[i_rid][d_rid])){
				val_hops += 1;
			}
			int val_out = global_routing_table[c_rid].find(i_nid)->second;
			int val_queue_size = max((*global_router_list)[c_rid]->GetUsedCredit(val_out), 0);
			// Chose this router if it is the first try or if it is better than the previous choice
			if(i == 1 || (val_hops * val_queue_size < chosen_val_hops * chosen_val_queue_size)){
				chosen_val_i_nid = i_nid;
				chosen_val_hops = val_hops; 
				chosen_val_queue_size = val_queue_size;
				valid_val_path_found = true;
			}
		}
		int min_hops = ((s_rid == d_rid) ? 0 : ((*global_neighbors)[s_rid][d_rid] ? 1 : 2));
		int min_out = global_routing_table[s_rid].find(f->dest)->second;
		int min_queue_size = max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
		// Use min path
		if ((not valid_val_path_found) or ((min_hops * min_queue_size) <= (chosen_val_hops * chosen_val_queue_size + adaptiveThreshold ))) {	
			f->ph = 1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
		// Use VAL path
		else {
			f->intm = chosen_val_i_nid;
			out_port = global_routing_table[c_rid].find(f->intm)->second;
		}	
	}
	// This is not the first router: We use min routing
	else{
		out_port = global_routing_table[c_rid].find(f->dest)->second;
	}
	outputs->AddRange(out_port, vcBegin, vcEnd);
}

// Adaptive routing for PolarFly
void ugalg2_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Number of different VAL paths examined 
	int attemptsNr = 4;
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
		// Try different VAL paths, store best choice	
		int chosen_val_i_nid = -1;
		int chosen_val_hops = numeric_limits<int>::max();
		int chosen_val_queue_size = numeric_limits<int>::max(); 
		// Use 4 attempts to select a next-hop router unequal to source and destination
		int i = 0;
		while(i < attemptsNr) {
			// Select random router
			int i_rid = rand() % global_router_list->size();
			// Abort if we selected source or destination				
			if(i_rid == s_rid|| i_rid == d_rid) {
				continue;
			}
			// Increment attempt number
			i++;
			// Get a node that is attached to the selected router
			int i_nid = getRandomNodeAttachedToRouter(i_rid);
			// Quality metrics for VAL path
			int val_hops = 2;
			int val_out = 0;
			int val_queue_size = 0;
			// If source and intermediate are adjacent
			if ((*global_neighbors)[s_rid][i_rid]){
				// Source -> Intermediate
				val_out = global_routing_table[s_rid].find(i_nid)->second;
				val_queue_size += max((*global_router_list)[s_rid]->GetUsedCredit(val_out), 0);
			}
			// If source and intermediate are not adjacent
			else{
				val_hops += 2;
				// Source -> Intermediate 0
				int i0_rid = (*global_common_neighbors)[s_rid][i_rid];
				int i0_nid = getRandomNodeAttachedToRouter(i0_rid);
				val_out = global_routing_table[s_rid].find(i0_nid)->second;
				val_queue_size += max((*global_router_list)[s_rid]->GetUsedCredit(val_out), 0);
				// Intermediate 0 -> Intermediate 
				val_out = global_routing_table[i0_rid].find(i_nid)->second;
				val_queue_size += max((*global_router_list)[i0_rid]->GetUsedCredit(val_out), 0);
			}
			// If intermediate and destination are adjacent
			if ((*global_neighbors)[i_rid][d_rid]){
				// Intermediate -> Destination
				int d_nid = getRandomNodeAttachedToRouter(d_rid);
				val_out = global_routing_table[i_rid].find(d_nid)->second;
				val_queue_size += max((*global_router_list)[i_rid]->GetUsedCredit(val_out), 0);
			}
			// If intermediate and destination are not adjacent
			else{
				val_hops += 2;
				// Intermediate -> Intermediate 2
				int i2_rid = (*global_common_neighbors)[i_rid][d_rid];
				int i2_nid = getRandomNodeAttachedToRouter(i2_rid);
				val_out = global_routing_table[i_rid].find(i2_nid)->second;
				val_queue_size += max((*global_router_list)[i_rid]->GetUsedCredit(val_out), 0);
				// Intermediate 2 -> Destination
				int d_nid = getRandomNodeAttachedToRouter(d_rid);
				val_out = global_routing_table[i2_rid].find(d_nid)->second;
				val_queue_size += max((*global_router_list)[i2_rid]->GetUsedCredit(val_out), 0);
			}
			// Chose this router if it is the first try or if it is better than the previous choice
			if(i == 1 || (val_queue_size < chosen_val_queue_size)){
				chosen_val_i_nid = i_nid;
				chosen_val_hops = val_hops; 
				chosen_val_queue_size = val_queue_size;
			}
		}
		int min_hops = ((s_rid == d_rid) ? 0 : ((*global_neighbors)[s_rid][d_rid] ? 1 : 2));
		int min_queue_size = 0;
		if(s_rid != d_rid){
			int min_out = global_routing_table[s_rid].find(f->dest)->second;
			min_queue_size += max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
			if(not ((*global_neighbors)[s_rid][d_rid])){
				int i_rid = (*global_common_neighbors)[s_rid][d_rid];
				int i_nid = getRandomNodeAttachedToRouter(i_rid);
				min_out = global_routing_table[i_rid].find(i_nid)->second;
				min_queue_size += max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
			}
		}
		// Use min path
		if (min_queue_size <= chosen_val_queue_size) {	
			f->ph = 1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
		// Use VAL path
		else {
			f->intm = chosen_val_i_nid;
			out_port = global_routing_table[c_rid].find(f->intm)->second;
		}	
	}
	// This is not the first router: We use min routing
	else{
		out_port = global_routing_table[c_rid].find(f->dest)->second;
	}
	outputs->AddRange(out_port, vcBegin, vcEnd);
}


// Adaptive routing for PolarFly
void ugall3_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Threshold for using VAL path
	int adaptiveThreshold = global_adaptive_threshold;
	// Number of different VAL paths examined 
	int attemptsNr = 4;
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
		// Try different VAL paths, store best choice	
		int chosen_val_i_nid = -1;
		int chosen_val_hops= numeric_limits<int>::max();
		int chosen_val_queue_size = numeric_limits<int>::max(); 
		bool valid_val_path_found = false;
		// Build list of neighbors.
		// TODO: 	This should be done only once at the beginning but creating a global
		//			neighbourhood list yields invalid pointers and I can't see why...	
		vector<int> my_neighbors;
		for(int rid = 0; rid < (*global_router_list).size(); rid++){
			if ((*global_neighbors)[c_rid][rid]){
				my_neighbors.push_back(rid);
			}
		}
		vector<int> candidates_i_rid;

		// Choose 4 random neighbors
		for(int i = 0; i < 4; i++){
			candidates_i_rid.push_back(my_neighbors[rand() % my_neighbors.size()]);	
		}
		// Choose 4 random non-neighbors 
		for(int i = 0; i < 4; i++){
			while(true){	
				int i_rid = rand() % global_router_list->size();
				if(count(my_neighbors.begin(), my_neighbors.end(), i_rid) == 0){
					candidates_i_rid.push_back(i_rid);
					break;
				}
			}
		}
		for(int i_rid : candidates_i_rid){
			int candidate_hops = -1;
			// We selected source or destination:
			if(i_rid == s_rid || i_rid == d_rid) {
				continue;	// ABORT
			}
			// Path current -> destination has 1 hop
			if((*global_neighbors)[c_rid][d_rid]){
				// Path current -> intermediate has 1 hop
				if((*global_neighbors)[c_rid][i_rid]){
					// Path intermediate -> destination has 1 hop
					if((*global_neighbors)[i_rid][d_rid]){
						candidate_hops = 2;	
					}else{
						continue;
					}
				}
				else{
					// Path current -> intermediate has 2 hop
					if((*global_common_neighbors)[c_rid].find(i_rid) != (*global_common_neighbors)[c_rid].end()){
						// Path intermediate -> destination has 1 hop
						if((*global_neighbors)[i_rid][d_rid]){
							candidate_hops = 3;	
						}else{
							// Path intermediate -> destination has 2 hop
							if((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()){
								candidate_hops = 4;
							}
							else{
								continue;	// ABORT
							}
						}
					}
				}
			}
			else{ 
				// Path current -> destination has 2 hop
				if((*global_common_neighbors)[c_rid].find(d_rid) != (*global_common_neighbors)[c_rid].end()){
					// Path current -> intermediate has 1 hop
					if((*global_neighbors)[c_rid][i_rid]){
						// Path intermediate -> destination has 1 hop
						if((*global_neighbors)[i_rid][d_rid]){
							candidate_hops = 2;	
						}else{
							// Path intermediate -> destination has 2 hop
							if((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()){
								candidate_hops = 3;
							}
							else{
								continue;	// ABORT
							}
						}
					}
					else{
						// Path current -> intermediate has 2 hop
						if((*global_common_neighbors)[c_rid].find(i_rid) != (*global_common_neighbors)[c_rid].end()){
							// Path intermediate -> destination has 1 hop
							if((*global_neighbors)[i_rid][d_rid]){
								candidate_hops = 3;	
							}else{
								// Path intermediate -> destination has 2 hop
								if((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()){
									candidate_hops = 4;
								}
								else{
									candidate_hops = 5;
								}
							}
						}
					}
				}
				// Path current -> destination has 3 hop (only possible in extended PF with method 2
				else{
					// Path current -> intermediate has 1 hop
					if((*global_neighbors)[c_rid][i_rid]){
						// Path intermediate -> destination has 1 hop
						if((*global_neighbors)[i_rid][d_rid]){
							candidate_hops = 2;	
						}else{
							// Path intermediate -> destination has 2 hop
							if((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()){
								candidate_hops = 3;
							}
							else{
								candidate_hops = 4;
							}
						}
					}
					else{
						// Path current -> intermediate has 2 hop
						if((*global_common_neighbors)[c_rid].find(i_rid) != (*global_common_neighbors)[c_rid].end()){
							// Path intermediate -> destination has 1 hop
							if((*global_neighbors)[i_rid][d_rid]){
								candidate_hops = 3;	
							}else{
								// Path intermediate -> destination has 2 hop
								if((*global_common_neighbors)[i_rid].find(d_rid) != (*global_common_neighbors)[i_rid].end()){
									candidate_hops = 4;
								}
								else{
									candidate_hops = 5;
								}
							}
						}
					}

				}
			}
			int i_nid = getRandomNodeAttachedToRouter(i_rid);
			int candidate_output = global_routing_table[c_rid].find(i_nid)->second;
			int candidate_queue = max((*global_router_list)[c_rid]->GetUsedCredit(candidate_output), 0); 
			// Chose this router if it is the first try or if it is better than the previous choice
			if((not valid_val_path_found) || (candidate_hops * candidate_queue < chosen_val_hops * chosen_val_queue_size)){
				chosen_val_i_nid = i_nid;
				chosen_val_hops = candidate_hops; 
				chosen_val_queue_size = candidate_queue;
				valid_val_path_found = true;
			}
		}
		int min_hops = ((s_rid == d_rid) ? 0 : ((*global_neighbors)[s_rid][d_rid] ? 1 : (((*global_common_neighbors)[s_rid].find(d_rid) != (*global_common_neighbors)[s_rid].end()) ? 2 : 3)));
		int min_out = global_routing_table[s_rid].find(f->dest)->second;
		int min_queue_size = max((*global_router_list)[s_rid]->GetUsedCredit(min_out), 0);
		// Use min path
		if ((not valid_val_path_found) || ((min_hops * min_queue_size) <= (chosen_val_hops * chosen_val_queue_size + adaptiveThreshold ))) {	
			f->ph = 1;
			f->intm = -1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
		// Use VAL path
		else {
			f->intm = chosen_val_i_nid;
			out_port = global_routing_table[c_rid].find(f->intm)->second;
		}	
	}
	// This is not the first router: We use min routing
	else{
		if(f->intm >= 0){
			if(isAttached(f->intm, c_rid)){
				f->intm = -1;
				out_port = global_routing_table[c_rid].find(f->dest)->second;
			}	
			else{
				out_port = global_routing_table[c_rid].find(f->intm)->second;
			}
		}
		else{
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
	}
	outputs->AddRange(out_port, vcBegin, vcEnd);
}


//void ugall4_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
//	// Set virtual channel begin and end
//	int vcBegin = 0, vcEnd = gNumVCs-1;
//	if ( f->type == Flit::READ_REQUEST) {
//		vcBegin = gReadReqBeginVC;
//		vcEnd	 = gReadReqEndVC;
//	} else if ( f->type == Flit::WRITE_REQUEST ) {
//		vcBegin = gWriteReqBeginVC;
//		vcEnd	 = gWriteReqEndVC;
//	} else if ( f->type ==	Flit::READ_REPLY ) {
//		vcBegin = gReadReplyBeginVC;
//		vcEnd	 = gReadReplyEndVC;
//	} else if ( f->type ==	Flit::WRITE_REPLY ) {
//		vcBegin = gWriteReplyBeginVC;
//		vcEnd	 = gWriteReplyEndVC;
//	}
//	// If this flit is being injected, add range of virtual channels to output
//	outputs->Clear();
//	if(inject) {
//		outputs->AddRange(-1, vcBegin, vcEnd);
//		return;
//	}
//	// Port over which the flit should be sent
//	int out_port = -1;
//	// Ids of source, this and destination router
//	int c_rid = r->GetID(); 
//	int s_rid = (*global_node_list)[f->src];
//	int d_rid = (*global_node_list)[f->dest];
//	// If this is the flit's first router
//	if(in_channel < gP_anynet) {
//		// Set flit phase
//		f->ph = 0;
//		// Get info about min path
//		int min_output = global_routing_table[c_rid].find(f->dest)->second;
//		int min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(min_output), 0);
//		// If the queue on the min path is not full
//		if(3 * min_queue < 2 * global_buffer_size){
//			// Use min path
//			f->intm = -1;
//			out_port = global_routing_table[c_rid].find(f->dest)->second;
//		}
//		else{
//			// Find neighbor with smallest output queue towards it
//			int chosen_queue = INT_MAX;
//			int chosen_rid = -1;
//			// for(auto rid_tuple : (*global_neighbors)[c_rid]){
//			for(auto rid_tuple :  (*global_router_map)[1][c_rid]){
//				int rid = rid_tuple.first;
//				int nid = getRandomNodeAttachedToRouter(rid);
//				int non_min_output = global_routing_table[c_rid].find(nid)->second;
//				int non_min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(non_min_output), 0);
//				if(non_min_queue < chosen_queue){
//					chosen_queue = non_min_queue;
//					chosen_rid = rid;
//				}
//			}
//			// If path current -> destination has length 1
//			if ((*global_neighbors)[c_rid][d_rid]){
//				// Find all neighbors of the chosen neighbor
//				vector<int> candidates;
//				//for(auto rid_tuple : (*global_neighbors)[chosen_rid]){
//				for(auto rid_tuple :  (*global_router_map)[1][chosen_rid]){
//					int rid = rid_tuple.first;
//					if((not (*global_neighbors)[c_rid][rid]) && (rid != c_rid) && (rid != d_rid)){
//						candidates.push_back(rid);
//					}
//				}
//				// Select a random neighbor of the chosen neighbor as intermediate router
//				int final_rid = candidates[rand() % candidates.size()];
//				f->intm = getRandomNodeAttachedToRouter(final_rid);
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//			// If path source -> destination has length 2
//			else{
//				// Send do chosen neighbor
//				f->intm = getRandomNodeAttachedToRouter(chosen_rid);
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//		}
//	}
//	// This is not the first router: We use min routing
//	else{
//		// If this flit is on it's way to an intermediate router
//		if(f->intm >= 0){
//			// If we are the intermediate router
//			if(isAttached(f->intm, c_rid)){
//				// Send flit towards destination
//				f->intm = -1;
//				out_port = global_routing_table[c_rid].find(f->dest)->second;
//			}	
//			else{
//				// Send flit towards intermediate router
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//		}
//		// Send flit towards destination
//		else{
//			f->intm = -1;
//			out_port = global_routing_table[c_rid].find(f->dest)->second;
//		}
//	}
//	//cout << "F:" << f->id << "\tC: " << r->GetID() << "\tS: " << f->src / 4 << "\tI: " << f->intm / 4 << "\tD: " << f->dest / 4 << endl;
//	outputs->AddRange(out_port, vcBegin, vcEnd);
//}





void ugall4_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
	// Set virtual channel begin and end
	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST) {
		vcBegin = gReadReqBeginVC;
		vcEnd	 = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd	 = gWriteReqEndVC;
	} else if ( f->type ==	Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd	 = gReadReplyEndVC;
	} else if ( f->type ==	Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd	 = gWriteReplyEndVC;
	}
	// If this flit is being injected, add range of virtual channels to output
	outputs->Clear();
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}
	// Port over which the flit should be sent
	int out_port = -1;
	// Ids of source, this and destination router
	int c_rid = r->GetID(); 
	int s_rid = (*global_node_list)[f->src];
	int d_rid = (*global_node_list)[f->dest];
	// If this is the flit's first router
	if(in_channel < gP_anynet) {
		// Set flit phase
		f->ph = 0;
        vcBegin = 0;
        vcEnd   = 0;
		// Get info about min path
		int min_output = global_routing_table[c_rid].find(f->dest)->second;
		int min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(min_output), 0);
		int minpath_out_port = global_routing_table[c_rid].find(f->dest)->second;
		// If the queue on the min path is not full
		if(3 * min_queue < 2 * global_buffer_size){
			// Use min path
			f->intm = -1;
			out_port = minpath_out_port;//global_routing_table[c_rid].find(f->dest)->second;
		}
		else{
			// Find neighbor with smallest output queue towards it
			int chosen_queue = INT_MAX;
			int chosen_rid = -1;
            int chosen_non_min_output = -1;
			// for(auto rid_tuple : (*global_neighbors)[c_rid]){
			for(auto rid_tuple :  (*global_router_map)[1][c_rid]){
				int rid = rid_tuple.first;
				int nid = getRandomNodeAttachedToRouter(rid);
				int non_min_output = global_routing_table[c_rid].find(nid)->second;
				int non_min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(non_min_output), 0);
				if(non_min_queue < chosen_queue){
					chosen_queue = non_min_queue;
					chosen_rid = rid;
                    chosen_non_min_output = non_min_output;
				}
			}

            
			// If path current -> destination has length 1
			if (chosen_non_min_output == minpath_out_port)
            {
                f->intm = -1;
                out_port = minpath_out_port;
            }
			else if ((*global_neighbors)[c_rid][d_rid]){
				// Find all neighbors of the chosen neighbor
				vector<int> candidates;
				//for(auto rid_tuple : (*global_neighbors)[chosen_rid]){
				for(auto rid_tuple :  (*global_router_map)[1][chosen_rid]){
					int rid = rid_tuple.first;
					if(((rid != c_rid) && (rid != d_rid))){
						candidates.push_back(rid);
					}
				}
				// Select a random neighbor of the chosen neighbor as intermediate router
				int final_rid = candidates[rand() % candidates.size()];
				f->intm = getRandomNodeAttachedToRouter(final_rid);
				out_port = global_routing_table[c_rid].find(f->intm)->second;
			}
			// If path source -> destination has length 2
			else{
				// Send do chosen neighbor
				f->intm = getRandomNodeAttachedToRouter(chosen_rid);
				out_port = global_routing_table[c_rid].find(f->intm)->second;
			}
		}

        //Select VC for injection
        int minVC   = 0;
        int minCred = max((*global_router_list)[c_rid]->GetUsedCreditVC(out_port, 0), 0);
        for (int i=0; i<gNumVCs; i++)
        {
            int cred = max((*global_router_list)[c_rid]->GetUsedCreditVC(out_port, i), 0); 
            if (cred < minCred)
            {
                minCred = cred;
                minVC   = i;
            }
        }
        vcBegin = minVC;
        vcEnd   = minVC;
	}
	// This is not the first router: We use min routing
	else{
        f->ph++;
        //vcBegin = f->ph;
        //vcEnd   = f->ph;
        //if (f->ph >= gNumVCs-1)
        //{
            vcBegin = 0;
            vcEnd   = gNumVCs - 1;
        //}

        //if (c_rid == d_rid)
        //{
        //    vcBegin = 0;
        //    vcEnd   = gNumVCs - 1;
        //}
        assert(vcEnd < gNumVCs); 
		// If this flit is on it's way to an intermediate router
		if(f->intm >= 0){
			// If we are the intermediate router
			if(isAttached(f->intm, c_rid)){
				// Send flit towards destination
				f->intm = -1;
				out_port = global_routing_table[c_rid].find(f->dest)->second;
			}	
			else{
				// Send flit towards intermediate router
				out_port = global_routing_table[c_rid].find(f->intm)->second;
			}
		}
		// Send flit towards destination
		else{
			f->intm = -1;
			out_port = global_routing_table[c_rid].find(f->dest)->second;
		}
	}
	//cout << "F:" << f->id << "\tC: " << r->GetID() << "\tS: " << f->src / 4 << "\tI: " << f->intm / 4 << "\tD: " << f->dest / 4 << endl;
	outputs->AddRange(out_port, vcBegin, vcEnd);
}





//// Adaptive routing for PolarFly
//void ugall4_pf_anynet(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject){
//	// Set virtual channel begin and end
//	int vcBegin = 0, vcEnd = gNumVCs-1;
//	if ( f->type == Flit::READ_REQUEST) {
//		vcBegin = gReadReqBeginVC;
//		vcEnd	 = gReadReqEndVC;
//	} else if ( f->type == Flit::WRITE_REQUEST ) {
//		vcBegin = gWriteReqBeginVC;
//		vcEnd	 = gWriteReqEndVC;
//	} else if ( f->type ==	Flit::READ_REPLY ) {
//		vcBegin = gReadReplyBeginVC;
//		vcEnd	 = gReadReplyEndVC;
//	} else if ( f->type ==	Flit::WRITE_REPLY ) {
//		vcBegin = gWriteReplyBeginVC;
//		vcEnd	 = gWriteReplyEndVC;
//	}
//	// If this flit is being injected, add range of virtual channels to output
//	outputs->Clear();
//	if(inject) {
//		outputs->AddRange(-1, vcBegin, vcEnd);
//		return;
//	}
//	// Port over which the flit should be sent
//	int out_port = -1;
//	// Ids of source, this and destination router
//	int c_rid = r->GetID(); 
//	int s_rid = (*global_node_list)[f->src];
//	int d_rid = (*global_node_list)[f->dest];
//	// If this is the flit's first router
//	if(in_channel < gP_anynet) {
//		// Set flit phase
//		f->ph = 0;
//		// Get info about min path
//		int min_output = global_routing_table[c_rid].find(f->dest)->second;
//		int min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(min_output), 0);
//		// If the queue on the min path is not full
//		if(min_queue < global_buffer_size){
//			// Use min path
//			f->intm = -1;
//			out_port = global_routing_table[c_rid].find(f->dest)->second;
//		}
//		else{
//			// Find neighbor with lowest output queue towards it
//			int chosen_queue = INT_MAX;
//			int chosen_rid = -1;
//			for(auto rid : global_neighbors[c_rid]){
//				int nid = getRandomNodeAttachedToRouter(rid.first);
//				int non_min_output = global_routing_table[c_rid].find(nid)->second;
//				int non_min_queue = max((*global_router_list)[c_rid]->GetUsedCredit(non_min_output), 0);
//				if(non_min_queue < chosen_queue){
//					chosen_queue = non_min_queue;
//					chosen_rid = rid.first;
//				}
//			}
//			// If path current -> destination has length 1
//			if ((*global_neighbors)[c_rid][d_rid]){
//				// Find all neighbors of the chosen neighbor
//				int rid = -1;
//				bool rid_is_neigh = false;
//				do{
//					rid = getRandomRouterAttachedToRouter(chosen_rid);
//					rid_is_neigh = ((*global_neighbors)[c_rid].find(rid)!=(*global_neighbors)[c_rid].end()) || ((*global_neighbors)[d_rid].find(rid)!=(*global_neighbors)[d_rid].end()) || (rid == c_rid) || (rid == d_rid);
//				}
//				while (rid_is_neigh);
//				// Select a random neighbor of the chosen neighbor as intermediate router
//				f->intm = getRandomNodeAttachedToRouter(rid);
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//			// If path source -> destination has length 2
//			else{
//				// Send do chosen neighbor
//				f->intm = getRandomNodeAttachedToRouter(chosen_rid);
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//		}
//	}
//	// This is not the first router: We use min routing
//	else{
//		// If this flit is on it's way to an intermediate router
//		if(f->intm >= 0){
//			// If we are the intermediate router
//			if(isAttached(f->intm, c_rid)){
//				// Send flit towards destination
//				f->intm = -1;
//				out_port = global_routing_table[c_rid].find(f->dest)->second;
//			}	
//			else{
//				// Send flit towards intermediate router
//				out_port = global_routing_table[c_rid].find(f->intm)->second;
//			}
//		}
//		// Send flit towards destination
//		else{
//			out_port = global_routing_table[c_rid].find(f->dest)->second;
//		}
//	}
//	outputs->AddRange(out_port, vcBegin, vcEnd);
//}



//Basic adaptive routign algorithm for the anynet
void ugallnew_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
  
  /*outputs->Clear( );
  if(inject) {
    int inject_vc= rand() % gNumVCs;
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }*/
    
  outputs->Clear( );
  if(inject) {
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  
  int out_vc = 0;
  
  int out_port = -1;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int adaptiveThreshold = 0;

   
    if(in_channel < gP_anynet) {
      //cout << "---------------------------------------------" << endl;
      f->ph = 0;
      
      assert(currentRouterID == sourceRouterID);
      
      int chosenMinOutput = -1;
      int chosenMinQueueSize = numeric_limits<int>::max(); 
      int chosenIntNode = -1;
      int chosenHopsNr = numeric_limits<int>::max();
      
      //vector<int> queuesChosen;
      //vector<int> routersChosen;
      vector<int> selectedRouters;
      
      int attemptsNr = 4;
      int i = 0;
        while(i < attemptsNr) {

          int rid = rand() % global_router_list->size();
            
          if(rid == sourceRouterID || rid == destRouterID) {
                continue;
          }
          
          for(int m = 0; m < selectedRouters.size(); m++) {
              if(selectedRouters[m] == rid) {
                  continue;
              }
          }
          selectedRouters.push_back(rid);
          i++;
          
          //vector<int> queues;
          //vector<int> routers;
          
          //routers.push_back(sourceRouterID);

          int intermediateNode = getRandomNodeAttachedToRouter(rid);
          int intermRouterID = (*global_node_list)[intermediateNode];
          
          assert(intermRouterID == rid);
          
          int srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
          int intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];
          
          if((*global_neighbors)[sourceRouterID][intermRouterID] && srcIntermRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  srcIntermRouterID = -1;
              }
          }
          if((*global_neighbors)[intermRouterID][destRouterID] && intermDestRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  intermDestRouterID = -1;
              }
          }
          
          int nonminPathHops = 0;
          int nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
          int nonminQueueSize = max((*global_router_list)[currentRouterID]->GetUsedCredit(nonMinOutput), 0);

          int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
          int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

          if (srcIntermNodeID >= 0 && intermDestNodeID >= 0) {
              nonminPathHops = 4;
          }
          else if (srcIntermNodeID >= 0 && intermDestNodeID < 0) {
              nonminPathHops = 3;
          }
          else if (srcIntermNodeID < 0 && intermDestNodeID >= 0) {
              nonminPathHops = 3;
          }
          else if (srcIntermNodeID < 0 && intermDestNodeID < 0) {
              nonminPathHops = 2;
          }
          else {
              assert(1 != 2);
          }
          
          if(i == 1) {
              chosenMinQueueSize = nonminQueueSize; 
              chosenIntNode = intermediateNode;
              chosenHopsNr = nonminPathHops;
              chosenMinOutput = nonMinOutput;
          }
          
          if(nonminPathHops * nonminQueueSize < chosenHopsNr * chosenMinQueueSize) {
              chosenMinQueueSize = nonminQueueSize; 
              chosenIntNode = intermediateNode;
              chosenHopsNr = nonminPathHops;
              chosenMinOutput = nonMinOutput;
              //queuesChosen = queues;
              //routersChosen = routers;
          }

        /*  
        cout << ">>>>>>>> NONMIN: " << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < queues.size(); m++) {
            cout << queues[m] << " ";
        }
        cout << ", sum: " << nonMinimumQueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < routers.size(); m++) {
            cout << routers[m] << " ";
        }
        cout << ", hops: " << nonminPathHops << endl;*/

          /*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
             (nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
             (nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) {*/
          
        }
        
        int minPathHops = 0;
        int minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
        int minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
        
        bool neighbors = true;
        
        //vector<int> minQueues;
        //vector<int> minRouters;
        
        //minRouters.push_back(sourceRouterID);

        assert(currentRouterID == sourceRouterID);
        
        if(sourceRouterID == destRouterID) {
            minPathHops = 0;
        }
        else if((*global_neighbors)[sourceRouterID][destRouterID]) {
            minPathHops = 1;
        }
        else {
            neighbors = false;
            minPathHops = 2;
        }
                  
        int chosen = 0;
        if ((minPathHops * minQueueSize) <= (chosenHopsNr * chosenMinQueueSize) + adaptiveThreshold ) {	
          f->ph = 1;
          out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        else {
            f->intm = chosenIntNode;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }  
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      out_port = global_routing_table[currentRouterID].find(f->intm)->second;
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
      //f->ph = 2;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else {
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  
  out_vc = f->ph;

  //outputs->AddRange(out_port, out_vc, out_vc);
  outputs->AddRange(out_port, vcBegin, vcEnd);
}














//Basic adaptive routign algorithm for the anynet
void ugalgnew_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
    
  outputs->Clear( );
  if(inject) {
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  
  int out_port = -1;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int adaptiveThreshold = 30;
  
  //int maxQueueSize = numeric_limits<int>::max();
    
    if(in_channel < gP_anynet) {
      //cout << "---------------------------------------------" << endl;
      f->ph = 0;
      
      assert(currentRouterID == sourceRouterID);
      
      int chosenMinOutput = -1;
      int chosenMinQueueSize = numeric_limits<int>::max(); 
      int chosenIntNode = -1;
      int chosenHopsNr = numeric_limits<int>::max();
      
      //vector<int> queuesChosen;
      //vector<int> routersChosen;
      vector<int> selectedRouters;
      
      int attemptsNr = 4;
      int i = 0;
        while(i < attemptsNr) {

          int rid = rand() % global_router_list->size();
            
          if(rid == sourceRouterID || rid == destRouterID) {
                continue;
          }
          
          for(int m = 0; m < selectedRouters.size(); m++) {
              if(selectedRouters[m] == rid) {
                  continue;
              }
          }
          selectedRouters.push_back(rid);
          i++;
          
          //vector<int> queues;
          //vector<int> routers;
          
          //routers.push_back(sourceRouterID);

          int intermediateNode = getRandomNodeAttachedToRouter(rid);
          int intermRouterID = (*global_node_list)[intermediateNode];
          
          assert(intermRouterID == rid);
          
          int srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
          int intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];
          
          if((*global_neighbors)[sourceRouterID][intermRouterID] && srcIntermRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  srcIntermRouterID = -1;
              }
          }
          if((*global_neighbors)[intermRouterID][destRouterID] && intermDestRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  intermDestRouterID = -1;
              }
          }
          
          int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
          int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

          int nonminPathHops = 0;
          int nonMinOutput = -1;
          int nonminQueueSize = -1;
          int nonMinimumQueSizeSum = 0;

          if (srcIntermNodeID >= 0) {
              nonMinOutput = global_routing_table[sourceRouterID].find(srcIntermNodeID)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(srcIntermRouterID);
              nonminPathHops++;

              nonMinOutput = global_routing_table[srcIntermRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[srcIntermRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }
          else {
              nonMinOutput = global_routing_table[sourceRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }     
          
          assert(nonminPathHops < 5);
          
          /*bool startNewLoop = false;
          for(int m = 0; m < queues.size(); m++) {
              if(queues[m] > maxQueueSize) {
                  startNewLoop = true;
                  break;
                }
          }
          if(startNewLoop) {
              continue;
          }*/
          
        /*  
        cout << ">>>>>>>> NONMIN: " << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < queues.size(); m++) {
            cout << queues[m] << " ";
        }
        cout << ", sum: " << nonMinimumQueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < routers.size(); m++) {
            cout << routers[m] << " ";
        }
        cout << ", hops: " << nonminPathHops << endl;*/
    
          /*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
             (nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
             (nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) {*/
          if(nonMinimumQueSizeSum < chosenMinQueueSize) {
              chosenMinQueueSize = nonMinimumQueSizeSum; 
              chosenIntNode = intermediateNode;
              chosenHopsNr = nonminPathHops;
              //queuesChosen = queues;
              //routersChosen = routers;
          }
        }
        
        int minPathHops = 0;//anynet_min_r2n_hopcnt(currentRouterID,f->dest);
        int minOutput = -1;
        int minQueueSize = -1;
        int minQueueSizeSum = 0;
        //int maxMinQueueSize = -1;
        
        bool neighbors = true;
        
        //vector<int> minQueues;
        //vector<int> minRouters;
        
        //minRouters.push_back(sourceRouterID);

        assert(currentRouterID == sourceRouterID);
        
        if(sourceRouterID == destRouterID) {
            minPathHops = 0;
        }
        else if((*global_neighbors)[sourceRouterID][destRouterID]) {
            minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //if(minQueueSize > maxMinQueueSize) {
            //    maxMinQueueSize = minQueueSize;
            //}
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(destRouterID);
            
            //assert(minPathHops == 1);
        }
        else {
            neighbors = false;
            
            int intermRouterID = (*global_common_neighbors)[sourceRouterID][destRouterID];
            
            int intermNodeID = getRandomNodeAttachedToRouter(intermRouterID);

            minOutput = global_routing_table[sourceRouterID].find(intermNodeID)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //if(minQueueSize > maxMinQueueSize) {
            //    maxMinQueueSize = minQueueSize;
            //}
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(intermRouterID);

            minOutput = global_routing_table[intermRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //if(minQueueSize > maxMinQueueSize) {
            //    maxMinQueueSize = minQueueSize;
            //}
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(destRouterID);
            
            //assert(minPathHops == 2);
        }
        
        //int tres = 0;
        int chosen = 0;
        // && chosenHopsNr <= minPathHops
        if ((chosenMinQueueSize < minQueueSizeSum) && minPathHops > 0) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }
        else {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      
      out_port = global_routing_table[currentRouterID].find(f->intm)->second;
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
      //f->ph = 2;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else {
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }

  outputs->AddRange(out_port, vcBegin, vcEnd);
}



//Basic adaptive routign algorithm for the anynet
void ugalg3hops_anynet( const Router *r, const Flit *f, int in_channel, 
		OutputSet *outputs, bool inject )
{
	//assert(gNumVCs==2);

	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST ) {
		vcBegin = gReadReqBeginVC;
		vcEnd   = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd   = gWriteReqEndVC;
	} else if ( f->type ==  Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd   = gReadReplyEndVC;
	} else if ( f->type ==  Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd   = gWriteReplyEndVC;
	}

	outputs->Clear( );
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}

	int out_port = -1;
	int currentRouterID =  r->GetID(); 
	int sourceRouterID = (*global_node_list)[f->src];
	int destRouterID = (*global_node_list)[f->dest];

	int adaptiveThreshold = 30;

	//int maxQueueSize = numeric_limits<int>::max();

	if(in_channel < gP_anynet) {
		//cout << "---------------------------------------------" << endl;
		f->ph = 0;

		assert(currentRouterID == sourceRouterID);

		int chosenMinOutput = -1;
		int chosenMinQueueSize = numeric_limits<int>::max(); 
		int chosenIntNode = -1;
		int chosenHopsNr = numeric_limits<int>::max();

		//vector<int> queuesChosen;
		//vector<int> routersChosen;
		vector<int> selectedRouters;

		int attemptsNr = 4;
		int i = 0;
		while(i < attemptsNr) {


			int rid = rand() % global_router_list->size();

      int srcIntermRouterID = 1;
      int intermDestRouterID = 1;

      int intermediateNode = -1;
      int intermRouterID = -1;

      intermediateNode = getRandomNodeAttachedToRouter(rid);
      intermRouterID = (*global_node_list)[intermediateNode];

      assert(intermRouterID == rid);

      srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
      intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

      ////if(rid == sourceRouterID || rid == destRouterID) 
      ////while(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
      while(1) {
        ////continue;
        rid = rand() % global_router_list->size();

        if(rid == sourceRouterID || rid == destRouterID) {
          continue;
        }

        //vector<int> queues;
        //vector<int> routers;

        //routers.push_back(sourceRouterID);

        intermediateNode = getRandomNodeAttachedToRouter(rid);
        intermRouterID = (*global_node_list)[intermediateNode];

        assert(intermRouterID == rid);

        srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
        intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

////        cout << "src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
        ////if(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
        if(!(*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) { // || srcIntermRouterID == intermDestRouterID) {
          continue;
        }
				else if((*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) {
          srcIntermRouterID = -1;
          break;
        }
        else if(!(*global_neighbors)[sourceRouterID][rid] && (*global_neighbors)[rid][destRouterID]) {
          intermDestRouterID = -1;
          break;
        }
        else {
          srcIntermRouterID = -1;
          intermDestRouterID = -1;
          break;
        }
      }

      selectedRouters.push_back(rid);
      i++;



			int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
			int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

			int nonminPathHops = 0;
			int nonMinOutput = -1;
			int nonminQueueSize = -1;
			int nonMinimumQueSizeSum = 0;

			if (srcIntermNodeID >= 0) {
				nonMinOutput = global_routing_table[sourceRouterID].find(srcIntermNodeID)->second;
				nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(srcIntermRouterID);
				nonminPathHops++;

				nonMinOutput = global_routing_table[srcIntermRouterID].find(intermediateNode)->second;
				nonminQueueSize = max((*global_router_list)[srcIntermRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(intermRouterID);
				nonminPathHops++;

				if (intermDestNodeID >= 0) {
					nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(intermDestRouterID);
					nonminPathHops++;

					nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
				else {
					nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
			}
			else {
				nonMinOutput = global_routing_table[sourceRouterID].find(intermediateNode)->second;
				nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(intermRouterID);
				nonminPathHops++;

				if (intermDestNodeID >= 0) {
					nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(intermDestRouterID);
					nonminPathHops++;

					nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
				else {
					nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
			}     

			assert(nonminPathHops < 4);

			/*bool startNewLoop = false;
				for(int m = 0; m < queues.size(); m++) {
				if(queues[m] > maxQueueSize) {
				startNewLoop = true;
				break;
				}
				}
				if(startNewLoop) {
				continue;
				}*/

			/*  
					cout << ">>>>>>>> NONMIN: " << endl;
					cout << ">>> queues: ";
					for(int m = 0; m < queues.size(); m++) {
					cout << queues[m] << " ";
					}
					cout << ", sum: " << nonMinimumQueSizeSum << endl;
					cout << ">>> routers: ";
					for(int m = 0; m < routers.size(); m++) {
					cout << routers[m] << " ";
					}
					cout << ", hops: " << nonminPathHops << endl;*/

			/*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
				(nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
				(nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) {*/
			if(nonMinimumQueSizeSum < chosenMinQueueSize) {
				chosenMinQueueSize = nonMinimumQueSizeSum; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				//queuesChosen = queues;
				//routersChosen = routers;
			}
		}

		int minPathHops = 0;//anynet_min_r2n_hopcnt(currentRouterID,f->dest);
		int minOutput = -1;
		int minQueueSize = -1;
		int minQueueSizeSum = 0;
		//int maxMinQueueSize = -1;

		bool neighbors = true;

		//vector<int> minQueues;
		//vector<int> minRouters;

		//minRouters.push_back(sourceRouterID);

		assert(currentRouterID == sourceRouterID);

		if(sourceRouterID == destRouterID) {
			minPathHops = 0;
		}
		else if((*global_neighbors)[sourceRouterID][destRouterID]) {
			minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
			minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(destRouterID);

			//assert(minPathHops == 1);
		}
		else {
			neighbors = false;

			int intermRouterID = (*global_common_neighbors)[sourceRouterID][destRouterID];

			int intermNodeID = getRandomNodeAttachedToRouter(intermRouterID);

			minOutput = global_routing_table[sourceRouterID].find(intermNodeID)->second;
			minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(intermRouterID);

			minOutput = global_routing_table[intermRouterID].find(f->dest)->second;
			minQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(destRouterID);

			//assert(minPathHops == 2);
		}

		//int tres = 0;
		int chosen = 0;
		// && chosenHopsNr <= minPathHops
		if ((chosenMinQueueSize < minQueueSizeSum) && minPathHops > 0) {
			f->intm = chosenIntNode;
			//nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
			//out_port = chosenMinOutput;
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
			chosen = 1;
		}
		else {
			f->ph = 1;
			//minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
			//out_port = chosenMinOutput;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		}
		else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){

			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
		}
		else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
			//f->ph = 2;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		outputs->AddRange(out_port, vcBegin, vcEnd);
	}








//Basic adaptive routign algorithm for the anynet
void ugalg2hops_anynet( const Router *r, const Flit *f, int in_channel, 
		OutputSet *outputs, bool inject )
{
	//assert(gNumVCs==2);

	int vcBegin = 0, vcEnd = gNumVCs-1;
	if ( f->type == Flit::READ_REQUEST ) {
		vcBegin = gReadReqBeginVC;
		vcEnd   = gReadReqEndVC;
	} else if ( f->type == Flit::WRITE_REQUEST ) {
		vcBegin = gWriteReqBeginVC;
		vcEnd   = gWriteReqEndVC;
	} else if ( f->type ==  Flit::READ_REPLY ) {
		vcBegin = gReadReplyBeginVC;
		vcEnd   = gReadReplyEndVC;
	} else if ( f->type ==  Flit::WRITE_REPLY ) {
		vcBegin = gWriteReplyBeginVC;
		vcEnd   = gWriteReplyEndVC;
	}

	outputs->Clear( );
	if(inject) {
		outputs->AddRange(-1, vcBegin, vcEnd);
		return;
	}

	int out_port = -1;
	int currentRouterID =  r->GetID(); 
	int sourceRouterID = (*global_node_list)[f->src];
	int destRouterID = (*global_node_list)[f->dest];

	int adaptiveThreshold = 30;

	//int maxQueueSize = numeric_limits<int>::max();

	if(in_channel < gP_anynet) {
		//cout << "---------------------------------------------" << endl;
		f->ph = 0;

		assert(currentRouterID == sourceRouterID);

		int chosenMinOutput = -1;
		int chosenMinQueueSize = numeric_limits<int>::max(); 
		int chosenIntNode = -1;
		int chosenHopsNr = numeric_limits<int>::max();

		//vector<int> queuesChosen;
		//vector<int> routersChosen;
		vector<int> selectedRouters;

		int attemptsNr = 4;
		int i = 0;
		while(i < attemptsNr) {


			int rid = rand() % global_router_list->size();

      int srcIntermRouterID = 1;
      int intermDestRouterID = 1;

      int intermediateNode = -1;
      int intermRouterID = -1;

      intermediateNode = getRandomNodeAttachedToRouter(rid);
      intermRouterID = (*global_node_list)[intermediateNode];

      assert(intermRouterID == rid);

      srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
      intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

      ////if(rid == sourceRouterID || rid == destRouterID) 
      ////while(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
      while(1) {
        ////continue;
        rid = rand() % global_router_list->size();

        if(rid == sourceRouterID || rid == destRouterID) {
          continue;
        }

        //vector<int> queues;
        //vector<int> routers;

        //routers.push_back(sourceRouterID);

        intermediateNode = getRandomNodeAttachedToRouter(rid);
        intermRouterID = (*global_node_list)[intermediateNode];

        assert(intermRouterID == rid);

        srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
        intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];

////        cout << "src: " << sourceRouterID << " ----> " << srcIntermRouterID << " ---->  rid: " << rid << " ----> " << intermDestRouterID << " ----> dest: " << destRouterID << endl;
        ////if(srcIntermRouterID >= 0 && intermDestRouterID >= 0 && srcIntermRouterID != intermDestRouterID) 
        if(!(*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) { // || srcIntermRouterID == intermDestRouterID) {
          continue;
        }
				else if((*global_neighbors)[sourceRouterID][rid] && !(*global_neighbors)[rid][destRouterID]) {
          //srcIntermRouterID = -1;
          continue;
        }
        else if(!(*global_neighbors)[sourceRouterID][rid] && (*global_neighbors)[rid][destRouterID]) {
          //intermDestRouterID = -1;
          continue;
        }
        else {
          srcIntermRouterID = -1;
          intermDestRouterID = -1;
          break;
        }
      }

      selectedRouters.push_back(rid);
      i++;



			int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
			int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

			int nonminPathHops = 0;
			int nonMinOutput = -1;
			int nonminQueueSize = -1;
			int nonMinimumQueSizeSum = 0;

			if (srcIntermNodeID >= 0) {
				nonMinOutput = global_routing_table[sourceRouterID].find(srcIntermNodeID)->second;
				nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(srcIntermRouterID);
				nonminPathHops++;

				nonMinOutput = global_routing_table[srcIntermRouterID].find(intermediateNode)->second;
				nonminQueueSize = max((*global_router_list)[srcIntermRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(intermRouterID);
				nonminPathHops++;

				if (intermDestNodeID >= 0) {
					nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(intermDestRouterID);
					nonminPathHops++;

					nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
				else {
					nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
			}
			else {
				nonMinOutput = global_routing_table[sourceRouterID].find(intermediateNode)->second;
				nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
				nonMinimumQueSizeSum += nonminQueueSize;

				//queues.push_back(nonminQueueSize);
				//routers.push_back(intermRouterID);
				nonminPathHops++;

				if (intermDestNodeID >= 0) {
					nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(intermDestRouterID);
					nonminPathHops++;

					nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
				else {
					nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
					nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
					nonMinimumQueSizeSum += nonminQueueSize;

					//queues.push_back(nonminQueueSize);
					//routers.push_back(destRouterID);
					nonminPathHops++;
				}
			}     

			assert(nonminPathHops < 4);

			/*bool startNewLoop = false;
				for(int m = 0; m < queues.size(); m++) {
				if(queues[m] > maxQueueSize) {
				startNewLoop = true;
				break;
				}
				}
				if(startNewLoop) {
				continue;
				}*/

			/*  
					cout << ">>>>>>>> NONMIN: " << endl;
					cout << ">>> queues: ";
					for(int m = 0; m < queues.size(); m++) {
					cout << queues[m] << " ";
					}
					cout << ", sum: " << nonMinimumQueSizeSum << endl;
					cout << ">>> routers: ";
					for(int m = 0; m < routers.size(); m++) {
					cout << routers[m] << " ";
					}
					cout << ", hops: " << nonminPathHops << endl;*/

			/*if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
				(nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
				(nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) {*/
			if(nonMinimumQueSizeSum < chosenMinQueueSize) {
				chosenMinQueueSize = nonMinimumQueSizeSum; 
				chosenIntNode = intermediateNode;
				chosenHopsNr = nonminPathHops;
				//queuesChosen = queues;
				//routersChosen = routers;
			}
		}

		int minPathHops = 0;//anynet_min_r2n_hopcnt(currentRouterID,f->dest);
		int minOutput = -1;
		int minQueueSize = -1;
		int minQueueSizeSum = 0;
		//int maxMinQueueSize = -1;

		bool neighbors = true;

		//vector<int> minQueues;
		//vector<int> minRouters;

		//minRouters.push_back(sourceRouterID);

		assert(currentRouterID == sourceRouterID);

		if(sourceRouterID == destRouterID) {
			minPathHops = 0;
		}
		else if((*global_neighbors)[sourceRouterID][destRouterID]) {
			minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
			minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(destRouterID);

			//assert(minPathHops == 1);
		}
		else {
			neighbors = false;

			int intermRouterID = (*global_common_neighbors)[sourceRouterID][destRouterID];

			int intermNodeID = getRandomNodeAttachedToRouter(intermRouterID);

			minOutput = global_routing_table[sourceRouterID].find(intermNodeID)->second;
			minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(intermRouterID);

			minOutput = global_routing_table[intermRouterID].find(f->dest)->second;
			minQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(minOutput), 0);
			minQueueSizeSum += minQueueSize;
			minPathHops++;

			//if(minQueueSize > maxMinQueueSize) {
			//    maxMinQueueSize = minQueueSize;
			//}

			//minQueues.push_back(minQueueSize);
			//minRouters.push_back(destRouterID);

			//assert(minPathHops == 2);
		}

		//int tres = 0;
		int chosen = 0;
		// && chosenHopsNr <= minPathHops
		if ((chosenMinQueueSize < minQueueSizeSum) && minPathHops > 0) {
			f->intm = chosenIntNode;
			//nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
			//out_port = chosenMinOutput;
			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
			chosen = 1;
		}
		else {
			f->ph = 1;
			//minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
			//out_port = chosenMinOutput;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		}
		else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){

			out_port = global_routing_table[currentRouterID].find(f->intm)->second;
		}
		else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
			f->ph = 1;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
			//f->ph = 2;
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}
		else {
			out_port = global_routing_table[currentRouterID].find(f->dest)->second;
		}

		outputs->AddRange(out_port, vcBegin, vcEnd);
	}













//Basic adaptive routign algorithm for the anynet
void ugallnew2_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
    
  //need x VCs for deadlock freedom

    
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
    
  outputs->Clear( );
  if(inject) {
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  
  int out_port = -1;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int adaptiveThreshold = 30;
  int maxQueueSize = numeric_limits<int>::max();
  
    
    if(in_channel < gP_anynet) {
      //cout << "---------------------------------------------" << endl;
      f->ph = 0;
      
      assert(currentRouterID == sourceRouterID);
      
      int chosenMinOutput = -1;
      int chosenMinQueueSize = numeric_limits<int>::max(); 
      int chosenIntNode = -1;
      int chosenHopsNr = numeric_limits<int>::max();
      
      vector<int> queuesChosen;
      vector<int> routersChosen;
      
      int attemptsNr = 6;
      int i = 0;
        while(i < attemptsNr) {

          int rid = rand() % global_router_list->size();
            
          if(rid == sourceRouterID || rid == destRouterID) {
                continue;
          }
          i++;
          
          vector<int> queues;
          vector<int> routers;
          
          routers.push_back(sourceRouterID);

          int intermediateNode = getAnyNodeAttachedToRouter(rid);
          int intermRouterID = (*global_node_list)[intermediateNode];
          
          assert(intermRouterID == rid);
          
          int srcIntermRouterID = commonNeighbor(sourceRouterID, intermRouterID, false);
          int intermDestRouterID = commonNeighbor(intermRouterID, destRouterID, false);

          int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
          int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

          int nonminPathHops = 0;
          int nonMinOutput = -1;
          int nonminQueueSize = -1;
          int nonMinimumQueSizeSum = 0;

          if (srcIntermNodeID >= 0) {
              nonMinOutput = global_routing_table[sourceRouterID].find(srcIntermNodeID)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              queues.push_back(nonminQueueSize);
              routers.push_back(srcIntermRouterID);
              nonminPathHops++;

              nonMinOutput = global_routing_table[srcIntermRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[srcIntermRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              queues.push_back(nonminQueueSize);
              routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }
          else {
              nonMinOutput = global_routing_table[sourceRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              queues.push_back(nonminQueueSize);
              routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                queues.push_back(nonminQueueSize);
                routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }     
          
          assert(nonminPathHops < 5);
          
          bool startNewLoop = false;
          for(int m = 0; m < queues.size(); m++) {
              if(queues[m] > maxQueueSize) {
                  startNewLoop = true;
                  break;
                }
          }
          if(startNewLoop) {
              continue;
          }
          
          
        cout << ">>>>>>>> NONMIN: " << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < queues.size(); m++) {
            cout << queues[m] << " ";
        }
        cout << ", sum: " << nonMinimumQueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < routers.size(); m++) {
            cout << routers[m] << " ";
        }
        cout << ", hops: " << nonminPathHops << endl;
          
          if((nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum == chosenMinQueueSize) ||
             (nonminPathHops == chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize) ||
             (nonminPathHops < chosenHopsNr && nonMinimumQueSizeSum < chosenMinQueueSize)) {
              //chosenMinOutput = nonMinOutput;
              //chosenMinOutput = global_routing_table[sourceRouterID].find(f->dest)->second
              chosenMinQueueSize = nonMinimumQueSizeSum; 
              chosenIntNode = intermediateNode;
              chosenHopsNr = nonminPathHops;
              queuesChosen = queues;
              routersChosen = routers;
          }
        }
        
        int minPathHops = anynet_min_r2n_hopcnt(currentRouterID,f->dest);
        int minOutput = -1;
        int minQueueSize = -1;
        int minQueueSizeSum = 0;
        int maxMinQueueSize = -1;
        
        bool neighbors = true;
        
        vector<int> minQueues;
        vector<int> minRouters;
        
        minRouters.push_back(sourceRouterID);

        assert(currentRouterID == sourceRouterID);
        
        if(areNeighbors(sourceRouterID,destRouterID, false)) {
            minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            
            if(minQueueSize > maxMinQueueSize) {
                maxMinQueueSize = minQueueSize;
            }
            
            minQueues.push_back(minQueueSize);
            minRouters.push_back(destRouterID);
            
            assert(minPathHops == 1);
        }
        else {
            neighbors = false;
            
            int intermRouterID = commonNeighbor(sourceRouterID,destRouterID, false);
            int intermNodeID = getAnyNodeAttachedToRouter(intermRouterID);

            minOutput = global_routing_table[sourceRouterID].find(intermNodeID)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            
            if(minQueueSize > maxMinQueueSize) {
                maxMinQueueSize = minQueueSize;
            }
            
            minQueues.push_back(minQueueSize);
            minRouters.push_back(intermRouterID);

            minOutput = global_routing_table[intermRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            
            if(minQueueSize > maxMinQueueSize) {
                maxMinQueueSize = minQueueSize;
            }
            
            minQueues.push_back(minQueueSize);
            minRouters.push_back(destRouterID);
            
            assert(minPathHops == 2);
        }
        
        int tres = 0;
        int chosen = 0;
        
        if (chosenMinQueueSize < minQueueSizeSum) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }
        else {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        
        /*
        if (maxMinQueueSize > 20 && chosenIntNode != -1) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }
        else {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }*/
        
        
        //if(minQueueSizeSum <= chosenMinQueueSize && minPathHops < chosenHopsNr) {
        /*if(chosenIntNode == -1 || (chosenIntNode != -1 && maxMinQueueSize > maxQueueSize)
           (minPathHops < chosenHopsNr && minQueueSizeSum == chosenMinQueueSize) ||
           (minPathHops == chosenHopsNr && minQueueSizeSum < chosenMinQueueSize+tres) ||
           (minPathHops < chosenHopsNr && minQueueSizeSum < chosenMinQueueSize+tres)) {
            
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        else {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }*/

        if(minQueueSizeSum > 0) {
            //cout << "A jednak!!!!!!!!!" << endl;
        }
       
       
        
        
        cout << ">>>>>>>> MIN: " << endl;
        cout << "(" << ((neighbors == true) ? "neighbors" : "not neighbors") << ")" << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < minQueues.size(); m++) {
            cout << minQueues[m] << " ";
        }
        cout << ", sum: " << minQueueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < minRouters.size(); m++) {
            cout << minRouters[m] << " ";
        }
        cout << ", hops: " << minPathHops << endl;
        
        cout << ">>> chosen: " << ((chosen == 0) ? " minimum " : " nonminimum ") << endl;
        
        //cout << "MIN:    hops: " << minPathHops << ", queue size: " << minQueueSizeSum << endl;
       // cout << "NONMIM: hops: " << chosenHopsNr << ", queue size: " << chosenMinQueueSize << endl;
       // cout << ">>> chosen: " << ((chosen == 0) ? " minimum " : " nonminimum ") << endl;
         
        
        
        
         /*  //cout << "HOPS: " << chosenHopsNr << ", QUEUE SIZE: " << chosenMinQueueSize << endl;
        if(chosenHopsNr == 1 || chosenHopsNr == 2) {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
            //cout << "chosen min path" << endl;
        }
        else if(chosenHopsNr > 2) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            //cout << "chosen non min path" << endl;
        }
        else {
            cout << "Wrong hops nr!!!" << endl;
            exit(-1);
        }*/
     
      //}
      
        
        
        
        
        
        
        
      /*
 if(verboseRouting) {
       cout << "Flit " << f->id << " originally : node " << f->src << " (router " << (*global_node_list)[f->src] <<
                ") ---> node " << f->dest << " (router " << (*global_node_list)[f->dest] << 
                ")\t\t|   alternative path via node " << chosenIntNode <<
                " (router " << intermRouterID << "), path hops: " << nonminPathHops << endl;
      }
 
 */
        
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->intm)->second;
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
      //f->ph = 2;
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else {
      out_port = inject ? -1 : global_routing_table[currentRouterID].find(f->dest)->second;
  }

  outputs->AddRange(out_port, vcBegin, vcEnd);
}








//Basic global adaptive routign algorithm for the anynet
void ugalg_anynet( const Router *r, const Flit *f, int in_channel, 
			OutputSet *outputs, bool inject )
{
  //need x VCs for deadlock freedom

    
  //assert(gNumVCs==2);
  
  int vcBegin = 0, vcEnd = gNumVCs-1;
  if ( f->type == Flit::READ_REQUEST ) {
    vcBegin = gReadReqBeginVC;
    vcEnd   = gReadReqEndVC;
  } else if ( f->type == Flit::WRITE_REQUEST ) {
    vcBegin = gWriteReqBeginVC;
    vcEnd   = gWriteReqEndVC;
  } else if ( f->type ==  Flit::READ_REPLY ) {
    vcBegin = gReadReplyBeginVC;
    vcEnd   = gReadReplyEndVC;
  } else if ( f->type ==  Flit::WRITE_REPLY ) {
    vcBegin = gWriteReplyBeginVC;
    vcEnd   = gWriteReplyEndVC;
  }
  
  outputs->Clear( );
  if(inject) {
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  
  int out_port = -1;
  int currentRouterID =  r->GetID(); 
  int sourceRouterID = (*global_node_list)[f->src];
  int destRouterID = (*global_node_list)[f->dest];
  
  int adaptiveThreshold = 30;
    
    if(in_channel < gP_anynet) {
      f->ph = 0;
      
      assert(currentRouterID == sourceRouterID);
      
      int chosenMinOutput = -1;
      int chosenMinQueueSize = numeric_limits<int>::max(); 
      int chosenIntNode = -1;
      int chosenHopsNr = numeric_limits<int>::max();
      
      //vector<int> queuesChosen;
      //vector<int> routersChosen;
      
  //    if(isAttached(f->dest,sourceRouterID)) {
  //        f->ph = 1;
  //        out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  //    }
  //    else {
      
      vector<int> routersToSelect;
      for(int i = 0; i < global_router_list->size(); i++) {
          if(i != sourceRouterID && i != destRouterID) {
                routersToSelect.push_back(i);
          }
      }
      
      int attemptsNr = 40;
      int i = 0;
        //for(int rid = 0; rid < global_router_list->size(); rid++) {
        while(!routersToSelect.empty()) {
            i++;
            if (i > attemptsNr) {
                break;
            }
          int rid = rand() % routersToSelect.size();
          rid = routersToSelect[rid];
            
          if(rid == sourceRouterID || rid == destRouterID) {
                continue;
          }
         
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == rid) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
          
          //vector<int> queues;
          //vector<int> routers;
          
          //routers.push_back(sourceRouterID);

          int intermediateNode = getRandomNodeAttachedToRouter(rid);

          int intermRouterID = (*global_node_list)[intermediateNode];
          
          assert(intermRouterID == rid);
          
          int srcIntermRouterID = (*global_common_neighbors)[sourceRouterID][intermRouterID];
          int intermDestRouterID = (*global_common_neighbors)[intermRouterID][destRouterID];
          
          /*if((*global_neighbors)[sourceRouterID][intermRouterID] && srcIntermRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  srcIntermRouterID = -1;
              }
          }
          if((*global_neighbors)[intermRouterID][destRouterID] && intermDestRouterID > 0) {
              int choice = rand() % 2;
              if(choice == 0) {
                  intermDestRouterID = -1;
              }
          }*/

          int srcIntermNodeID = (srcIntermRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(srcIntermRouterID);
          int intermDestNodeID = (intermDestRouterID == -1) ? -1 : getRandomNodeAttachedToRouter(intermDestRouterID);

          //int nonminPathHops = anynet_min_r2n_hopcnt(currentRouterID,intermediateNode) + anynet_min_r2n_hopcnt(intermRouterID,f->dest);
          //int nonminPathHops = anynet_min_r2r_hopcnt(currentRouterID,intermRouterID) + anynet_min_r2r_hopcnt(intermRouterID,destRouterID);

          int nonminPathHops = 0;
          int nonMinOutput = -1;
          int nonminQueueSize = -1;
          int nonMinimumQueSizeSum = 0;

          if (srcIntermNodeID >= 0) {
              nonMinOutput = global_routing_table[sourceRouterID].find(srcIntermNodeID)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(srcIntermRouterID);
              nonminPathHops++;

              nonMinOutput = global_routing_table[srcIntermRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[srcIntermRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }
          else {
              nonMinOutput = global_routing_table[sourceRouterID].find(intermediateNode)->second;
              nonminQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(nonMinOutput), 0);
              nonMinimumQueSizeSum += nonminQueueSize;
              
              //queues.push_back(nonminQueueSize);
              //routers.push_back(intermRouterID);
              nonminPathHops++;

              if (intermDestNodeID >= 0) {
                nonMinOutput = global_routing_table[intermRouterID].find(intermDestNodeID)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(intermDestRouterID);
                nonminPathHops++;

                nonMinOutput = global_routing_table[intermDestRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermDestRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
              else {
                nonMinOutput = global_routing_table[intermRouterID].find(f->dest)->second;
                nonminQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(nonMinOutput), 0);
                nonMinimumQueSizeSum += nonminQueueSize;
                
                //queues.push_back(nonminQueueSize);
                //routers.push_back(destRouterID);
                nonminPathHops++;
              }
          }

          assert(nonminPathHops < 5);
          
          //if ((nonminPathHops < chosenHopsNr) && (nonMinimumQueSizeSum <= chosenMinQueueSize)) {
          //if ((nonminPathHops < chosenHopsNr) && (nonMinimumQueSizeSum <= chosenMinQueueSize)) {
          //if ((nonMinimumQueSizeSum < chosenMinQueueSize) || ((nonMinimumQueSizeSum == chosenMinQueueSize) && (nonminPathHops < chosenHopsNr))) {	
          
          
         if(nonMinimumQueSizeSum < chosenMinQueueSize) {
              //chosenMinOutput = nonMinOutput;
              //chosenMinOutput = global_routing_table[sourceRouterID].find(f->dest)->second
              chosenMinQueueSize = nonMinimumQueSizeSum; 
              chosenIntNode = intermediateNode;
              chosenHopsNr = nonminPathHops;
              //queuesChosen = queues;
              //routersChosen = routers;
          }
        }
        
        int minPathHops = 0;//anynet_min_r2n_hopcnt(currentRouterID,f->dest);
        int minOutput = -1;
        int minQueueSize = -1;
        int minQueueSizeSum = 0;
        
        bool neighbors = true;
        
        //vector<int> minQueues;
        //vector<int> minRouters;
        
        //minRouters.push_back(sourceRouterID);

        assert(currentRouterID == sourceRouterID);
        
        if((*global_neighbors)[sourceRouterID][destRouterID]) {
            minOutput = global_routing_table[sourceRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(destRouterID);
            
            //assert(minPathHops == 1);
        }
        else {
            neighbors = false;
            
            int intermRouterID = (*global_common_neighbors)[sourceRouterID][destRouterID];
            int intermNodeID = getRandomNodeAttachedToRouter(intermRouterID);

            minOutput = global_routing_table[sourceRouterID].find(intermNodeID)->second;
            minQueueSize = max((*global_router_list)[sourceRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(intermRouterID);

            minOutput = global_routing_table[intermRouterID].find(f->dest)->second;
            minQueueSize = max((*global_router_list)[intermRouterID]->GetUsedCredit(minOutput), 0);
            minQueueSizeSum += minQueueSize;
            minPathHops++;
            
            //minQueues.push_back(minQueueSize);
            //minRouters.push_back(destRouterID);
            
            //assert(minPathHops == 2);
        }
        
        //int tres = 0;
        int chosen = 0;
        
        if (chosenMinQueueSize < minQueueSizeSum) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }
        else {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        
        /*
        //if(minQueueSizeSum <= chosenMinQueueSize && minPathHops < chosenHopsNr) {
        if((minPathHops < chosenHopsNr && minQueueSizeSum == chosenMinQueueSize) ||
           (minPathHops == chosenHopsNr && minQueueSizeSum < chosenMinQueueSize+tres) ||
           (minPathHops < chosenHopsNr && minQueueSizeSum < chosenMinQueueSize+tres)) {
            
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
        }
        else {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            chosen = 1;
        }*/

       
       /*
        cout << "---------------------------------------------" << endl;
        cout << ">>>>>>>> NONMIN: " << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < queuesChosen.size(); m++) {
            cout << queuesChosen[m] << " ";
        }
        cout << ", sum: " << chosenMinQueueSize << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < routersChosen.size(); m++) {
            cout << routersChosen[m] << " ";
        }
        cout << ", hops: " << chosenHopsNr << endl;
        
        cout << ">>>>>>>> MIN: " << endl;
        cout << "(" << ((neighbors == true) ? "neighbors" : "not neighbors") << ")" << endl;
        cout << ">>> queues: ";
        for(int m = 0; m < minQueues.size(); m++) {
            cout << minQueues[m] << " ";
        }
        cout << ", sum: " << minQueueSizeSum << endl;
        cout << ">>> routers: ";
        for(int m = 0; m < minRouters.size(); m++) {
            cout << minRouters[m] << " ";
        }
        cout << ", hops: " << minPathHops << endl;
        
        cout << ">>> chosen: " << ((chosen == 0) ? " minimum " : " nonminimum ") << endl;*/
        
        //cout << "MIN:    hops: " << minPathHops << ", queue size: " << minQueueSizeSum << endl;
       // cout << "NONMIM: hops: " << chosenHopsNr << ", queue size: " << chosenMinQueueSize << endl;
       // cout << ">>> chosen: " << ((chosen == 0) ? " minimum " : " nonminimum ") << endl;
         
        
        
        
         /*  //cout << "HOPS: " << chosenHopsNr << ", QUEUE SIZE: " << chosenMinQueueSize << endl;
        if(chosenHopsNr == 1 || chosenHopsNr == 2) {
            f->ph = 1;
            //minOutput = global_routing_table[currentRouterID].find(f->dest)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->dest)->second;
            //cout << "chosen min path" << endl;
        }
        else if(chosenHopsNr > 2) {
            f->intm = chosenIntNode;
            //nonMinOutput = global_routing_table[currentRouterID].find(intermediateNode)->second;
            //out_port = chosenMinOutput;
            out_port = global_routing_table[currentRouterID].find(f->intm)->second;
            //cout << "chosen non min path" << endl;
        }
        else {
            cout << "Wrong hops nr!!!" << endl;
            exit(-1);
        }*/
     
      //}
      
        
        
        
        
        
        
        
      /*
 if(verboseRouting) {
       cout << "Flit " << f->id << " originally : node " << f->src << " (router " << (*global_node_list)[f->src] <<
                ") ---> node " << f->dest << " (router " << (*global_node_list)[f->dest] << 
                ")\t\t|   alternative path via node " << chosenIntNode <<
                " (router " << intermRouterID << "), path hops: " << nonminPathHops << endl;
      }
 
 */
        
  }
  else if (f->ph == 0 && !isAttached(f->intm,currentRouterID)){
      
      out_port = global_routing_table[currentRouterID].find(f->intm)->second;
  }
  else if (f->ph == 0 && isAttached(f->intm,currentRouterID)) {
      f->ph = 1;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else if (f->ph == 1 && !isAttached(f->dest,currentRouterID)){
      //f->ph = 2;
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }
  else {
      out_port = global_routing_table[currentRouterID].find(f->dest)->second;
  }

  outputs->AddRange(out_port, vcBegin, vcEnd);
}

  vector<int>* getRouterNeighbors(int r1, int r2) {
      vector<int>* routers = new vector<int>();
      
      for(int i = 0; i < global_router_list->size(); i++) {
          int rID = (*global_router_list)[i]->GetID();
          if(rID == r2) {
              continue;
          }
          
          if(areNeighbors(r1,rID,false) && areNeighbors(r2,rID,false)) {
              continue;
          }
          
          if(areNeighbors(r1,rID,false)) {
              routers->push_back(rID);
          }
      }
      return routers;
  }
  
  
bool areNeighbors(int r1, int r2, bool verbose) {
    
    //map<int,   map<int, int >*>::const_iterator iter;
    map<int, pair<int,int> >::const_iterator iter;
    //cout << "[areNeighbors]" << endl;
    for(iter = (*global_router_map)[1][r1].begin(); iter != (*global_router_map)[1][r1].end(); iter++) {
    //for(iter = (*global_router_map)[1][0].begin(); iter != (*global_router_map)[1][0].end(); iter++) {
        if(iter->first == r2) {
            if(verbose) {
                cout << "[areNeighbors] router " << r1 << " <---> " << r2 << endl;
            }
            return true;
        }
    }
    return false;
}



  void AnyNet::buildBadTrafficPattern1NotRand() {
      
      cout << "========================== Building Bad traffic pattern, type 1... ============================" << endl;

   /*   bool* availableNodesForSend = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForSend[i] = true;
      }
      
      bool* availableNodesForReceive = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForReceive[i] = true;
      }
      
      vector<int> routersToRepeat;
      
      bool* availableNodes = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodes[i] = true;
      }
      
      int sumOfAvailableNodes = _nodes;
      int sumOfNodesSending = 0;
      
      map<int,map<int,int> >::const_iterator iter1;
      for(iter1 = channelsMap.begin(); iter1 != channelsMap.end(); iter1++) {
          map<int,int>::const_iterator iter2;
          for(iter2 = iter1->second.begin(); iter2 != iter1->second.end(); iter2++) {
              
                  int r1 = iter1->first;
                  int r2 = iter2->first;
                  
                  if((std::find(routersToRepeat.begin(),routersToRepeat.end(),r1) != routersToRepeat.end()) || 
                     (std::find(routersToRepeat.begin(),routersToRepeat.end(),r2) != routersToRepeat.end())) {
                      continue;
                  }
                  
                  routersToRepeat.push_back(r1);
                  routersToRepeat.push_back(r2);

                  assert(areNeighbors(r1,r2,false) == true);
                  
                  vector<int>* exclR1 = new vector<int>();
                  exclR1->push_back(r2);
                  
                  vector<int>* exclR2 = new vector<int>();
                  exclR2->push_back(r1);
 
                  vector<int>* neighborsR1 = getRouterNeighbors(r1,exclR1);
                  vector<int>* neighborsR2 = getRouterNeighbors(r2,exclR2);
                  
                  int barrierOfNodes = int(_p/2);
                  int barrierOfNodesForNeighbors = int(neighborsR1->size() / 4);
                  
                  int nodeDestForR1 = getAnyNodeAttachedToRouter(r2);
                  int nodeDestForR2 = getAnyNodeAttachedToRouter(r1);
                  
                  for(int i = 0; i < neighborsR1->size(); i++) {
                      vector<int>* nodesAttachedToNeighbors = getNodesAttached((*neighborsR1)[i]);
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true && j < barrierOfNodesForNeighbors) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR1;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[r1][r2] += 1;
                                sumOfNodesSending++;
                          }
                      }
                  }
                  
                  vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
                  for(int i = 0; i < nodesAttachedToR1->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR1)[i]] == true && i < barrierOfNodes) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR1)[i]] = nodeDestForR1;
                          availableNodes[(*nodesAttachedToR1)[i]] = false;
                          channelsMap[r1][r2] += 1;
                          sumOfNodesSending++;
                      }
                  }
                  
                  for(int i = 0; i < neighborsR2->size(); i++) {
                      vector<int>* nodesAttachedToNeighbors = getNodesAttached((*neighborsR2)[i]);
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR2;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[r2][r1] += 1;
                                sumOfNodesSending++;
                          }
                      }
                  }
                  
                  vector<int>* nodesAttachedToR2 = getNodesAttached(r2);
                  for(int i = 0; i < nodesAttachedToR2->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR2)[i]] == true) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR2)[i]] = nodeDestForR2;
                          availableNodes[(*nodesAttachedToR2)[i]] = false;
                          channelsMap[r2][r1] += 1;
                          sumOfNodesSending++; 
                      }
                  }
                  
              }
      //    }
      }
      cout << "=================== Finished building bad traffic pattern... ====================" << endl;
      printChannelsMap();
      
      cout << "Sum of nodes sending: " << sumOfNodesSending << endl;
      cout << "Sum of nodes: " << _nodes << endl;*/
  }
  
  
  
  void AnyNet::buildBadTrafficPattern1() {
      
    /*  cout << "========================== Building Bad traffic pattern, type 2... ============================" << endl;

      map<int,map<int,int> >::const_iterator iter1;
      
      vector<int> routersToRepeat;
      
      bool* availableNodesForSend = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForSend[i] = true;
      }
      
      bool* availableNodesForReceive = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForReceive[i] = true;
      }
      
      int sumOfAvailableNodes = _nodes;
      int sumOfNodesSending = 0;
      
      vector<int> routersToSelect;
      for(int i = 0; i < _size; i++) {
          routersToSelect.push_back(i);
      }

      int barrierOfNeighborAttempts = 40;
      
      while(!routersToSelect.empty()) {
          int r1 = rand() % routersToSelect.size();
          int r2 = rand() % routersToSelect.size();
          
          r1 = routersToSelect[r1];
          r2 = routersToSelect[r2];
          
          if(r1 == r2) {
              continue;
          }
          
          if(!areNeighbors(r1,r2,false)) {
              barrierOfNeighborAttempts -= 1;
              
              if(barrierOfNeighborAttempts > 0) {
                  continue;
              }
          }
          if(barrierOfNeighborAttempts <= 0) {
              break;
          }
          barrierOfNeighborAttempts = 40;
          
          cout << ((areNeighbors(r1,r2,false)) ? " NEIGHBORS " : " NOT NEIGHBORS ") << endl;
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r1) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r2) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
        
          cout << "Routers chosen: " << r1 << " " << r2 << ", still to choose: " << routersToSelect.size() << endl;
          
          vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
          vector<int>* nodesAttachedToR2 = getNodesAttached(r2);

          assert(nodesAttachedToR1->size() == nodesAttachedToR2->size());
          
          for(int j = 0; j < nodesAttachedToR1->size(); j++) {
              assert(availableNodesForSend[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedToR2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR2)[j]] == true);
              
              (*global_bad_traffic_table)[(*nodesAttachedToR1)[j]] = (*nodesAttachedToR2)[j];
              channelsMap[r1][r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[r2][r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
          }
          
          vector<int>* exclR1 = new vector<int>();
          exclR1->push_back(r2);
                  
          vector<int>* exclR2 = new vector<int>();
          exclR2->push_back(r1);
 
          vector<int>* neighborsR1 = getRouterNeighbors(r1,r2);
          vector<int>* neighborsR2 = getRouterNeighbors(r2,r1);
          
          assert(neighborsR1->size() == neighborsR2->size());
                  
          int barrierOfNodes = int(_p/2);
          int barrierOfNodesForNeighbors = int(neighborsR1->size() / 4);
                  
          int nodeDestForR1 = getAnyNodeAttachedToRouter(r2);
          int nodeDestForR2 = getAnyNodeAttachedToRouter(r1);
                  
          for(int i = 0; i < neighborsR1->size(); i++) {
              vector<int>* nodesAttachedToN1 = getNodesAttached((*neighborsR1)[i]);
              vector<int>* nodesAttachedToN2 = getNodesAttached((*neighborsR2)[i]);
              
              assert(nodesAttachedToN1->size() == nodesAttachedToN2->size());
              
              for(int j = 0; j < nodesAttachedToN1->size(); j++) {
                  if((availableNodesForSend[(*nodesAttachedToN1)[j]] == true) &&
                     (availableNodesForReceive[(*nodesAttachedToN2)[j]] == true)) {
                      
                      (*global_bad_traffic_table)[(*nodesAttachedToN1)[j]] = (*nodesAttachedToN2)[j];
                  }
                  
                  
                  
                  
                  
                  
              channelsMap[r1][r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[r2][r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
                  
                  
                  
                  assert(availableNodesForSend[(*nodesAttachedToR2)[j]] == true);
                  assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
                  assert();
              }
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true && j < barrierOfNodesForNeighbors) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR1;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[r1][r2] += 1;
                                sumOfNodesSending++;
                          }
                      }
          }
                  
                  vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
                  for(int i = 0; i < nodesAttachedToR1->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR1)[i]] == true && i < barrierOfNodes) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR1)[i]] = nodeDestForR1;
                          availableNodes[(*nodesAttachedToR1)[i]] = false;
                          channelsMap[r1][r2] += 1;
                          sumOfNodesSending++;
                      }
                  }
                  
                  for(int i = 0; i < neighborsR2->size(); i++) {
                      vector<int>* nodesAttachedToNeighbors = getNodesAttached((*neighborsR2)[i]);
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR2;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[r2][r1] += 1;
                                sumOfNodesSending++;
                          }
                      }
                  }
                  
                  vector<int>* nodesAttachedToR2 = getNodesAttached(r2);
                  for(int i = 0; i < nodesAttachedToR2->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR2)[i]] == true) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR2)[i]] = nodeDestForR2;
                          availableNodes[(*nodesAttachedToR2)[i]] = false;
                          channelsMap[r2][r1] += 1;
                          sumOfNodesSending++; 
                      }
                  }
      }
      
    
      cout << "=================== Finished building bad traffic pattern... ====================" << endl;
      printChannelsMap();
      
      cout << "Sum of nodes sending: " << sumOfNodesSending << endl;
      cout << "Sum of nodes: " << _nodes << endl;*/
  }

  
  
   void AnyNet::buildBadTrafficPattern3() {
      
      cout << "========================== Building Bad traffic pattern, type 3... ============================" << endl;

      map<int,map<int,int> >::const_iterator iter1;
      
      vector<int> routersToRepeat;
      
      bool* availableNodesForSend = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForSend[i] = true;
      }
      
      bool* availableNodesForReceive = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForReceive[i] = true;
      }
      
      int sumOfAvailableNodes = _nodes;
      int sumOfNodesSending = 0;
      
      vector<int> routersToSelect;
      for(int i = 0; i < _size; i++) {
          routersToSelect.push_back(i);
      }

      int barrierOfNeighborAttempts = 100;
     
			bool finished = false;
 
      while(!finished) {
          int r1 = rand() % routersToSelect.size();
          int r2 = rand() % routersToSelect.size();
          
          r1 = routersToSelect[r1];
          r2 = routersToSelect[r2];
         
					cout << ">>>>>>>>> Routers selected for traffic: " << r1 << ", " << r2 << endl;
 
          if(r1 == r2) {
						cout << "Repeating (the same)...";
              continue;
          }
          
          if(!areNeighbors(r1,r2,false)) {
              barrierOfNeighborAttempts -= 1;
              
              if(barrierOfNeighborAttempts > 0) {
									cout << "Repeating (no neighbors)...";
                  continue;
              }
							else {
								cout << "Too many failed attempts, finishing..." << endl;
								finished = true;
                continue;
							}
          }
          barrierOfNeighborAttempts = 100;
         

					//!!!!!!!!! now the second stage of selection - neighbors of selected r1 and r2

					vector<int>* r1_neighbors = getRouterNeighbors(r1,r2);
					vector<int>* r2_neighbors = getRouterNeighbors(r2,r1);
					
					//assert(r1_neighbors->size() == r2_neighbors->size());

          //cout << ((areNeighbors(r1,r2,false)) ? " NEIGHBORS " : " NOT NEIGHBORS ") << endl;
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r1) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r2) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
     
					bool repeat = false; 

					for(vector<int>::iterator iter_1 = r1_neighbors->begin(); iter_1 != r1_neighbors->end(); iter_1++) {
					  bool exists = false; 
					  for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == *iter_1) {
									exists = true;
                  routersToSelect.erase(iter);
									break;
              }
						}
						if(exists == false) {
							repeat = true;
							break;
						}
          }

					if(repeat == true) {
						cout << "Repeating (used)...";
						continue;
					}

					repeat = false;

					for(vector<int>::iterator iter_1 = r2_neighbors->begin(); iter_1 != r2_neighbors->end(); iter_1++) {
					bool exists = false; 
					for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == *iter_1) {
									exists = true;
                  routersToSelect.erase(iter);
									break;
              }
						}
						if(exists == false) {
							repeat = true;
							break;
						}
          }
				
					if(repeat == true) {
						cout << "Repeating (used 2)...";
						continue;
					}
 
          //cout << "Routers chosen: " << r1 << " " << r2 << ", still to choose: " << routersToSelect.size() << endl;
 
					cout << "********************* SUCCESS: selected " << r1 << " and " << r2 << endl;


				  vector<int>* nodesAttachedToorigR1 = getNodesAttached(r1);
				  vector<int>* nodesAttachedToorigR2 = getNodesAttached(r2);
		
		      vector<int>::iterator iter_2 = r2_neighbors->begin();
					for(vector<int>::iterator iter_1 = r1_neighbors->begin(); iter_1 != r1_neighbors->end(); iter_1++, iter_2++) {
				    vector<int>* nodesAttachedToR1 = getNodesAttached(*iter_1);
            vector<int>* nodesAttachedToR2 = getNodesAttached(*iter_2);

            assert(nodesAttachedToR1->size() == nodesAttachedToorigR2->size());
          
					  if(*iter_1 == r2 || *iter_2 == r1) continue;

            for(int j = 0; j < nodesAttachedToR1->size(); j++) {
              assert(availableNodesForSend[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedToorigR2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToorigR2)[j]] == true);
              
              (*global_bad_traffic_table)[(*nodesAttachedToR1)[j]] = (*nodesAttachedToorigR2)[j];
              channelsMap[*iter_1][r2] += 1;
              sumOfNodesSending++;
              //availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              //availableNodesForReceive[(*nodesAttachedToorigR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToorigR2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[r2][*iter_1] += 1;
              sumOfNodesSending++;
              //availableNodesForSend[(*nodesAttachedToorigR2)[j]] = false;
              //availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
            }
					}


		/*			vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
          vector<int>* nodesAttachedToR2 = getNodesAttached(r2);

          assert(nodesAttachedToR1->size() == nodesAttachedToR2->size());
          
          for(int j = 0; j < nodesAttachedToR1->size(); j++) {
              assert(availableNodesForSend[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedToR2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR2)[j]] == true);
              
              (*global_bad_traffic_table)[(*nodesAttachedToR1)[j]] = (*nodesAttachedToR2)[j];
              channelsMap[r1][r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[r2][r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
          }
*/

      }
      
     
      cout << "=================== Finished building worst-case traffic pattern... ====================" << endl;
      //printChannelsMap();
      
      cout << "Sum of nodes sending: " << sumOfNodesSending << endl;
      cout << "Sum of nodes: " << _nodes << endl;
  }

  void AnyNet::buildBadTrafficPatternHeaviestEdgeEqual() {
     
      cout << "========================== Building Bad traffic pattern, type HE+E... ============================" << endl;

      ofstream myfile;
      myfile.open("HeaviestEqualTrafficFewerEdges.mapping");

      map<int,map<int,int> >::const_iterator iter1;
      
      vector<int> routersToRepeat;
      
      bool* availableNodesForSend = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForSend[i] = true;
      }
      
      bool* availableNodesForReceive = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForReceive[i] = true;
      }
      
      int sumOfAvailableNodes = _nodes;
      int sumOfNodesSending = 0;
      
      vector<int> routersToSelect;
      for(int i = 0; i < _size; i++) {
          routersToSelect.push_back(i);
      }

      int barrierOfNeighborAttempts = 100;
     
			bool finished = false;
 
      int nodes_all = _nodes;
      int nodes_sending = 0;
      int nodes_receiving = 0;

      int total_repeated_iters = 0;
      int successes = 0;

      std::map<int,int> mapping;

      while(!finished) {
          if(total_repeated_iters++ > 100000 && successes > 0) {
          //if(successes > 0) {
            break;
          }

          int r1 = rand() % routersToSelect.size();
          int r2 = rand() % routersToSelect.size();
          int r3 = rand() % routersToSelect.size();
          int r4 = rand() % routersToSelect.size();
          
          r1 = routersToSelect[r1];
          r2 = routersToSelect[r2];
          r3 = routersToSelect[r3];
          r4 = routersToSelect[r4];
        
          if((std::find(routersToSelect.begin(), routersToSelect.end(), r1) == routersToSelect.end()) || 
             (std::find(routersToSelect.begin(), routersToSelect.end(), r2) == routersToSelect.end())) {
            continue;
          }
         
					//cout << "[" << total_repeated_iters << "]" << ">>>>>>>>> Routers selected for traffic: " << r1 << ", " << r2 << endl;
 
          if(r1 == r2) {
						//cout << "Repeating (the same)..." << endl;
              continue;
          }
          
          if(!areNeighbors(r1,r2,false)) {
              barrierOfNeighborAttempts -= 1;
              
              if(barrierOfNeighborAttempts > 0) {
									//cout << "Repeating (no neighbors)..." << endl;
                  continue;
              }
							else {
								//cout << "Too many failed attempts, finishing..." << endl;
								finished = true;
                continue;
							}
          }
          barrierOfNeighborAttempts = 100;

          //cout << "******************************* (1) PASSED *********************, selected " << r1 << " and " << r2 << endl;         

					//!!!!!!!!! now the second stage of selection - neighbors of selected r1 and r2

					vector<int>* r1_neighbors = getRouterNeighbors(r1,r2);
					vector<int>* r2_neighbors = getRouterNeighbors(r2,r1);
					
          int __r_r1 = -1;
          int __r_r2 = -1;
          bool success = false;

          for(vector<int>::iterator iter_1 = r1_neighbors->begin(); iter_1 != r1_neighbors->end(); iter_1++) {

             bool iter_1_ok = false;
              for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
                if(*iter_1 == *iter) {
                  iter_1_ok = true;
                }
              }
              if(iter_1_ok == false) {
                continue;
              }
          }

          //cout << "******************************* (2) PASSED *********************, selected " << r1 << " and " << r2 << endl;         

          for(vector<int>::iterator iter_2 = r2_neighbors->begin(); iter_2 != r2_neighbors->end(); iter_2++) {
              
              bool iter_2_ok = false;
              for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
                if(*iter_2 == *iter) {
                  iter_2_ok = true;
                }
              }
              if(iter_2_ok == false) {
                continue;
              }

          }

          //cout << "******************************* (3) PASSED *********************, selected " << r1 << " and " << r2 << endl;         

          for(vector<int>::iterator iter_1 = r1_neighbors->begin(); iter_1 != r1_neighbors->end(); iter_1++) {
            for(vector<int>::iterator iter_2 = r2_neighbors->begin(); iter_2 != r2_neighbors->end(); iter_2++) {
              if( !(*iter_1 == *iter_2) && !areNeighbors(r1,*iter_2, false) && !areNeighbors(r2, *iter_1, false)  
                && (std::find(routersToSelect.begin(), routersToSelect.end(), *iter_1) != routersToSelect.end())
                && (std::find(routersToSelect.begin(), routersToSelect.end(), *iter_2) != routersToSelect.end())  ) {
                __r_r1 = *iter_1;
                __r_r2 = *iter_2;
                success = true;
                goto __end;
              }
            }
          }
          
          __end:

          if(!success) {
									//cout << "Repeating (neighbors of neighbors problem)..." << endl;
                  continue;
          }

          //cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ (4) PASSED ********************* " << endl;         
					
          //assert(r1_neighbors->size() == r2_neighbors->size());

          //cout << ((areNeighbors(r1,r2,false)) ? " NEIGHBORS " : " NOT NEIGHBORS ") << endl;
         
         bool __r_r1_erased = false;
         bool __r_r2_erased = false;
         bool r1_erased = false;
         bool r2_erased = false;
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == __r_r1) {
                  routersToSelect.erase(iter);
                  __r_r1_erased = true;
                  break;
              }
          }
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == __r_r2) {
                  routersToSelect.erase(iter);
                  __r_r2_erased = true;
                  break;
              }
          }
        
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r1) {
                  routersToSelect.erase(iter);
                  r1_erased = true;
                  break;
              }
          }
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r2) {
                  routersToSelect.erase(iter);
                  r2_erased = true;
                  break;
              }
          }
         
          
          if(!__r_r1_erased || !__r_r2_erased || !r1_erased || !r2_erased) {
									//cout << "Repeating (cannot be erased!)..." << endl;
                  continue;
          }

          //assert(__r_r1_erased);
          //assert(__r_r2_erased);

					//cout << "********************* SUCCESS: selected " << __r_r1 << "---" << r1 << "---" << r2 << "---" << __r_r2 << endl;
		
          vector<int>* nodesAttachedTo__R_R1 = getNodesAttached(__r_r1);
          vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
          vector<int>* nodesAttachedTo__R_R2 = getNodesAttached(__r_r2);
          vector<int>* nodesAttachedToR2 = getNodesAttached(r2);

          assert(nodesAttachedToR1->size() == nodesAttachedTo__R_R1->size());
          assert(nodesAttachedToR1->size() == nodesAttachedToR2->size());
          assert(nodesAttachedToR2->size() == nodesAttachedTo__R_R2->size());
          
          for(int j = 0; j < nodesAttachedToR1->size(); j++) {
              assert(availableNodesForSend[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedToR2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR2)[j]] == true);
              
              assert(availableNodesForSend[(*nodesAttachedTo__R_R1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedTo__R_R2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedTo__R_R1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedTo__R_R2)[j]] == true);

              /////////////////////////////////////////

              //myfile << (*nodesAttachedTo__R_R1)[j] << " " << (*nodesAttachedToR2)[j] << std::endl;
              //myfile << (*nodesAttachedToR2)[j] << " " << (*nodesAttachedTo__R_R1)[j] << std::endl;
              //myfile << (*nodesAttachedTo__R_R2)[j] << " " << (*nodesAttachedToR1)[j] << std::endl;
              //myfile << (*nodesAttachedToR1)[j] << " " << (*nodesAttachedTo__R_R2)[j] << std::endl;

              mapping[(*nodesAttachedTo__R_R1)[j]] = (*nodesAttachedToR2)[j];
              mapping[(*nodesAttachedToR2)[j]] = (*nodesAttachedTo__R_R1)[j];
              mapping[(*nodesAttachedTo__R_R2)[j]] = (*nodesAttachedToR1)[j];
              mapping[(*nodesAttachedToR1)[j]] = (*nodesAttachedTo__R_R2)[j];

              nodes_sending += 4;
              nodes_receiving += 4;

              /////////////////////////////////////////

              (*global_bad_traffic_table)[(*nodesAttachedTo__R_R1)[j]] = (*nodesAttachedToR2)[j];
              channelsMap[__r_r1][r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedTo__R_R1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR2)[j]] = (*nodesAttachedTo__R_R1)[j];
              channelsMap[r2][__r_r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedTo__R_R1)[j]] = false;


              (*global_bad_traffic_table)[(*nodesAttachedTo__R_R2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[__r_r2][r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedTo__R_R2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR1)[j]] = (*nodesAttachedTo__R_R2)[j];
              channelsMap[r1][__r_r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedTo__R_R2)[j]] = false;

          }
          ++successes;
      }

    
      assert(nodes_sending == nodes_receiving);

      for (std::map<int,int>::iterator it=mapping.begin(); it!=mapping.end(); ++it) {
        int n1 = it->first;
        int n2 = it->second;

        myfile << n1 << " " << n2 << endl;
      }

      myfile << "Info: " << nodes_all << " " << nodes_sending << " " << nodes_receiving << " " << successes << std::endl;
      myfile.close();
      
     
      cout << "=================== Finished building worst-case traffic pattern... ====================" << endl;
      //printChannelsMap();
      
      cout << "Sum of nodes sending: " << sumOfNodesSending << endl;
      cout << "Sum of nodes: " << _nodes << endl;
  }
  

  
  void AnyNet::buildBadTrafficPattern2() {
      
      cout << "========================== Building Bad traffic pattern, type 2 (*** Seems the simplest ***)... ============================" << endl;

      map<int,map<int,int> >::const_iterator iter1;
     
      ofstream myfile;
      myfile.open("LighterEqualTrafficMoreEdges.mapping");
      
      int nodes_all = _nodes;
      int nodes_sending = 0;
      int nodes_receiving = 0;

      vector<int> routersToRepeat;
      
      bool* availableNodesForSend = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForSend[i] = true;
      }
      
      bool* availableNodesForReceive = new bool[_nodes];
      for(int i = 0; i < _nodes; i++) {
          availableNodesForReceive[i] = true;
      }
      
      int sumOfAvailableNodes = _nodes;
      int sumOfNodesSending = 0;
      
      vector<int> routersToSelect;
      for(int i = 0; i < _size; i++) {
          routersToSelect.push_back(i);
      }

      int barrierOfNeighborAttempts = 20;
     
      std::map<int,int> mapping;
      
      int successes = 0;

      while(!routersToSelect.empty()) {
          int r1 = rand() % routersToSelect.size();
          int r2 = rand() % routersToSelect.size();
          
          r1 = routersToSelect[r1];
          r2 = routersToSelect[r2];
          
          if(r1 == r2) {
              continue;
          }
          
          if(!areNeighbors(r1,r2,false)) {
              barrierOfNeighborAttempts -= 1;
              
              if(barrierOfNeighborAttempts > 0) {
                  continue;
              }
          }
          barrierOfNeighborAttempts = 20;
          
          //cout << ((areNeighbors(r1,r2,false)) ? " NEIGHBORS " : " NOT NEIGHBORS ") << endl;
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r1) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
          
          for(vector<int>::iterator iter = routersToSelect.begin(); iter != routersToSelect.end(); iter++) {
              if(*iter == r2) {
                  routersToSelect.erase(iter);
                  break;
              }
          }
        
          //cout << "Routers chosen: " << r1 << " " << r2 << ", still to choose: " << routersToSelect.size() << endl;
          
          vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
          vector<int>* nodesAttachedToR2 = getNodesAttached(r2);

          assert(nodesAttachedToR1->size() == nodesAttachedToR2->size());
          
          for(int j = 0; j < nodesAttachedToR1->size(); j++) {
              assert(availableNodesForSend[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForSend[(*nodesAttachedToR2)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR1)[j]] == true);
              assert(availableNodesForReceive[(*nodesAttachedToR2)[j]] == true);
         
              /////////////////////////////////////////

              //myfile << (*nodesAttachedToR1)[j] << " " << (*nodesAttachedToR2)[j] << std::endl;
              //myfile << (*nodesAttachedToR2)[j] << " " << (*nodesAttachedToR1)[j] << std::endl;

              mapping[(*nodesAttachedToR1)[j]] = (*nodesAttachedToR2)[j];
              mapping[(*nodesAttachedToR2)[j]] = (*nodesAttachedToR1)[j];

              nodes_sending += 2;
              nodes_receiving += 2;

              /////////////////////////////////////////


              
              (*global_bad_traffic_table)[(*nodesAttachedToR1)[j]] = (*nodesAttachedToR2)[j];
              channelsMap[r1][r2] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR1)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR2)[j]] = false;
              
              (*global_bad_traffic_table)[(*nodesAttachedToR2)[j]] = (*nodesAttachedToR1)[j];
              channelsMap[r2][r1] += 1;
              sumOfNodesSending++;
              availableNodesForSend[(*nodesAttachedToR2)[j]] = false;
              availableNodesForReceive[(*nodesAttachedToR1)[j]] = false;
          }
          successes++;
      }
      
      /*for(iter1 = channelsMap.begin(); iter1 != channelsMap.end(); iter1++) {
          map<int,int>::const_iterator iter2;
          for(iter2 = iter1->second.begin(); iter2 != iter1->second.end(); iter2++) {
         //     if(iter2->second == 0) {
                  
                 // cout << "Bad pattern for connection between routers " << iter1->first << " and " << iter2->first << endl;
 
                  int r1 = iter1->first;
                  int r2 = iter2->first;
                  
                  if((std::find(routersToRepeat.begin(),routersToRepeat.end(),r1) != routersToRepeat.end()) || 
                     (std::find(routersToRepeat.begin(),routersToRepeat.end(),r2) != routersToRepeat.end())) {
                      //continue;
                  }
                  
                  routersToRepeat.push_back(r1);
                  routersToRepeat.push_back(r2);

                  assert(areNeighbors(r1,r2,false) == true);
                  
                  vector<int>* exclR1 = new vector<int>();
                  exclR1->push_back(r2);
                  
                  vector<int>* exclR2 = new vector<int>();
                  exclR2->push_back(r1);
 
                  vector<int>* neighborsR1 = getRouterNeighbors(r1,exclR1);
                  vector<int>* neighborsR2 = getRouterNeighbors(r2,exclR2);
                  
                  int barrierOfNodes = int(_p/2);
                  int barrierOfNodesForNeighbors = int(neighborsR1->size() / 4);
                  
                  int nodeDestForR1 = getAnyNodeAttachedToRouter(r2);
                  int nodeDestForR2 = getAnyNodeAttachedToRouter(r1);
                  
                  for(int i = 0; i < neighborsR1->size(); i++) {
                      vector<int>* nodesAttachedToNeighbors = getNodesAttached((*neighborsR1)[i]);
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true && j < barrierOfNodesForNeighbors) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR1;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[iter1->first][iter2->first] += 1;
                                sumOfNodesSending++;
                          }
                      }
                  }
                  
                  vector<int>* nodesAttachedToR1 = getNodesAttached(r1);
                  for(int i = 0; i < nodesAttachedToR1->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR1)[i]] == true && i < barrierOfNodes) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR1)[i]] = nodeDestForR1;
                          availableNodes[(*nodesAttachedToR1)[i]] = false;
                          channelsMap[iter1->first][iter2->first] += 1;
                          sumOfNodesSending++;
                      }
                  }
                  
                  for(int i = 0; i < neighborsR2->size(); i++) {
                      vector<int>* nodesAttachedToNeighbors = getNodesAttached((*neighborsR2)[i]);
                      
                      for(int j = 0; j < nodesAttachedToNeighbors->size(); j++) {
                          if(availableNodes[(*nodesAttachedToNeighbors)[j]] == true) {
                                (*global_bad_traffic_table)[(*nodesAttachedToNeighbors)[j]] = nodeDestForR2;
                                availableNodes[(*nodesAttachedToNeighbors)[j]] = false;
                                channelsMap[iter1->first][iter2->first] += 1;
                          }
                      }
                  }
                  
                  vector<int>* nodesAttachedToR2 = getNodesAttached(r2);
                  for(int i = 0; i < nodesAttachedToR2->size(); i++) {
                      if(availableNodes[(*nodesAttachedToR2)[i]] == true) {
                          (*global_bad_traffic_table)[(*nodesAttachedToR2)[i]] = nodeDestForR2;
                          availableNodes[(*nodesAttachedToR2)[i]] = false;
                          channelsMap[iter1->first][iter2->first] += 1;
                      }
                  }
                  
              }
      //    }
      }*/



      assert(nodes_sending == nodes_receiving);
      //myfile << "Info: " << nodes_all << " " << nodes_sending << " " << nodes_receiving << std::endl;
      //myfile.close();

      for (std::map<int,int>::iterator it=mapping.begin(); it!=mapping.end(); ++it) {
        int n1 = it->first;
        int n2 = it->second;

        myfile << n1 << " " << n2 << endl;
      }

      myfile << "Info: " << nodes_all << " " << nodes_sending << " " << nodes_receiving << " " << successes << std::endl;
      myfile.close();
      
 

      cout << "=================== Finished building bad traffic pattern... ====================" << endl;
      //printChannelsMap();
      
      cout << "Sum of nodes sending: " << sumOfNodesSending << endl;
      cout << "Sum of nodes: " << _nodes << endl;
  }


void AnyNet::buildGoodTrafficPatternPF() {
    std::vector<int> quadric_routers;
    //find q
    int q = 0;
    for(int i=0; i<_size; i++)
        q += (*global_router_map)[1][i].size();
    q   = q/_size;
    printf("q = %d\n", q);
    assert(_size == q*q + q + 1);
    for (int i=0; i<_size; i++)
    {
        if ((*global_router_map)[1][i].size()==q)
            quadric_routers.push_back(i);
    }
    assert(quadric_routers.size() == q+1);
    //each quadric sends to other quadric
    for (int i=0; i<quadric_routers.size(); i++)
    {
        int src = quadric_routers[i];
        int dst = quadric_routers[(i + 1)%quadric_routers.size()];
        for (int j=0; j<_p; j++)
            (*global_bad_traffic_table)[src*_p + j] = dst*_p + j;
    }

    //create centers
    int v_quadric = quadric_routers[0];
    std::vector<int> centers;
    for(auto rid : (*global_router_map)[1][v_quadric])
        centers.push_back(rid.first);
    assert(centers.size() == q);
    for (int i=0; i<centers.size(); i++)
    {
        int src = centers[i];
        int dst = centers[(i + 1)%centers.size()];
        for (int j=0; j<_p; j++)
            (*global_bad_traffic_table)[src*_p + j] = dst*_p + j;
    }

    //create clusters
    std::vector<int>router_to_cluster (_size, -1);
    std::vector<bool>is_center(_size, false);
    std::vector<bool>is_quadric(_size, false);
    for (auto x : quadric_routers)
    {
        router_to_cluster[x]    = 0;
        is_quadric[x]           = true;
    }
    for (int i=0; i<q; i++)
    {
        router_to_cluster[centers[i]]   = i;
        is_center[centers[i]]           = true;
        for (auto rid : (*global_router_map)[1][centers[i]])
        {
            if (!is_quadric[rid.first])
                router_to_cluster[rid.first]= i; 
        }
    }
    //all routers should be assigned a cluster
    for (auto x : router_to_cluster)
        assert(x >= 0);

    std::vector<int>tri_vid (_size, -1);
    for (auto center : centers)
    {
        std::vector<int> tri1; //first vertex in a triangle
        std::vector<int> tri2; //second vertex in a triangle
        int cl = router_to_cluster[center];
        for (auto rid : (*global_router_map)[1][center])
        {
            int neigh   = rid.first;
            if (is_quadric[neigh]) continue;
            if (tri_vid[neigh] >= 0) continue;
            tri1.push_back(neigh);
            tri_vid[neigh]  = 1;
            
            int neigh_in_cluster  = 0;   
            for (auto nid : (*global_router_map)[1][neigh])
            {
                if (is_quadric[nid.first]) continue;
                if (router_to_cluster[nid.first] != cl) continue;
                if (nid.first == center) continue;
                neigh_in_cluster++;
                assert(tri_vid[nid.first] < 0);
                tri2.push_back(nid.first);
                tri_vid[nid.first]  = 2; 
            }
            if (neigh_in_cluster != 1)
                printf("found %d neighbors\n", neigh_in_cluster);
            assert(neigh_in_cluster==1);
        }
        assert(tri1.size() == (q-1)/2);
        assert(tri2.size() == (q-1)/2);

        for (int i=0; i<tri1.size(); i++)
        {
            int src = tri1[i];
            int dst = tri1[(i + 1)%tri1.size()];
            for (int j=0; j<_p; j++)
                (*global_bad_traffic_table)[src*_p + j] = dst*_p + j;
        }
        for (int i=0; i<tri2.size(); i++)
        {
            int src = tri2[i];
            int dst = tri2[(i + 1)%tri2.size()];
            for (int j=0; j<_p; j++)
                (*global_bad_traffic_table)[src*_p + j] = dst*_p + j;
        }
    }
    for (int i=0; i<_nodes; i++)
    {
        int src = i/_p;
        int dst = (*global_bad_traffic_table)[i]/_p;
    }
}
  
  
  
void AnyNet::buildBadTrafficPatternPF() {
    std::vector<int> quadric_routers;
    //find q
    int q = 0;
    for(int i=0; i<_size; i++)
        q += (*global_router_map)[1][i].size();
    q   = q/_size;
    printf("q = %d\n", q);
    for (int i=0; i<_size; i++)
    {
        if ((*global_router_map)[1][i].size()==q)
            quadric_routers.push_back(i);
    }
    assert(quadric_routers.size() == q+1);

    //lonely vertex, sends to itself
    int v_quadric = quadric_routers[0];
    assert((*global_router_map)[0][v_quadric].size() == _p);
    for (int i=0; i<_p; i++)
        (*global_bad_traffic_table)[v_quadric*_p + i] = v_quadric*_p + ((i+1)%_p); 

    //create centers
    std::vector<int> centers;
    for(auto rid : (*global_router_map)[1][v_quadric])
        centers.push_back(rid.first);
    assert(centers.size() == q);

    //create cluster
    std::vector<int>router_to_cluster (_size, -1);
    std::vector<bool>is_center(_size, false);
    std::vector<bool>is_quadric(_size, false);
    for (auto x : quadric_routers)
    {
        router_to_cluster[x]    = 0;
        is_quadric[x]           = true;
    }
    for (int i=0; i<q; i++)
    {
        router_to_cluster[centers[i]]   = i;
        is_center[centers[i]]           = true;
        for (auto rid : (*global_router_map)[1][centers[i]])
        {
            if (!is_quadric[rid.first])
                router_to_cluster[rid.first]= i; 
            else if (rid.first != v_quadric)
            {
                for (int j=0; j<_p; j++)
                {
                    (*global_bad_traffic_table)[centers[i]*_p + j]  = rid.first*_p + j; 
                    (*global_bad_traffic_table)[rid.first*_p + j]   = centers[i]*_p + j; 
                }
            }
        }
    }
    //all routers should be assigned a cluster
    for (auto x : router_to_cluster)
        assert(x >= 0);
    
    for (int i=0; i<_size; i++)
    {
        if (is_quadric[i] || is_center[i]) continue;
        int num_cluster_neigh = 0;
        for (auto rid : (*global_router_map)[1][i])
        {
            int neigh   = rid.first;
            if (is_quadric[neigh] || is_center[neigh]) continue;
            if (router_to_cluster[neigh] == router_to_cluster[i])
            {
                num_cluster_neigh++;
                for (int j=0; j<_p; j++)
                    (*global_bad_traffic_table)[i*_p + j]  = neigh*_p + j; 
            }
        }
        assert(num_cluster_neigh==1);
    } 

    printf("BAD TABLE\n");
    for (auto x : (*global_bad_traffic_table))
        printf("%d -> %d\n", x.first, x.second); 
    
}
    
  


  void AnyNet::printChannelsMap() {
      cout << "=================== Printing channels map... ====================" << endl;
      map<int,map<int,int> >::const_iterator iter1;
      for(iter1 = channelsMap.begin(); iter1 != channelsMap.end(); iter1++) {
          map<int,int>::const_iterator iter2;
          for(iter2 = iter1->second.begin(); iter2 != iter1->second.end(); iter2++) {
               cout << "Channel " << iter1->first << " <----> " << iter2->first << " = " << channelsMap[iter1->first][iter2->first] << endl;
          }
      }
      //exit(-1);
  }


void AnyNet::buildRoutingTable(){
  cout<<"========================== Build Routing table  =====================\n";  
  routing_table.resize(_size);
  for(int i = 0; i<_size; i++){
    cout << "Start searching for router " << i << flush ;
    route(i);
    cout << "Routing table for router " << i << " done  |" << endl << flush ;
  }
  cout << endl;
  global_routing_table = &routing_table[0];
  cout<<"========================== Build Routing table done  =====================\n";  
}




//11/7/2012
//basically djistra's, tested on a large dragonfly anynet configuration
void AnyNet::route(int r_start){
  int* dist = new int[_size];
  int* prev = new int[_size];
  set<int> rlist;
  //cout << "SIZE: " << _size << endl;
  for(int i = 0; i<_size; i++){
    dist[i] =  numeric_limits<int>::max();
    prev[i] = -1;
    rlist.insert(i);
  }
  dist[r_start] = 0;
  
  while(!rlist.empty()){
     // cout << rlist.size() << " " <<  flush;
    //find min 
      
    int min_dist = numeric_limits<int>::max();
    int min_cand = -1;
    for(set<int>::iterator i = rlist.begin();
	i!=rlist.end();
	i++){
      if(dist[*i]<min_dist){
	min_dist = dist[*i];
	min_cand = *i;
      }
    }
    rlist.erase(min_cand);

    //neighbor
    for(map<int,pair<int,int> >::iterator i = router_list[1][min_cand].begin(); 
	i!=router_list[1][min_cand].end(); 
	i++){
      int new_dist = dist[min_cand] + i->second.second;//distance is hops not cycles
      if(new_dist < dist[i->first]){
	dist[i->first] = new_dist;
	prev[i->first] = min_cand;
      }
    }
  }
  
  //post process from the prev list
  for(int i = 0; i<_size; i++){
    if(prev[i] ==-1){ //self
      assert(i == r_start);
      for(map<int, pair<int, int> >::iterator iter = router_list[0][i].begin();
	  iter!=router_list[0][i].end();
	  iter++){
	routing_table[r_start][iter->first]=iter->second.first;
	//cout<<"node "<<iter->first<<" port "<< iter->second.first<<endl;
      }
    } else {
      int distance=0;
      int neighbor=i;
      while(prev[neighbor]!=r_start){
	assert(router_list[1][neighbor].count(prev[neighbor])>0);
	distance+=router_list[1][prev[neighbor]][neighbor].second;//REVERSE lat
	neighbor= prev[neighbor];
      }
      distance+=router_list[1][prev[neighbor]][neighbor].second;//lat

      assert( router_list[1][r_start].count(neighbor)!=0);
      int port = router_list[1][r_start][neighbor].first;
      for(map<int, pair<int,int> >::iterator iter = router_list[0][i].begin();
	  iter!=router_list[0][i].end();
	  iter++){
	routing_table[r_start][iter->first]=port;
	//cout<<"node "<<iter->first<<" port "<< port<<" dist "<<distance<<endl;
      }
    }
  }
}



void AnyNet::printRoutingTable() { 
  cout << ">>>>>>>>>>>>>>>>>>>> Routing table printing: " << endl;
  
  for(int i = 0; i < routing_table.size(); i++) {
      cout << ">>>>>>>>> Routing table for router " << i << endl;
      map<int, int>::const_iterator itr;
      
      for(itr = routing_table[i].begin(); itr != routing_table[i].end(); itr++) {
          cout << "Port: " << (*itr).second << ", destination node: " << (*itr).first << endl;
      }
  }
  
  cout << ">>>>>>>>>>>>>>>>>>>> Routing table printing done" << endl;
}

void AnyNet::saveRoutingTable() {

  ofstream myfile(routing_table_file.c_str());;
  
  if(myfile.is_open()) {
    for(int i = 0; i < routing_table.size(); i++) {
        map<int, int>::const_iterator iter;
        for(iter = routing_table[i].begin(); iter != routing_table[i].end(); iter++) {
            myfile << i << " " << iter->first << " " << iter->second << endl;
        }
    }
    myfile.close();
  }
  else {
      cout << "Unable to open file " << routing_table_file << endl;
  }
      
      
  myfile.close();
}
  
void AnyNet::loadRoutingTable() {
  routing_table.resize(_size);
  string line;
  ifstream myfile(routing_table_file.c_str());
  
  if(myfile.is_open()) {
    vector<string> items;  
    while ( myfile.good() )
    {
      getline (myfile,line);
      //cout << line << endl;
      char sep = ' ';
      items = split(line, sep);
      
      int routerID = atoi(items[0].c_str());
      int nodeID = atoi(items[1].c_str());
      int outputID = atoi(items[2].c_str());
      
      (routing_table[routerID])[nodeID] = outputID;
    }
    myfile.close();
  }
  else {
      cout << "Unable to open file " << routing_table_file << endl;
  }
}



void AnyNet::readFile(){

  ifstream network_list;
  string line;
  enum ParseState{HEAD_TYPE=0,
		  HEAD_ID,
		  BODY_TYPE, 
		  BODY_ID,
		  LINK_WEIGHT};
  enum ParseType{NODE=0,
		 ROUTER,
		 UNKNOWN};

  network_list.open(file_name.c_str());
  if(!network_list.is_open()){
    cout<<"Anynet:can't open network file "<<file_name<<endl;
    exit(-1);
  }
  
  //loop through the entire file
  while(!network_list.eof()){
    getline(network_list,line);
    if(line==""){
      continue;
    }

    ParseState state=HEAD_TYPE;
    //position to parse out white sspace
    int pos = 0;
    int next_pos=-1;
    string temp;
    //the first node and its type
    int head_id = -1;
    ParseType head_type = UNKNOWN;
    //stuff that head are linked to
    ParseType body_type = UNKNOWN;
    int body_id = -1;
    int link_weight = 1;

    do{

      //skip empty spaces
      next_pos = line.find(" ",pos);
      temp = line.substr(pos,next_pos-pos);
      pos = next_pos+1;
      if(temp=="" || temp==" "){
	continue;
      }

      switch(state){
      case HEAD_TYPE:
	if(temp=="router"){
	  head_type = ROUTER;
	} else if (temp == "node"){
	  head_type = NODE;
	} else {
	  cout<<"Anynet:Unknow head of line type "<<temp<<"\n";
	  assert(false);
	}
	state=HEAD_ID;
	break;
      case HEAD_ID:
	//need better error check
	head_id = atoi(temp.c_str());
        
	//intialize router structures
	if(router_list[NODE].count(head_id) == 0){
	  router_list[NODE][head_id] = map<int, pair<int,int> >();
	}
	if(router_list[ROUTER].count(head_id) == 0){
	  router_list[ROUTER][head_id] = map<int, pair<int,int> >();
	}  

	state=BODY_TYPE;
	break;
      case LINK_WEIGHT:
	if(temp=="router"||
	   temp == "node"){
	  //ignore
	} else {
	  link_weight= atoi(temp.c_str());
	  router_list[head_type][head_id][body_id].second=link_weight;
	  break;
	}
	//intentionally letting it flow through
      case BODY_TYPE:
	if(temp=="router"){
	  body_type = ROUTER;
	} else if (temp == "node"){
	  body_type = NODE;
	} else {
	  cout<<"Anynet:Unknow body type "<<temp<<"\n";
	  assert(false);
	}
	state=BODY_ID;
	break;
      case BODY_ID:
	body_id = atoi(temp.c_str());	
        
	//intialize router structures if necessary
	if(body_type==ROUTER){
	  if(router_list[NODE].count(body_id) ==0){
	    router_list[NODE][body_id] = map<int, pair<int,int> >();
	  }
	  if(router_list[ROUTER].count(body_id) == 0){
	    router_list[ROUTER][body_id] = map<int, pair<int,int> >();
	  }
	}

	if(head_type==NODE && body_type==NODE){ 

	  cout<<"Anynet:Cannot connect node to node "<<temp<<"\n";
	  assert(false);

	} else if(head_type==NODE && body_type==ROUTER){

	  if(node_list.count(head_id)!=0 &&
	     node_list[head_id]!=body_id){
	    cout<<"Anynet:Node "<<body_id<<" trying to connect to multiple router "
		<<body_id<<" and "<<node_list[head_id]<<endl;
	    assert(false);
	  }
	  node_list[head_id]=body_id;
	  router_list[NODE][body_id][head_id]=pair<int, int>(-1,1);

	} else if(head_type==ROUTER && body_type==NODE){
	  //insert and check node
	  if(node_list.count(body_id) != 0 &&
	     node_list[body_id]!=head_id){
	    cout<<"Anynet:Node "<<body_id<<" trying to connect to multiple router "
		<<body_id<<" and "<<node_list[head_id]<<endl;
	    assert(false);
	  }
	  node_list[body_id] = head_id;
	  router_list[NODE][head_id][body_id]=pair<int, int>(-1,1);

	} else if(head_type==ROUTER && body_type==ROUTER){
	  router_list[ROUTER][head_id][body_id]=pair<int, int>(-1,1);
	  if(router_list[ROUTER][body_id].count(head_id)==0){
	    router_list[ROUTER][body_id][head_id]=pair<int, int>(-1,1);
	  }
	}
	state=LINK_WEIGHT;
	break ;
      default:
	cout<<"Anynet:Unknow parse state\n";
	assert(false);
	break;
      }

    } while(pos!=0);
    if(state!=LINK_WEIGHT &&
       state!=BODY_TYPE){
      cout<<"Anynet:Incomplete parse of the line: "<<line<<endl;
    }

  }

  //map verification, make sure the information contained in bother maps
  //are the same
  assert(router_list[0].size() == router_list[1].size());

  //traffic generator assumes node list is sequenctial and starts at 0
  vector<int> node_check;
  for(map<int,int>::iterator i = node_list.begin();
      i!=node_list.end();
      i++){
    node_check.push_back(i->first);
  }
  sort(node_check.begin(), node_check.end());
  for(size_t i = 0; i<node_check.size(); i++){
    if(node_check[i] != i){
      cout<<"Anynet:booksim trafficmanager assumes sequential node numbering starting at 0\n";
      assert(false);
    }
  }
  
}

