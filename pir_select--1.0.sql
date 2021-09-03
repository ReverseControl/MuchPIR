-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pir_select" to load this file. \quit

CREATE OR REPLACE FUNCTION
pir_select_internal( state bytea, query bytea, col text)
RETURNS bytea
AS '$libdir/pir_select', 'pir_select_internal'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION
pir_final( state bytea )
RETURNS bytea
AS '$libdir/pir_select', 'pir_final'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE AGGREGATE pir_select( query bytea, col text)  
(
    sfunc = pir_select_internal,
    stype = bytea,
    FINALFUNC = pir_final,
    initcond = "\x000000000000000000000000000000"   --Omitting initcond sets initcond to null. 
);
