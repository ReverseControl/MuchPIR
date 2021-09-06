# MuchPir

This is a protype implementation of Private Information Retrieval using Homomorphic Encryption implemented as a C/C++ Aggregate extension to Postgres.

This version is one of my early versions and its performance is severely lacking. Nonetheless, I think it is good a enough implementation for
folks who want to experiment with PIR on Postgres and have a working C/C++ extension.

# Buiding

This code has been run and tested on Ubuntu 18.04 and Ubuntu 20.04. It does require:

1. Microsoft SEAL Library Version 3.5.8: https://github.com/microsoft/SEAL/tree/v3.5.8
2. Postgfres C++ wrapper libpqxx: https://github.com/jtv/libpqxx
3. Postgres Development Packages for C/C++ extensions.

#Dockerfile

Let us take MuchPIR for a test run.

```
git clone https://github.com/ReverseControl/MuchPIR.git
cd MuchPIR
docker build -t muchpir:1.0 .
```

When that is done building. Start a container. This container will be a running server with 
the MuchPIR extension installed and accessible to postgres.


```
docker build -t muchpir:1.0 .
docker run muchpir:1.0
```

This will start the container that runs the postgres server with our extension. 

Now we want to enter the docker container while it is running. In another terminal, 
we do:

```
docker container ls
docker exec -ti <container-name> bash
```

Now we are inside the container that is running the postgres server. By default you will enter in the
right directory to run: 

```
psql -U postgres -d testdb -f ./load_data.sql

```

This will load data into tables that we will use for testing. The data comes from the IMDB dataset you 
find here: https://datasets.imdbws.com/name.basics.tsv.gz. We took the first 69,420 lines, excluding the first 
line which is the header for the columns, and created several tables.

```
TABLE         Size
data_10       1024
data_11       2048
data_12       4096
data_13       8192
data_14      16384
data_15      32768
data_16      65536 
data_all     69420

Signature for the tables
table_N (
       nconst            bigint,
       primaryName       text,
       birthYear         bigint,
       deathYear         bigint,
       primaryProfession text,
       knownForTitles    text
);
```

Postgres extensions are installed per database. Now that we have installed our extension on the system 
(docker container) and Postgres knows where to find it, we can now install it on our database so we 
may use it.

```
psql -U postgres -d testdb -f /git/MuchPIR/pir_select--1.0.sql
```




