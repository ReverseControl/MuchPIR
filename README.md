# MuchPIR

This is a protype implementation of Private Information Retrieval using Homomorphic Encryption implemented as a C/C++ Aggregate extension to Postgres.

This version is one of my early versions and its performance is severely lacking. Nonetheless, I think it is good a enough implementation for
folks who want to experiment with PIR on Postgres and have a working C/C++ extension.


I will soon be adding building documentation and a server docker file.

# Buiding

This code has been run and tested on Ubuntu 18.04 and Ubuntu 20.04. It does require:

1. Microsoft SEAL Library Version 3.5.8: https://github.com/microsoft/SEAL/tree/v3.5.8
2. Postgres C++ wrapper libpqxx: https://github.com/jtv/libpqxx
3. Postgres Development Packages for C/C++ extensions.
