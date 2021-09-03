extern "C" { // C Headers must be inside exter "C" { } block.
	#include <postgres.h>
	#include <utils/rel.h>
	#include <fmgr.h>
	#include <utils/array.h>
	#include <utils/builtins.h>
	#include <catalog/pg_type.h>
	#include <stdlib.h>
	#include <stdint.h>
	#include <inttypes.h>
	PG_MODULE_MAGIC;
}

// CPP Header must be outside extern "C" { } block.
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator> // For the ostream_iterator

// External projects c++ libraries compiled and linked on running 'make'.
#include "seal/seal.h"
#include "seal/util/polyarithsmallmod.h"

//CONSTANT VALUES
//These values are use to allocate memory to hold state between rows in the search.
//They may need to be increased. If you relinearize every multiply, CIPHER_SIZE_IN_S, can be set to 3.
//If you are only using primes of 60 bits, CRT_SIZE_c, can get 360 bits in cyphertext space for q.
#define POLY_DEGREE               ( 4096ULL )                   
#define DB_SIZE                   ( 4096ULL )
#define CIPHER_SIZE2_BYTES        ( 120000ULL ) 
#define PARAMETER_BYTES           ( 1032ULL )
#define STATE_T_SIZE              (  8 + PARAMETER_BYTES +  ( 8 + CIPHER_SIZE2_BYTES)*(POLY_DEGREE+1)  ) 

typedef struct cipher_t{
    uint64_t size;
    uint8_t data[ CIPHER_SIZE2_BYTES ];
} cipher_t;

typedef struct param_t{
    uint64_t size;
    uint8_t data[ PARAMETER_BYTES ];
} param_t;

typedef struct state_t {
    uint64_t row_idx;
    param_t    param;
    cipher_t   accum; 
    cipher_t dim1[ POLY_DEGREE ];
} state_t ;

using namespace std;
using namespace seal;

//////////////////////
//    Utilities    //
/////////////////////
typedef std::vector<std::vector<seal::Ciphertext>> PirQuery;
typedef std::vector<seal::Ciphertext> PirReply;


string string_to_hex_poly( string data ){  //Len does NOT include NULL terminator
    if( data.size() == 0) return "00";
    string table( "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

    ostringstream stream;
    for( uint64_t jj = 0 ; jj < data.size() - 1  ; jj++ ){
        uint8_t byte = (uint8_t)data[ jj ];
        stream <<  table[ 2*byte ]  <<  table[ 2*byte + 1  ] << "x^" << data.size() - 1 - jj << " + ";
    }

    stream << table[ 2*data[ data.size() - 1 ] ] << table[ 2*data[ data.size() - 1 ] + 1 ]  ;

    string result = stream.str();

    return result;
}


inline void multiply_power_of_X(const Ciphertext &encrypted, Ciphertext &destination, size_t index, seal::EncryptionParameters &params ) { 

    auto encrypted_count = encrypted.size();
    destination = encrypted;

    // Prepare for destination
    // Multiply X^index for each ciphertext polynomial
    for (long unsigned int i = 0; i < encrypted_count; i++) {
            util::negacyclic_shift_poly_coeffmod(encrypted, encrypted_count, index, params.coeff_modulus(), destination);
    }   
}

inline vector<Ciphertext> expand_query(const Ciphertext &encrypted, uint32_t m, GaloisKeys &galkey, seal::EncryptionParameters &params ){

    auto context = SEALContext::Create(params);
    Evaluator evaluator_(context);

    // Assume that m is a power of 2. If not, round it to the next power of 2.
    uint32_t logm = ceil(log2(m));
    Plaintext two("2");

    vector<int> galois_elts;
    auto n = params.poly_modulus_degree();
    if (logm > ceil(log2(n))){
        throw logic_error("m > n is not allowed.");
    }

    for (int i = 0; i < ceil(log2(n)); i++) {
        galois_elts.push_back((n + util::exponentiate_uint(2, i)) / util::exponentiate_uint(2, i));
    }

    vector<Ciphertext> temp;
    temp.push_back(encrypted);
    Ciphertext tempctxt;
    Ciphertext tempctxt_rotated;
    Ciphertext tempctxt_shifted;
    Ciphertext tempctxt_rotatedshifted;

    for (uint32_t i = 0; i < logm - 1; i++) {
        vector<Ciphertext> newtemp(temp.size() << 1);
        // temp[a] = (j0 = a (mod 2**i) ? ) : Enc(x^{j0 - a}) else Enc(0).  With
        // some scaling....
        int index_raw = (n << 1) - (1 << i);
        int index = (index_raw * galois_elts[i]) % (n << 1);

        for (uint32_t a = 0; a < temp.size(); a++) {

            evaluator_.apply_galois(temp[a], galois_elts[i], galkey, tempctxt_rotated);
            evaluator_.add(temp[a], tempctxt_rotated, newtemp[a]);
            multiply_power_of_X(temp[a], tempctxt_shifted, index_raw, params );
            multiply_power_of_X(tempctxt_rotated, tempctxt_rotatedshifted, index, params );
            // Enc(2^i x^j) if j = 0 (mod 2**i).
            evaluator_.add(tempctxt_shifted, tempctxt_rotatedshifted, newtemp[a + temp.size()]);
        }
        temp = newtemp;
    }

    // Last step of the loop
    vector<Ciphertext> newtemp(temp.size() << 1);
    int index_raw = (n << 1) - (1 << (logm - 1));
    int index = (index_raw * galois_elts[logm - 1]) % (n << 1);
    for (uint32_t a = 0; a < temp.size(); a++) {
        if (a >= (m - (1 << (logm - 1)))) {                       // corner case.
            evaluator_.multiply_plain(temp[a], two, newtemp[a] ); // plain multiplication by 2.
        } else {
            evaluator_.apply_galois(temp[a], galois_elts[logm - 1], galkey, tempctxt_rotated);
            evaluator_.add(temp[a], tempctxt_rotated, newtemp[a]);
            multiply_power_of_X(temp[a], tempctxt_shifted, index_raw, params  );
            multiply_power_of_X(tempctxt_rotated, tempctxt_rotatedshifted, index, params );
            evaluator_.add(tempctxt_shifted, tempctxt_rotatedshifted, newtemp[a + temp.size()]);
        }
    }

    vector<Ciphertext>::const_iterator first = newtemp.begin();
    vector<Ciphertext>::const_iterator last = newtemp.begin() + m;
    vector<Ciphertext> newVec(first, last);
    return newVec;
}

GaloisKeys generate_galois_keys( const int N, KeyGenerator *keygen ) { 
    vector< uint32_t > galois_elts;
    const int logN = seal::util::get_power_of_two(N);

    for (int i = 0; i < logN; i++){ 
        galois_elts.push_back(  (N + seal::util::exponentiate_uint(2, i)) / seal::util::exponentiate_uint(2, i) ) ; 
    }   

    return keygen->galois_keys_local( galois_elts );
}

extern "C" { // PostgreSQL functions be inside extern "C" { } block.
	PG_FUNCTION_INFO_V1( pir_select_internal );

	Datum pir_select_internal(PG_FUNCTION_ARGS){
	    //Get the Postgres C-Extension Parameters
        bytea     *state    = PG_GETARG_BYTEA_P(0);
	    text  *inputText    = PG_GETARG_TEXT_PP(2);
       
        //Pointers to access argument data 
        state_t *arg0 = (state_t *) VARDATA( state );

        //First time call, set up state
        if(   arg0->row_idx == 0 ){
            //Get the query
            bytea    *query     = PG_GETARG_BYTEA_P(1);
            
            //Pointers to access argument data 
            char  *arg1 = (char *) VARDATA_ANY(query);

            ////Load byte data into a stream
            string serialized( arg1, VARSIZE_ANY_EXHDR(query)  );
            stringstream stream;
            stream << serialized;

            ////The encryption parameters are unloaded first
  	        seal::EncryptionParameters params;
            params.load(stream);

            ////Create context to unload the remaining values in the stream
            auto context = SEALContext::Create(params);

            ////Unload Galois Keys
            GaloisKeys gl_keys ;
            gl_keys.load( context, stream );

            ////Unload ciphertext
            Ciphertext dimension1;
            dimension1.load( context, stream  );

            //Expand hypercube
            vector<Ciphertext> hcube_dim1 = expand_query( dimension1, DB_SIZE, gl_keys, params );
       
            ////Make object to store state from now on.
            bytea *new_state   = (bytea *) palloc( STATE_T_SIZE + VARHDRSZ );
            SET_VARSIZE( new_state,                STATE_T_SIZE + VARHDRSZ );

            //Map new state object to local struct having the structure we will use to store data.
	        state_t *nstate  = (state_t *) VARDATA(new_state);
            
            //Save the parameter into the state of the aggregation
            std::ostringstream param_buffer;
            params.save( param_buffer  );
            string param_str = param_buffer.str();
            nstate->param.size = param_str.size();

            memcpy( (void *) nstate->param.data , (void *)param_str.data(), param_str.size() ); 
            
            ////Save state to memory
            for (uint32_t jj = 0; jj < hcube_dim1.size(); jj++){
                //Create stream to store Ciphertext Object
                std::ostringstream buffer;
            
                //Save Ciphertext to stream as raw bytes
                hcube_dim1[jj].save(  buffer  );

                //Get the length of the stream in bytes
                std::string data_string = buffer.str();
            
                //Store data into memory
                nstate->dim1[jj].size = data_string.size();
                memcpy( (void *)  nstate->dim1[jj].data, (void *)data_string.data(), data_string.size() ); 
            }

            //Encode the current (first) row data into a polynomial string
            string row_data( VARDATA_ANY(inputText), VARSIZE_ANY_EXHDR( inputText ) );
            Plaintext BFV_row_data( string_to_hex_poly( row_data )  ); 

            //Multiply encrypted index by plain data
            Evaluator evaluator(context);
            Ciphertext mul_result;
            evaluator.multiply_plain( hcube_dim1[ 0 ], BFV_row_data, mul_result ) ;  
            
            //Testing return
            std::stringstream output;
            mul_result.save( output  );
            string rts     = output.str();
            nstate->accum.size = rts.size();
            memcpy( (void *)  nstate->accum.data, (void *)rts.data(), rts.size() );
            
            //Update the row index
            nstate->row_idx = 1ULL;
      
            ////Return new state
            PG_RETURN_BYTEA_P( new_state );

        }else{

            //Map new state object to local struct having the structure we will use to store data.
	        state_t *nstate  = (state_t *) VARDATA_ANY( state );

            //If database is bigger than supported database size, do not proccess current row.
            if( nstate->row_idx > DB_SIZE  ){
                PG_RETURN_BYTEA_P( state );
            }

            ////Load parameters
            string s1( (char *) nstate->param.data, nstate->param.size );
            stringstream stream1;
            stream1 << s1;
  	        seal::EncryptionParameters params;
            params.load( stream1 );
    
            //Set context from this parameter
            auto context = SEALContext::Create(params);

            //Load hcube dim1 ciphertext
            string s2( (char *) nstate->dim1[ nstate->row_idx  ].data, nstate->dim1[ nstate->row_idx  ].size   );
            stringstream stream2;
            stream2 << s2;
            Ciphertext hcube_1;
            hcube_1.load( context, stream2 );

            //Load accumulation ciphertext
            string s3( (char *) nstate->accum.data , nstate->accum.size   );
            stringstream stream3;
            stream3 << s3;
            Ciphertext accum;
            accum.load( context, stream3 );
            
            //Encode the current (first) row data into a polynomial string
            string row_data( VARDATA_ANY(inputText), VARSIZE_ANY_EXHDR( inputText ) );
            Plaintext BFV_row_data( string_to_hex_poly( row_data )  ); 

            ////Multiply encrypted index by plain data
            Evaluator evaluator(context);
            Ciphertext result;
            evaluator.multiply_plain( hcube_1, BFV_row_data, result ) ;  
            evaluator.add_inplace( result, accum ) ;  
            
            ////Testing return
            std::stringstream output;
            result.save( output  );
            string rts         = output.str();
            nstate->accum.size = rts.size();
            memcpy( (void *)  nstate->accum.data, (void *)rts.data(), rts.size() );
      
            ////Update the row index
            nstate->row_idx++;

            PG_RETURN_BYTEA_P( state );
        }
	};

	PG_FUNCTION_INFO_V1( pir_final );
	Datum pir_final(PG_FUNCTION_ARGS){
        //Function to extract the accumulated value from the state
        //and return that as the answer
        bytea   *state  = PG_GETARG_BYTEA_P(0);
	    state_t *cstate = (state_t *) VARDATA_ANY(state);

        bytea *ret = (bytea *) palloc( cstate->accum.size + VARHDRSZ  );
        SET_VARSIZE( ret,              cstate->accum.size + VARHDRSZ );    
        memcpy( (void *)  VARDATA_ANY(ret), (void *) cstate->accum.data ,  cstate->accum.size );

        PG_RETURN_BYTEA_P( ret );
    };
}
