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
docker run -e POSTGRES_PASSWORD=pass -e POSTGRES_USER=postgres  muchpir:1.0
```

This will start the container that runs the postgres server with our extension. Now we want to enter the docker container while it is running. In another terminal, we do:

```
docker container ls
docker exec -u 0 -ti <container-name> bash
```

By default you will enter in the right directory to run: 

```
cd /data/
su - postgres
psql -U postgres -d testdb -f ./load_data.sql
exit
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
cd /data/
su - postgres
psql -U postgres -d testdb -f /git/MuchPIR/pir_select--1.0.sql
exit
```

Now that we have the database running with the extension installed on a databse with tables and data
ready to go, let's build the client for testing.

```
cd /git/MuchPIR/client
g++  -std=c++17 -march=native -I/usr/local/include/SEAL-3.5/ -L/usr/local/lib/ client_module.cpp -lpqxx -lpq -l:libseal.so.3.5
time ./a.out
```

You should get an output like this:

```
root@565c02be961c:/git/MuchPIR/client# g++  -std=c++17 -march=native -I/usr/local/include/SEAL-3.5/ -L/usr/local/lib/ client_module.cpp -lpqxx -lpq -l:libseal.so.3.5
root@565c02be961c:/git/MuchPIR/client# time ./a.out 
Plain Modulus: 40961

   db_size: 1024 <-- Note: By default 1024 in this demo. Can be increased up to 10s of millions: requires math and code, i.e the latest version. 
 row_index: 0 <-- Note: You can change this to any row you want in code.
 hcube_dim: 1 <-- Note: Hypercube dimension. This demo supports only 1 dimension. Limits DB_size to 4096 rows.
hecube_len: 1024 <-- Note: An optimization. Does not affect this demo. Improves performance for high dimensional hyper cubes.

Polynomial Degree(N):4096
Parameters size: 129
query size in bytes: 131177

  Result Size: 182434 bytes.
  Query  Size: 1790427bytes.
Query result (Polynomial): 46x^11 + 72x^10 + 65x^9 + 64x^8 + 20x^7 + 41x^6 + 73x^5 + 74x^4 + 61x^3 + 69x^2 + 72x^1 + 65
Query result (ASCII)     : Fred Astaire

real	0m58.264s
user	0m0.230s
sys	0m0.009s
```



