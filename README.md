# RVRP-HTW

This project contains the implementation of the branch-and-price algorithm for the robust vehicle routing problem with heterogeneous time windows of my PhD thesis.
It is implementend in C using the MINLP solver SCIP.

## Requirements

To run, this codes needs an installation of SCIP version 9.1.1 or higher. Env_Variable "SCIP_DIR" shall contain the path to the SCIP installation.
See the website for an installation guide:
https://scipopt.org/#scipoptsuite



## Usage

```markdown
$ make
$ ./bin/columnGeneration <path-to-modeldata-file.dat> 
    [-w <appointment window length in sec (optional; default 0)>] 
    [-o <output json file (optional; default used if no name is provided)>]
    [-s <input solution json file (optional)>]
    [-a <objective function parameters (delay, travel time, encounter probability) (default: 1, 0, 0) (mandatory) >]
    [-g <value for Gamma (optional; default 0)>]
    [-v <activate vehicle-assignment branching (optional)>]
    [-output <path to file for printing stats (optional)>]
```

Example usage with modeldata "bayern0_r20_d5_w0.2.dat" in "../test-data/model-data-paper/paper-evaluation-small-tw-0/bayern" 
and an appointment window length of 60m (=3600s) with the creation of an output file:

```markdown
$ make
$ ./bin/columnGeneration ../data/runtime_inst/bayern0_r20_d5_w0.2.dat -w 3600 -a 0.2 0.8 0 -g 7
```


## License

Shield: [![CC BY 4.0][cc-by-shield]][cc-by]

This work is licensed under a
[Creative Commons Attribution 4.0 International License][cc-by].

[![CC BY 4.0][cc-by-image]][cc-by]

[cc-by]: http://creativecommons.org/licenses/by/4.0/
[cc-by-image]: https://i.creativecommons.org/l/by/4.0/88x31.png
[cc-by-shield]: https://img.shields.io/badge/License-CC%20BY%204.0-lightgrey.svg
