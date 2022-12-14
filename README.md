# NTT-data-generator

Distribution shift experiments with ns-3 for data generation to train and evaluate the Network Traffic Transformer (NTT).

## Overview

- `ntt-generator/` An ns-3 module containing simulation helpers to generate traffic with specific CDF workloads.
- `simulation/` Simulation scripts.

## Running the simulations

### Docker with Visual Studio Code

When opening the directory, VSCode will prompt you to open the folder in a
container. Inside the dev container, you can run scripts from the local directory or use the `waf` command.
Before you run the simulations for the first time, you need to configure ns-3:

    $ waf configure

For the small topology:

    $ ./run_topo_small.sh <args>

For the larger topology:

    $ ./run_topo.sh <args>

Check all available parameters:

    $ waf --run "trafficgen --PrintHelp"

Note: I have not tested this VSCode environment's settings or workflow myself for the NTT dataset generation, so I would advise the second method below.

### Native Docker (Preferred method)

Use the `./docker-run.sh` script to run the simulations in a ns-3 environment.
The first time you run the script, it will take some time to download and start
the container. Afterwards, the container will be re-used.
The project directory will be mapped to the container.
Before you run the simulations for the first time, you need to configure ns-3:

    $ ./docker-run.sh waf configure

Afterwards, you can run everything to generate the data for the NTT, with the following self contained scripts:

For the small topology, with one or two bottlenecks:

    $ ./run_topo_small.sh <args>

For the larger topology, with multi-path and four bottlenecks:

    $ ./run_topo.sh <args>

Please refer to the comments in the run scripts ([run_topo](run_topo.sh) and [run_topo_small](run_topo_small.sh)) in order to generate variations and multiple runs of pre-training and fine-tuning data.
To generate multiple runs of either kind of data, you need to pass the `seed` argument, which varies the start-times of the flows, using a uniform distribution and thus causes differences in flow interactions, leading to different network dynamics.

You can check all available parameters:

    $ ./docker-run.sh waf --run "trafficgen --PrintHelp"

If you need to clean up and remove the container, use `docker rm --force ntt-generator-runner`.

The outputs of the simulation will always be in a folder called `results`, and this folder can be moved to the relevant directories for the NTT training.
