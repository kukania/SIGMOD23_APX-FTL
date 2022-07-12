# APX-FTL

This repository is an artifact for SIGMOD23 submission.

These codes include several FTL schems (Sector, DFTL, SFTL, TPFTL, and APX-FTL).\
All FTLs' code is in the "algorithm" directory.\
In the algorithm directory, there are 3 directories Sector, DFTL and APXFTL.\
The DFTL, SFTL, and TPFTL are based on the demand-based FTL.\
Thus, we implemented each specific code of 3 FTLs in the path "./algorithm/DFTL".\
The specific codes for them are in ./algorithm/DFTL/caching/ directory.\
\
DFTL uses ./algorithm/DFTL/caching/coarse for its cache.\
SFTL uses ./algorithm/DFTL/caching/sftl for its cache.\
TPFTL uses ./algorithm/DFTL/caching/tpftl for its cache.\

# Compile
We can change the target FTL to compile by editing the "TARGET_ALGO" in Makefile.
The default "TARGET_ALGO" is APXFTL which is our suggestion. 
To make other FTLs, you change the TARGET_ALGO="Sector or DFTL or APXFTL".

And then, you can compile FTL by 
```
make driver -j
```
The "driver" is the simple testing tool for implemented FTLs. 
It consists of two phases, random write and random read.

You can test each FTLs by running driver.

# Reproduce

We use the Xilinx Virtex UltraScale FPGA VCU108 platform and customized NAND flash modules. 
The customized NAND flash modules used in this paper are not publicly or commercially available. 
Therefore, you may need your own NAND modules compatible with VCU108 and adequate modifications to the hardware backend to replicate this work.

However, we implemented an SSD emulation version that regards DRAM area to storage.
The default setup is the SSD emulation version.


