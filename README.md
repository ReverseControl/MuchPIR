# MuchPir

This is a protype implementation of Private Information Retrieval using Homomorphic Encryption implemented as a C/C++ Aggregate extension to Postgres.

This version is an early versions and its performance is severely lacking. Nonetheless, it is good a enough implementation for
folks who want to experiment with PIR on Postgres and have a working C/C++ extension.


# Buiding

This code has been run and tested on Ubuntu 18.04 and Ubuntu 20.04. It does require:

1. Microsoft SEAL Library Version 3.5.8: https://github.com/microsoft/SEAL/tree/v3.5.8
2. Postgfres C++ wrapper libpqxx: https://github.com/jtv/libpqxx
3. Postgres Development Packages for C/C++ extensions.

# Demo

Let us take MuchPIR for a test run.

```
git clone https://github.com/ReverseControl/MuchPIR.git
cd MuchPIR
docker build -t muchpir:1.0 .
docker run muchpir:1.0
```

This will start the container that runs the postgres server with our extension. Now we want to enter the docker container while it is running. In another terminal, we do:

```
docker container ls
docker exec -u 0 -ti <container-name> bash
```

By default you will enter in the right directory to run: 

```
su - postgres
psql -U postgres -d testdb -f ./load_data.sql
```

This will load data into tables that we will use for testing. The data comes from the IMDB dataset you 
find here: https://datasets.imdbws.com/name.basics.tsv.gz. We took the first 69,420 lines, excluding the first 
line which is the header for the columns, and created several tables.

```
TABLE         Size   |    Signature for the tables
data_10       1024   |    table_N (
data_11       2048   |           nconst            bigint,
data_12       4096   |           primaryName       text,
data_13       8192   |           birthYear         bigint,
data_14      16384   |           deathYear         bigint,
data_15      32768   |           primaryProfession text,
data_16      65536   |           knownForTitles    text  
data_all     69420   |    ); 
```

Postgres extensions are installed per database. Now that we have installed our extension on the system 
(docker container) and Postgres knows where to find it, we can now install it on our database so we 
may use it.

```
psql -U postgres -d testdb -f /git/MuchPIR/pir_select--1.0.sql
```

Now that we have the database running with the extension installed on a databse with tables and data
ready to go, let's build the client for testing.

```
cd /git/MuchPIR/client
g++  -std=c++17 -march=native -I/usr/local/include/SEAL-3.5/ -L/usr/local/lib/ client_module.cpp -lpqxx -lpq -l:libseal.so.3.5
```

