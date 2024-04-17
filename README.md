## Introduction

In this repo you will find the topologies and modules developed for the thesis : "Diseño y Desarrollo de una Arquitectura de Monitorización para una Red de Telecomunicaciones Óptico-Móvil"

- topology_1_3.cc: Topology developed to acomplish goals 1 & 3 in the process of learning ns3-lena
- flow-monitor: Implementation of FlowMonitor, it respects the original architecture by only expanding the necessary classes.
- simple-global-routing.cc: Prototype of use of extended Flow-monitor module

## USE

To use this repo's code, you'll need to install [ns3](https://www.nsnam.org/) and [nr](https://gitlab.com/cttc-lena/nr) first. It will be assumed that you followed nr's tutorial.

Now assuming you're located in your ns3-dev directory. You'll need to execute the following to install this repo code.

```
cd contrib/nr
git clone git@github.com:lea-alfonso/ns3_topologies.git
cp -r -u contrib/nr/examples/* contrib/nr/ns3_topologies
mv contrib/nr/ns3_topologies contrib/nr/examples

cp -r --update=all contrib/nr/examples/flow-monitor src/

./ns3 build
```
