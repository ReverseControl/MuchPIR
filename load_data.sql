--Set password for postgres user. 
--We need this later.
ALTER USER postgres WITH PASSWORD 'pass';

--Create tables for different size tables
CREATE TABLE data_10 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_11 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_12 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_13 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_14 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_15 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_16 (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);
CREATE TABLE data_all (                                                             
        nconst            bigint,
        primaryName       text,
        birthYear         bigint,
        deathYear         bigint,
        primaryProfession text,
        knownForTitles    text
);

--Load the 69420 table from the IMDB dataset sample.
copy data_all from '/data/sample_imdb.tsv';

--Create smaller tables and store them locally
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 1024    ) TO '/tmp/data_10.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 2048    ) TO '/tmp/data_11.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 4196    ) TO '/tmp/data_12.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 8192    ) TO '/tmp/data_13.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 16384   ) TO '/tmp/data_14.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 32768   ) TO '/tmp/data_15.tsv';
COPY (SELECT * FROM data_all ORDER BY nconst ASC LIMIT 65536   ) TO '/tmp/data_16.tsv';

--Load table data from tables create above
copy data_10 from '/tmp/data_10.tsv';
copy data_11 from '/tmp/data_11.tsv';
copy data_12 from '/tmp/data_12.tsv';
copy data_13 from '/tmp/data_13.tsv';
copy data_14 from '/tmp/data_14.tsv';
copy data_15 from '/tmp/data_15.tsv';
copy data_16 from '/tmp/data_16.tsv';
