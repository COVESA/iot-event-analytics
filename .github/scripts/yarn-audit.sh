#!/bin/bash
yarn audit --groups dependencies --level critical;
yarncode=$?;
if [ "$yarncode" -lt 16 ]; then 
  exit 0; 
else 
  exit $yarncode; 
fi
