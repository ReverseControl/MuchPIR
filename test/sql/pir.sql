--Destroy table
DROP AGGREGATE IF EXISTS pir_select( bytea, bytea, int8,  int8, text) CASCADE ;
DROP AGGREGATE IF EXISTS pir_select( bytea, int8,  int8, text) CASCADE ;
DROP AGGREGATE IF EXISTS pir_select( bytea, int8,  text) CASCADE ;


CREATE OR REPLACE FUNCTION
pir_select_internal( state bytea, query bytea, col text)
RETURNS bytea
AS '$libdir/pir_select', 'pir_select_internal'
LANGUAGE C IMMUTABLE STRICT;;

CREATE OR REPLACE FUNCTION
pir_final( state bytea )
RETURNS bytea
AS '$libdir/pir_select', 'pir_final'
LANGUAGE C IMMUTABLE STRICT;;

CREATE OR REPLACE AGGREGATE pir_select( query bytea, col text)  
(
    sfunc = pir_select_internal,
    stype = bytea,
    FINALFUNC = pir_final,
    INITCOND = "\x00000000000000000000000000000"    --Omitting this value sets initcond to null.
);;

-- Create Table
CREATE TABLE seal_table( name varchar(70), numeric_id int8);;

-- Insert uint64_t numbers into table
INSERT INTO seal_table( name, numeric_id )
VALUES ( 'Minai', 95);;

--INSERT INTO seal_table( name, numeric_id )
--VALUES ( 'Ryzenton', 90);;
--
--INSERT INTO seal_table( name, numeric_id )
--VALUES ( 'kentaimon', 100);;

-- Test Function 
SELECT *
FROM seal_table;

\da

