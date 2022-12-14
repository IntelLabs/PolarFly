# $Id $

# Copyright (c) 2007-2012, Trustees of The Leland Stanford Junior University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.





# CPLEX_DIR=/home/maciej/cplex/cplex
# CONCERT_DIR=/home/maciej/cplex/concert
# CPLEX_INC=-I${CPLEX_DIR}/include -I${CONCERT_DIR}/include
# CPLEX_LIB=-L${CPLEX_DIR}/lib/x86-64_linux/static_pic -L${CONCERT_DIR}/lib/x86-64_linux/static_pic -lilocplex -lconcert -lcplex -lm -lpthread
# Hack to avoid cplex
#CPLEX_DIR=""
#CPLEX_INC=""


CPPFLAGS_FULL=-O3 -std=c++11 -g -Wall -Wextra -fopenmp -DIL_STD









#
# Makefile
#
MOC = /usr/lib/qt4/bin/moc
YACC   = bison -y
DEFINE = 
DEFINE_TEST = -DUSE_GUI
INCPATH = -I. -Iarbiters -Iallocators -Irouters -Inetworks -Ipower
CPPFLAGS += -Wall $(INCPATH) $(DEFINE)
#CPPFLAGS += -O3
CPPFLAGS += -g
LFLAGS +=  


OBJDIR := obj
PROG   := booksim

# simulator source files
CPP_SRCS = main.cpp \
   config_utils.cpp \
   booksim_config.cpp \
   module.cpp \
   buffer.cpp \
   vc.cpp \
   routefunc.cpp \
   traffic.cpp \
   flitchannel.cpp \
   trafficmanager.cpp \
   batchtrafficmanager.cpp \
   packet_reply_info.cpp \
   buffer_state.cpp \
   stats.cpp \
   credit.cpp \
   outputset.cpp \
   flit.cpp \
   injection.cpp\
   misc_utils.cpp\
   rng_wrapper.cpp\
   rng_double_wrapper.cpp\
   power_module.cpp \
   switch_monitor.cpp \
   buffer_monitor.cpp

ifeq ($(DEFINE) , $(DEFINE_TEST))
	CPP_SRCS += \
	bgui.cpp\
   moc_bgui.cpp
	LFLAGS +=  -lQtGui -lQtCore -lpthread 
	INCPATH += -I/usr/include/QtCore -I/usr/include/QtGui
else 
endif

LEX_OBJS  = ${OBJDIR}/lex.yy.o
YACC_OBJS = ${OBJDIR}/y.tab.o

# networks 
NETWORKS:= $(wildcard networks/*.cpp) 
ALLOCATORS:= $(wildcard allocators/*.cpp)
ARBITERS:= $(wildcard arbiters/*.cpp)
ROUTERS:= $(wildcard routers/*.cpp)
POWER:= $(wildcard power/*.cpp)

#--- Make rules ---
OBJS := $(LEX_OBJS) $(YACC_OBJS)\
 $(CPP_SRCS:%.cpp=${OBJDIR}/%.o)\
 $(NETWORKS:networks/%.cpp=${OBJDIR}/%.o)\
 $(ALLOCATORS:allocators/%.cpp=${OBJDIR}/%.o)\
 $(ARBITERS:arbiters/%.cpp=${OBJDIR}/%.o)\
 $(ROUTERS:routers/%.cpp=${OBJDIR}/%.o)\
 $(POWER:power/%.cpp=${OBJDIR}/%.o)

.PHONY: clean

all:$(PROG)

$(PROG): $(OBJS)
	 $(CXX) $(LFLAGS) $(CPPFLAGS_FULL) $^ -o $@ -lstdc++ 

# rules to compile simulator

moc_bgui.cpp: booksim_config.hpp \
		config_utils.hpp \
		bgui.hpp
	$(MOC) $(INCPATH) bgui.hpp -o moc_bgui.cpp

${LEX_OBJS}: lex.yy.c y.tab.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) -c $< -o $@

${YACC_OBJS}: y.tab.c y.tab.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) -c $< -o $@

${OBJDIR}/%.o: %.cpp 
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -c $< -o $@

# rules to compile networks
${OBJDIR}/%.o: networks/%.cpp 
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) $(CPPFLAGS_FULL) -c $< -o $@

# rules to compile arbiters
${OBJDIR}/%.o: arbiters/%.cpp 
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -c $< -o $@

# rules to compile allocators
${OBJDIR}/%.o: allocators/%.cpp 
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -c $< -o $@

# rules to compile routers
${OBJDIR}/%.o: routers/%.cpp 
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -c $< -o $@

# rules to compile power classes
${OBJDIR}/%.o: power/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) 
	rm -f $(PROG)
	rm -f *~
	rm -f allocators/*~
	rm -f arbiters/*~
	rm -f networks/*~
	rm -f runfiles/*~
	rm -f routers/*~
	rm -f examples/*~
	rm -f y.tab.c y.tab.h lex.yy.c
	rm -f moc_bgui.cpp

y.tab.c y.tab.h: config.y
	$(YACC) -d $<

lex.yy.c: config.l
	$(LEX) $<
