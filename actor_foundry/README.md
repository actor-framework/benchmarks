ActorFoundry
=======
Implemented benchmarks:
* actor_creation
* mailbox_performance
* mixed_case

Dependencies
------------
* Ant 1.7 (or later)
* Java Development Kit 6 (JDK 7 and JDK 8 will NOT work!)

Get the sources
---------------
Obtain the sources and untar them

    ~ # cd actor_foundry/
    ~ # wget http://osl.cs.illinois.edu/software/actor-foundry/foundry-local-src-1.0.tar.gz
    ~ # mkdir lib_src
    ~ # tar -zxf foundry-local-src-1.0.tar.gz -C lib_src

Move the files
--------------
    ~ # mkdir lib_src/src/osl/examples/caf_benches
    ~ # cp actor_creation/* lib_src/src/osl/examples/caf_benches
    ~ # cp mailbox_performance/* lib_src/src/osl/examples/caf_benches
    ~ # cp mixed_case/* lib_src/src/osl/examples/caf_benches

Build the sources
-----------------
Make sure your `$JAVA_HOME` is set, for me it is `/usr/lib/jvm/java-6-openjdk-amd64/`

    ~ # cd lib_src
    ~ # ant

_Note: It seems there is something wrong with the lib sources itself, some compiles will fail._

Usage
-----
Assuming you are still in the `lib_src` folder

* `java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osl.examples.caf_benches.actor_creation.testactor boot "_20"` spawns 2^20 actors

* `java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osl.examples.caf_benches.mailbox_performance.mailbox boot "_20_1000000"` spawn 20 sender which will send 1000000 messages each

* `java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osk.examples.caf_benches.mixed_case.mainactor "_1_10_30_1"` will spawn 1 ring, each ring with 10 chains, 30 as initial token and 1 repetition

[ActorFoundry Website](http://osl.cs.illinois.edu/software/actor-foundry/ "ActorFoundry Website")
