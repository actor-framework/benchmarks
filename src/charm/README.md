Charm++
=======
(Charm++)[http://charm.cs.uiuc.edu]

Implemented programms for libcaf benchmarks:
* actor_creation
* mailbox_performance
* mixed_case 

Get the sources
---------------
First of all get charm++ source and build it:

    ~ # git clone http://charm.cs.uiuc.edu/gerrit/charm
    ~ # cd charm
    ~ # ./build

Next step is to copy charm++ source files to subdirectories of charm++.

    ../benchmarks # mkdir ~/charm/source/
    ../benchmarks # cp charm++ ~/charm/source
    ../benchmarks # cd ~/charm/source/
   
Here are the abovementioned implementations. To build them simply 
enter directory and type `make`. With `./command` you can start them.

Usage
-----
* `./creation n` spawns 2^n actors
* `./mailbox c m` spawns c chares which are spaming m messages

