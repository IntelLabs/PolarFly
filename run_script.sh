#!/bin/bash

TAG=${USER}/sc

BASEDIR=$(pwd)

docker build \
  --build-arg USER_ID=$(id -u ${USER}) \
  --build-arg GROUP_ID=10022 \
  --build-arg TAG=$TAG \
  . \
  -f Dockerfile.polarfly \
  -t $TAG $@
  
PROJECT_FILES=${BASEDIR}/polarfly

mkdir -p ${PROJECT_FILES}

docker run \
  -v ${PROJECT_FILES}:/home/intel \
  -w /home/intel \
  -it --rm -u intel $TAG:latest

