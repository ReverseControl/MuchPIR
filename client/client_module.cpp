#include <inttypes.h>
#include <math.h>
#include <seal/seal.h>
#include <iostream>
#include <pqxx/pqxx>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include "seal/util/polyarithsmallmod.h"


using namespace std;
using namespace seal;
using namespace seal::util;
#define DB_SIZE (4096)

/*! 
 * \brief Embed string into a polynomial one character per polynomial coefficient.
 * @param[in]    encrypted Encrypted data in a ring polynomial.
 * @param[in]        index Power of the monomial the encrypted polynomial is multiplied by.
 * @param[out]      params Parameters of the ring polynomial where the operations take place.
 * @param[out] destination Store final result here.
 */
inline void multiply_power_of_x(const Ciphertext &encrypted, Ciphertext &destination, size_t index, seal::EncryptionParameters &params ) {

    auto coeff_mod_count = params.coeff_modulus().size();
    size_t coeff_count   = params.poly_modulus_degree();
    auto encrypted_count = encrypted.size();

    // First copy over.
    destination = encrypted;

    // Prepare for destination
    // Multiply X^index for each ciphertext polynomial
    for (long unsigned int i = 0; i < encrypted_count; i++) {
            util::negacyclic_shift_poly_coeffmod(encrypted, encrypted_count, index, params.coeff_modulus(), destination);
    }
}

/*! 
 * \brief Hypercube expansion along one dimension. 
 * @param[in]    encrypted Encrypted data in a ring polynomial.
 * @param[in]            m The number of coefficients that will be extracted as constant terms from first argument (encrypted).
 * @param[in]       galkey Galoi Keys needed to perform hypercube expansion.
 * @param[in]       params Parameters of the ring polynomial where the operations take place.
 * @param[in]    decryptor SEAL object needed to decrypt and measure noise.
 * @param[out]      return Vector containing polynomials whose constant term is the coeeficient of the n-th term of the first argument (encrypted).
 */
inline vector<Ciphertext> expand_query(const Ciphertext &encrypted, uint32_t m, GaloisKeys &galkey, seal::EncryptionParameters &params, Decryptor &decryptor, KeyGenerator &keygen ){

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

    cout << "logm: " << logm << "." << endl;
    auto relin_keys = keygen.relin_keys_local();

    for (uint32_t i = 0; i < logm - 1; i++) {
        vector<Ciphertext> newtemp(temp.size() << 1);
        int index_raw = (n << 1) - (1 << i);
        int index = (index_raw * galois_elts[i]) % (n << 1);

        for (uint32_t a = 0; a < temp.size(); a++) {

            evaluator_.apply_galois(temp[a], galois_elts[i], galkey, tempctxt_rotated);
            evaluator_.add(temp[a], tempctxt_rotated, newtemp[a]);
            multiply_power_of_x(temp[a], tempctxt_shifted, index_raw, params );
            multiply_power_of_x(tempctxt_rotated, tempctxt_rotatedshifted, index, params );
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
            multiply_power_of_x(temp[a], tempctxt_shifted, index_raw, params  );
            multiply_power_of_x(tempctxt_rotated, tempctxt_rotatedshifted, index, params );
            evaluator_.add(tempctxt_shifted, tempctxt_rotatedshifted, newtemp[a + temp.size()]);
        }
    }

    vector<Ciphertext>::const_iterator first = newtemp.begin();
    vector<Ciphertext>::const_iterator last = newtemp.begin() + m;
    vector<Ciphertext> newVec(first, last);
    return newVec;
}

/*!
 * \brief Embed string into a polynomial one byte per polynomial coefficient.
 * @param[in] data String containing raw data.
 * @param[out]     SEAL string hex polynomial encoding the data. 
 */
string char_to_poly( string data ){ 
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

/*! 
 * \brief Embed string into a polynomial one byte per polynomial coefficient.
 * @param[in] data Array of chars containing raw data.
 * @param[out]     SEAL string hex polynomial encoding the data. 
 */
string char_to_poly( char *data ){
    uint64_t len       = strlen(data);       
    char *data_padded  = (char *) malloc( len - (len%8) + 8 );
    uint64_t *data_int = (uint64_t *) data_padded;   
    uint64_t  len_int  = (len+7)/8;

    memset(  data_padded,                 0, len - (len%8) + 8 );
    memcpy( &data_padded[ 8 - len%8 ], data, len - (len%8) + 8 );

    string hex_str =  seal::util::uint_to_hex_string( data_int, len_int );
    free(data_padded);

    ostringstream stream;
    for( uint64_t jj = 0 ; jj < len_int; jj++ ){
        stream << hex_str[ (len_int - jj)*16 - 2  ] <<  hex_str[ (len_int - jj)*16 - 1  ] << "x^" << (len_int - jj)*8 - 1 << " + "   
               << hex_str[ (len_int - jj)*16 - 4  ] <<  hex_str[ (len_int - jj)*16 - 3  ] << "x^" << (len_int - jj)*8 - 2 << " + "
               << hex_str[ (len_int - jj)*16 - 6  ] <<  hex_str[ (len_int - jj)*16 - 5  ] << "x^" << (len_int - jj)*8 - 3 << " + "
               << hex_str[ (len_int - jj)*16 - 8  ] <<  hex_str[ (len_int - jj)*16 - 7  ] << "x^" << (len_int - jj)*8 - 4 << " + "
               << hex_str[ (len_int - jj)*16 - 10 ] <<  hex_str[ (len_int - jj)*16 - 9  ] << "x^" << (len_int - jj)*8 - 5 << " + "
               << hex_str[ (len_int - jj)*16 - 12 ] <<  hex_str[ (len_int - jj)*16 - 11 ] << "x^" << (len_int - jj)*8 - 6 << " + "
               << hex_str[ (len_int - jj)*16 - 14 ] <<  hex_str[ (len_int - jj)*16 - 13 ] << "x^" << (len_int - jj)*8 - 7 << " + ";
    
        if( jj == (len_int - 1)) 
            stream << hex_str[ (len_int - jj)*16 - 16 ] <<  hex_str[ (len_int - jj)*16 - 15 ];
        else
            stream << hex_str[ (len_int - jj)*16 - 16 ] <<  hex_str[ (len_int - jj)*16 - 15 ] << "x^" << (len_int - jj)*8 - 8 << " + ";
    }   

    string result = stream.str();

    return result;
}


/*! 
 * \brief Determine the smallest hypercube that will fit the chosen supported database size.
 * @param[out]   hcube_dim Number of dimensions needed to encode db_size. 
 * @param[out]   hcube_len Lenght of the hypercube dimensions.
 * @param[in]  poly_degree Degree of the polynomial ring quotient polynomial. 
 * @param[in]      db_size Size of the database.
 */
void get_hcube_param( uint64_t *hcube_dim, uint64_t *hcube_len, uint64_t poly_degree, uint64_t db_size){
    //Variables
    *hcube_dim = 0;
    *hcube_len = -1ULL;

    //Determine the smallest hypercube to encode row index given the database size.  
    //Note the degree is hardcoded to 1 for now.
    for( ; *hcube_len > poly_degree ; (*hcube_dim)++)  (*hcube_len) = (uint64_t) ceil(  pow( (double) db_size, ( (double)  1.00 )/( (double) (*hcube_dim) + 1.00) ) ); 
}



/*! 
 * \brief Generate encrypted query; i.e. embed index into a hypercube.
 * @param[in]       row_index Row number of the item to be retrieved from the database.  
 * @param[in]         db_size Size of the table we are querying. Should be equal to or greater than the table queried.
 * @param[in] poly_mod_degree Polynomial Ring Quotient degree; e.g. the N in the Polynomial Ring F[x]/(X^N+1). 
 * @param[in]       encryptor SEAL object needed to encrypt our hypercube embedding, aka query.
 * @param[out]         return Vector of ciphertexts corresponding to the hypercube embedding of our query; one vector of ciphertexts for each hypercube dimension.
 */
vector< vector<Ciphertext> >  generate_query( uint64_t row_index, const uint64_t db_size, uint64_t poly_mod_degree , Encryptor &encryptor ) {
    //Variables
    uint64_t hcube_dim;
    uint64_t hcube_len;

    //Determine the smallest hypercube to encode row index given the database size.  
    get_hcube_param( &hcube_dim, &hcube_len, poly_mod_degree, db_size);

    //Crypto objects to encode row_index under cipher
    Plaintext pt( poly_mod_degree );
    vector< vector<Ciphertext> >  result( hcube_dim );

    cout << endl << "db_size: " << db_size << endl << "row_index: " << row_index << endl << "hcube_dim: " << hcube_dim << endl << "hecube_len: " << hcube_len << endl;
    cout << "N=" << poly_mod_degree << endl; ;
    
    for ( ;  hcube_dim > 0 ;  ) {

        //Set plaintext to all zeroes
        pt.set_zero();

        //Get the digits based hcube_len to encodew row_index into hyper-cube
        uint64_t sub_index = row_index % hcube_len;

        //Encode dimension into plaintext
        pt[ sub_index ] = 1;

        //Update the next index position
        row_index = row_index/hcube_len;

        //Encrypt current plaintext encoding one dimension of the hypercube
        Ciphertext dest;
        encryptor.encrypt(pt, dest);

        //Store encrypted index into vector to store dimension
        hcube_dim--;
        result[ hcube_dim ].push_back(dest);
    }   
    
    return result;
}

/*! 
 * \brief Generate Galois keys needed to perform the hypercube expansion. 
 * @param[in]         N Encrypted data in a ring polynomial.
 * @param[in]    keygen SEAL object needed to generate the keys, key factory.
 * @param[out]   return All the galois keys needed to perform hypercube expansion, about log2(N) keys.
 */
Serializable<GaloisKeys>  generate_galois_keys( const int N, KeyGenerator *keygen ) { 
    vector< uint32_t > galois_elts;
    const int logN = seal::util::get_power_of_two(N);

    for (int i = 0; i < logN; i++){ 
        galois_elts.push_back(  (N + seal::util::exponentiate_uint(2, i)) / seal::util::exponentiate_uint(2, i) ) ;
    }

    return keygen->galois_keys( galois_elts );
}

#define POLY_DEGREE               ( 4096ULL )                   
#define CIPHER_SIZE2_BYTES        ( 131177ULL ) 
#define PARAMETER_BYTES           ( 129ULL )
#define STATE_T_SIZE              (  8 + PARAMETER_BYTES +  CIPHER_SIZE2_BYTES*(POLY_DEGREE+1) ) 

typedef struct cipher_t{
    uint8_t data[ CIPHER_SIZE2_BYTES ];
} cipher_t;

typedef struct param_t{
    uint8_t data[ PARAMETER_BYTES ];
} param_t;

typedef struct state_t {
    uint64_t row_idx;
    param_t    param;
    cipher_t   accum;
    cipher_t dim1[ POLY_DEGREE ];
} state_t ;


int main(int narg, char *argv[])
{
    //Set up parameters
    seal::EncryptionParameters params( seal::scheme_type::BFV );
    size_t poly_modulus_degree = 4096;
    params.set_poly_modulus_degree(poly_modulus_degree);
    params.set_coeff_modulus(seal::CoeffModulus::BFVDefault(poly_modulus_degree));
    params.set_plain_modulus( 40961 );
    auto context = SEALContext::Create(params);
    Evaluator evaluator(context);

    cout << "Plain Modulus: " << 40961 ;

    //Create public/private keys
    KeyGenerator keygen(context);
    PublicKey public_key = keygen.public_key();
    SecretKey secret_key = keygen.secret_key();

    //Create objects to compute on data
    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, secret_key);

    //Create Galois keys
    Serializable<GaloisKeys> GalKeys = generate_galois_keys( poly_modulus_degree, &keygen ); 

    //Initialize random seed.
    srand (time(NULL));

    //Generate random row number
    uint64_t row_index = 0;  //rand() % 1000;

    //Prepare pir query 
    vector< vector<Ciphertext> >  he_query = generate_query( row_index, DB_SIZE, poly_modulus_degree , encryptor );
    uint64_t hcube_dim; uint64_t hcube_len;
    get_hcube_param( &hcube_dim, &hcube_len, poly_modulus_degree, DB_SIZE);
    
    //Serialize objects.
    std::stringstream buffer;
    params.save( buffer, compr_mode_type::none );
    uint64_t prm = buffer.str().size() ;
    cout << " Parameters size: " << prm << endl;
    GalKeys.save( buffer );
    uint64_t lk = buffer.str().size();
    he_query[0][0].save( buffer, compr_mode_type::none );
    cout << "query size in bytes: " << buffer.str().size() - lk  << endl;
    string str_buffer = buffer.str();
    
    //Place into format libpqxx can read as bytea for postgres
    std::basic_string< std::byte >  query_buffer;
    query_buffer.reserve( str_buffer.size() );
    query_buffer.resize( str_buffer.size() );
    memcpy( (void *) query_buffer.data(), (void *) str_buffer.data(), str_buffer.size() );

    ///////////////////////
    //DATABASE INTERATION//
    ///////////////////////

    //Connect to database
    pqxx::connection c{"postgresql://postgres:pass@localhost/testdb"};
    pqxx::work w(c);

    //Prepare SQL query
    c.prepare( "pir_1", "SELECT pir_select( $1, description ) FROM data_10" );

    //Execute query
    pqxx::result r = w.exec_prepared("pir_1",  query_buffer );
   
    for (auto row: r){
        cout << "  Result Size: " << row[ "pir_select" ].size() << " bytes." endl;
        cout << "  Query  Size: " << query_buffer.size() << "bytes." << endl;
        
        //Interpret results as raw bytes
        auto data = row[ "pir_select" ].as<std::basic_string<std::byte>>();

        //Load raw bytes as polynomial ciphertext
        Ciphertext result;   
        stringstream stream;
        for( int jj=0; jj < data.size(); jj++) stream << (char)data[jj];
        result.load( context, stream  );

        //Do some magic and decrypt the ciphertext
        Plaintext decrypted_result;
        decryptor.decrypt( result , decrypted_result);
        Plaintext inverse("9FF7");
        Ciphertext temp;
        encryptor.encrypt( inverse, temp  );
        evaluator.multiply_plain( temp, decrypted_result, result ); 
        decryptor.decrypt( result , decrypted_result);

        //Print the plain polynomial
        cout << "Query: " << decrypted_result.to_string() << endl;

        //TODO: print polynomial data as raw ASCII chars.
    }
}
