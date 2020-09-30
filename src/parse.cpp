#include "parse.h"

#define INTEGER "\"integer\""_json
#define NUMERIC "\"numeric\""_json
#define LOGICAL "\"logical\""_json
#define STRING "\"character\""_json
#define NIL "null"_json

using json = nlohmann::json;

json serialize_list(Rcpp::List data, json schema);
json serialize_dataframe(Rcpp::DataFrame data, json schema);
SEXP _deserialize(json data, json schema);

SEXP deserialize(std::string data, std::string schema){
    return(_deserialize(json::parse(data), json::parse(schema)));
}

// SEXP types, from https://cran.r-project.org/doc/manuals/r-release/R-ints.html#SEXPs
// 0     NILSXP      NULL
// 1     SYMSXP      symbols
// 2     LISTSXP     pairlists
// 3     CLOSXP      closures
// 4     ENVSXP      environments
// 5     PROMSXP     promises
// 6     LANGSXP     language objects
// 7     SPECIALSXP  special functions
// 8     BUILTINSXP  builtin functions
// 9     CHARSXP     internal character strings
// 10    LGLSXP      logical vectors
// 13    INTSXP      integer vectors
// 14    REALSXP     numeric vectors
// 15    CPLXSXP     complex vectors
// 16    STRSXP      character vectors
// 17    DOTSXP      dot-dot-dot object
// 18    ANYSXP      make “any” args work
// 19    VECSXP      list (generic vector)
// 20    EXPRSXP     expression vector
// 21    BCODESXP    byte code
// 22    EXTPTRSXP   external pointer
// 23    WEAKREFSXP  weak reference
// 24    RAWSXP      raw vector
// 25    S4SXP       S4 classes not of simple type

Rcpp::String serialize(SEXP x, std::string schema){
    json schema_obj = json::parse(schema);
    json result = serialize(x, schema_obj);
    return(result.dump());
}

json serialize(SEXP x, json schema){
    bool is_atomic = schema.is_string();
    json output;
    switch(TYPEOF(x)){
        case NILSXP:
            // the uninitialized json is already null
            break;
        case REALSXP:
            if (is_atomic) {
                if (schema == INTEGER){
                    output = Rcpp::as<int>(x);
                } else {
                    output = Rcpp::as<double>(x);
                }
            } else {
                if (schema["list"] == INTEGER){
                    output = Rcpp::as<Rcpp::IntegerVector>(x);
                } else {
                    output = Rcpp::as<Rcpp::NumericVector>(x);
                }
            }
            break;
        case INTSXP:
            if (is_atomic) {
                output = Rcpp::as<int>(x);
            } else {
                output = Rcpp::as<Rcpp::IntegerVector>(x);
            }
            break;
        case STRSXP:
            if (is_atomic) {
                output = Rcpp::as<std::string>(x);
            } else {
                output = Rcpp::as<Rcpp::StringVector>(x);
            }
            break;
        case LGLSXP:
            if (is_atomic) {
                output = Rcpp::as<bool>(x);
            } else {
                output = Rcpp::as<Rcpp::LogicalVector>(x);
            }
            break;
        case VECSXP:
            output = serialize_list(Rcpp::as<Rcpp::List>(x), schema);
            break;
        default:
            Rcpp::Rcerr << "Serialization failure, could not serialize type: "
                        << schema << std::endl;
            break;
    }
    return output;
}

// serialize(list(1,2), '{"tuple":["integer", "integer"]}')
json serialize_list(Rcpp::List x, json schema){
    json output;
    if (schema.contains("tuple")){
        for(size_t i = 0; i < x.size(); i++){
            json el = serialize(x.at(i), schema["tuple"].at(i));
            output.push_back(el);
        }
    } else if (schema.contains("list")){
        json list_type = schema["list"].at(0);
        for(Rcpp::List::iterator it = x.begin(); it != x.end(); ++it) {
            json el = serialize(*it, list_type);
            output.push_back(el);
        }
    } else {
        Rcpp::Rcerr << "Records not yet supported: '" << schema.dump() << std::endl;
    }
    return(output);
}


json serialize_dataframe(Rcpp::DataFrame, json schema){
    json j("dataFrame");
    return(j);
}

SEXP _deserialize(json data, json schema){
    // FIXME: This is grossly inefficient. What we SHOULD do is convert the
    // schema into a structure of enums (e.g., [INT, DOUBLE, BOOL, LIST, TUPLE,
    // ...])

    SEXP result;

    if (schema.is_string()){
        if (schema == INTEGER){
            Rcpp::IntegerVector x = { data.get<int>() };
            result = Rcpp::as<SEXP>(x);
        } else if (schema == NUMERIC) {
            Rcpp::NumericVector x = { data.get<double>() };
            result = Rcpp::as<SEXP>(x);
        } else if (schema == LOGICAL) {
            Rcpp::LogicalVector x = { data.get<bool>() };
            result = Rcpp::as<SEXP>(x);
        } else if (schema == STRING) {
            Rcpp::StringVector x = { data.get<std::string>() };
            result = Rcpp::as<SEXP>(x);
        } else {
            Rcpp::Rcerr << "Unrecognized R value type: '" << schema.dump()
                        << "' returning NULL" << std::endl;
            result = Rcpp::as<SEXP>(R_NilValue);
        }
    } else if (schema.is_null()){
        result = Rcpp::as<SEXP>(R_NilValue);
    } else {
        if (schema.contains("list")){
            json list_type = schema["list"].at(0);
            if (list_type == INTEGER){
                std::vector<int> xs = data.get<std::vector<int>>();
                Rcpp::IntegerVector rs = Rcpp::IntegerVector::import(xs.begin(), xs.end());
                result = Rcpp::as<SEXP>(rs);
            } else if (list_type == NUMERIC) {
                std::vector<double> xs = data.get<std::vector<double>>();
                Rcpp::NumericVector rs = Rcpp::NumericVector::import(xs.begin(), xs.end());
                result = Rcpp::as<SEXP>(rs);
            } else if (list_type == LOGICAL) {
                std::vector<bool> xs = data.get<std::vector<bool>>();
                Rcpp::LogicalVector rs = Rcpp::LogicalVector::import(xs.begin(), xs.end());
                result = Rcpp::as<SEXP>(rs);
            } else if (list_type == STRING) {
                std::vector<std::string> xs = data.get<std::vector<std::string>>();
                Rcpp::StringVector rs = Rcpp::StringVector::import(xs.begin(), xs.end());
                result = Rcpp::as<SEXP>(rs);
            } else {
                Rcpp::List L = Rcpp::List::create();
                for (json::iterator it = data.begin(); it != data.end(); ++it) {
                    SEXP val = _deserialize(*it, list_type);
                    L.push_back(val);
                }
                result = Rcpp::as<SEXP>(L);
            }
        } else if (schema.contains("tuple")){
            Rcpp::List L = Rcpp::List::create();
            json tuple_types = schema["tuple"];
            for (size_t i = 0; i < tuple_types.size(); i++){
                SEXP val = _deserialize(data.at(i), tuple_types.at(i));
                L.push_back(val);
            }
            result = Rcpp::as<SEXP>(L);
        } else {
            Rcpp::Rcerr << "Unrecognized R object type: " << schema.dump() << std::endl;
        }
    }
    return(result);
}
