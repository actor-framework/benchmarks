#!/bin/bash
# download lib source and untar
wget http://osl.cs.illinois.edu/software/actor-foundry/foundry-local-src-1.0.tar.gz
mkdir lib_src
tar -zxf foundry-local-src-1.0.tar.gz -C lib_src

# move files
mkdir lib_src/src/osl/examples/caf_benches
cp -r actor_creation/* lib_src/src/osl/examples/caf_benches
cp -r mailbox_performance/* lib_src/src/osl/examples/caf_benches
cp -r mixed_case/* lib_src/src/osl/examples/caf_benches

# build sources
cd lib_src
ant
