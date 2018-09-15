//
//

#include "Contract.h"
#include "Web3.h"
#include <WiFi.h>
#include "Util.h"
#include "Log.h"
#include <vector>

#define SIGNATURE_LENGTH 64

/**
 * Public functions
 * */

Contract::Contract(Web3* _web3, const char* address) {
    web3 = _web3;
    contractAddress = address;
    options.gas=0;
    strcpy(options.from,"");
    strcpy(options.to,"");
    strcpy(options.gasPrice,"0");
    crypto = NULL;
}

void Contract::SetPrivateKey(const uint8_t *key) {
    crypto = new Crypto(web3);
    crypto->SetPrivateKey(key);
}

string Contract::SetupContractData(const string *func, ...) {
    string ret = "";

    string contractBytes = GenerateContractBytes(func);
    ret = contractBytes;

    size_t paramCount = 0;
    vector<string> params;
    char *p;
    char tmp[func->size()];
    memset(tmp, 0, sizeof(tmp));
    strcpy(tmp, func->c_str());
    strtok(tmp, "(");
    p = strtok(0, "(");
    p = strtok(p, ")");
    p = strtok(p, ",");
    if (p != 0) {
        params.push_back(string(p));
        paramCount++;
    }
    while(p != 0) {
        p = strtok(0, ",");
        if (p != 0) {
            params.push_back(string(p));
            paramCount++;
        }
    }

    va_list args;
    va_start(args, paramCount);
    for( int i = 0; i < paramCount; ++i ) {
        if (strncmp(params[i].c_str(), "uint", sizeof("uint")) == 0) {
            string output = GenerateBytesForUint(va_arg(args, uint32_t));
            ret = ret + output;
        } else if (strncmp(params[i].c_str(), "int", sizeof("int")) == 0
                   || strncmp(params[i].c_str(), "bool", sizeof("bool")) == 0) {
            string output = GenerateBytesForInt(va_arg(args, int32_t));
            ret = ret + string(output);
        } else if (strncmp(params[i].c_str(), "address", sizeof("address")) == 0) {
            string output = GenerateBytesForAddress(va_arg(args, string*));
            ret = ret + string(output);
        } else if (strncmp(params[i].c_str(), "string", sizeof("string")) == 0) {
            string output = GenerateBytesForString(va_arg(args, string*));
            ret = ret + string(output);
        } else if (strncmp(params[i].c_str(), "bytes", sizeof("bytes")) == 0) {
            long len = strtol(params[i].c_str()+5, nullptr,10);
            string output = GenerateBytesForBytes(va_arg(args, char*), len);
            ret = ret + string(output);
        }
    }
    va_end(args);

    return ret;
}

string Contract::ViewCall(const string* param)
{
    string result = web3->EthViewCall(param, contractAddress);
    return result;
}

string Contract::Call(const string* param) 
{
    const string from = string(options.from);
    const long gasPrice = strtol(options.gasPrice, nullptr, 10);
    const string value = "";
#if 0
    printf("from: %s\n", from.c_str());
    printf("to: %s\n", contractAddress->c_str());
    printf("value: %s\n", value.c_str());
    printf("data: %s\n", param->c_str());
    printf(param->c_str());
#endif
    string result = web3->EthCall(&from, contractAddress, options.gas, gasPrice, &value, param);
    return result;
}

string Contract::SendTransaction(uint32_t nonceVal, uint32_t gasPriceVal, uint32_t gasLimitVal,
                                 string *toStr, string *valueStr, string *dataStr) {
    uint8_t signature[SIGNATURE_LENGTH];
    memset(signature,0,SIGNATURE_LENGTH);
    int recid[1] = {0};
    GenerateSignature(signature, recid, nonceVal, gasPriceVal, gasLimitVal,
                          toStr, valueStr, dataStr);
    vector<uint8_t>param = RlpEncodeForRawTransaction(nonceVal, gasPriceVal, gasLimitVal,
                                         toStr, valueStr, dataStr,
                                         signature, recid[0]);
    string paramStr = Util::VectorToString(param);

#if 0
    printf("\nGenerated Transaction--------\n ");
    printf("len:%d\n", (int)param.size());
    for (int i = 0; i<param.size(); i++) {
        printf("%02x ", param[i]);
    }
    printf("\nparamStr: %s\n", paramStr.c_str());
    printf("\n\n");
#endif

    return web3->EthSendSignedTransaction(&paramStr, param.size());

}

/**
 * Private functions
 * */

void Contract::GenerateSignature(uint8_t* signature, int* recid, uint32_t nonceVal, uint32_t gasPriceVal, uint32_t  gasLimitVal,
                                string* toStr, string* valueStr, string* dataStr) {
    vector<uint8_t> encoded = RlpEncode(nonceVal, gasPriceVal, gasLimitVal, toStr, valueStr, dataStr);

    // hash
    string t = Util::VectorToString(encoded);
    string hashedStr = web3->Web3Sha3(&t);

    // sign
    char hash[hashedStr.size()];
    memset(hash, 0, sizeof(hash));
    Util::ConvertCharStrToUintArray((uint8_t*)hash, (uint8_t*)hashedStr.c_str());
    Sign((uint8_t*)hash, signature, recid);

#if 0
    printf("\nhash_input : %s\n", tmp);
    printf("\nhash_output: %s\n", hashedStr);
    printf("\nhash--------\n ");
    for (int i = 0; i<32; i++) {
        printf("%02x ", tmp[i]);
    }
#endif
}

string Contract::GenerateContractBytes(const string* func) {
    string in = "0x";
    char intmp[8];
    memset(intmp, 0, 8);

    for (int i = 0; i<128; i++) {
        char c = (*func)[i];
        if (c == '\0') {
            break;
        }
        sprintf(intmp, "%x", c);
        in = in + intmp;
    }
    string out = web3->Web3Sha3(&in);
    out.resize(10);
    return out;
}

string Contract::GenerateBytesForUint(const uint32_t value) {
    char output[70];
    memset(output, 0, sizeof(output));

    // check number of digits
    char dummy[64];
    int digits = sprintf(dummy, "%x", (uint32_t)value);

    // fill 0 and copy number to string
    for(int i = 2; i < 2+64-digits; i++){
        sprintf(output, "%s%s", output, "0");
    }
    sprintf(output,"%s%x", output, (uint32_t)value);
    return string(output);
}

string Contract::GenerateBytesForInt(const int32_t value) {
    char output[70];
    memset(output, 0, sizeof(output));

    // check number of digits
    char dummy[64];
    int digits = sprintf(dummy, "%x", value);

    // fill 0 and copy number to string
    char fill[2];
    if (value >= 0) {
        sprintf(fill, "%s", "0");
    } else {
        sprintf(fill, "%s", "f");
    }
    for(int i = 2; i < 2+64-digits; i++){
        sprintf(output, "%s%s", output, fill);
    }
    sprintf(output,"%s%x", output, value);
    return string(output);
}

string Contract::GenerateBytesForAddress(const string* value) {
    size_t digits = value->size() - 2;

    string zeros = "";
    for(int i = 2; i < 2+64-digits; i++){
        zeros = zeros + "0";
    }
    string tmp = string(*value);
    tmp.erase(tmp.begin(), tmp.begin() + 2);
    return zeros + tmp;
}

string Contract::GenerateBytesForString(const string* value) {
    string zeros = "";
    size_t remain = 32 - ((value->size()-2) % 32);
    for(int i = 0; i < remain + 32; i++){
        zeros = zeros + "0";
    }

    return *value + zeros;
}

string Contract::GenerateBytesForBytes(const char* value, const int len) {
    char output[70];
    memset(output, 0, sizeof(output));

    for(int i = 0; i < len; i++){
        sprintf(output, "%s%x", output, value[i]);
    }
    size_t remain = 32 - ((strlen(output)-2) % 32);
    for(int i = 0; i < remain + 32; i++){
        sprintf(output, "%s%s", output, "0");
    }

    return string(output);
}

vector<uint8_t> Contract::RlpEncode(
                         uint32_t nonceVal, uint32_t gasPriceVal, uint32_t  gasLimitVal,
                         string* toStr, string* valueStr, string* dataStr) {
    vector<uint8_t> nonce = Util::ConvertNumberToVector(nonceVal);
    vector<uint8_t> gasPrice = Util::ConvertNumberToVector(gasPriceVal);
    vector<uint8_t> gasLimit = Util::ConvertNumberToVector(gasLimitVal);
    vector<uint8_t> to = Util::ConvertStringToVector(toStr);
    vector<uint8_t> value = Util::ConvertStringToVector(valueStr);
    vector<uint8_t> data = Util::ConvertStringToVector(dataStr);

    vector<uint8_t> outputNonce = Util::RlpEncodeItemWithVector(nonce);
    vector<uint8_t> outputGasPrice = Util::RlpEncodeItemWithVector(gasPrice);
    vector<uint8_t> outputGasLimit = Util::RlpEncodeItemWithVector(gasLimit);
    vector<uint8_t> outputTo = Util::RlpEncodeItemWithVector(to);
    vector<uint8_t> outputValue = Util::RlpEncodeItemWithVector(value);
    vector<uint8_t> outputData = Util::RlpEncodeItemWithVector(data);

    vector<uint8_t> encoded = Util::RlpEncodeWholeHeaderWithVector(
            outputNonce.size()+
            outputGasPrice.size()+
            outputGasLimit.size()+
            outputTo.size()+
            outputValue.size()+
            outputData.size());


    encoded.insert(encoded.end(), outputNonce.begin(), outputNonce.end());
    encoded.insert(encoded.end(), outputGasPrice.begin(), outputGasPrice.end());
    encoded.insert(encoded.end(), outputGasLimit.begin(), outputGasLimit.end());
    encoded.insert(encoded.end(), outputTo.begin(), outputTo.end());
    encoded.insert(encoded.end(), outputValue.begin(), outputValue.end());
    encoded.insert(encoded.end(), outputData.begin(), outputData.end());

#if 0
    printf("\nRLP encoded--------\n ");
    printf("\nlength : %d\n", p);
    for (int i = 0; i<p; i++) {
        printf("%02x ", i, encoded[i]);
    }
#endif

    return encoded;
}

void Contract::Sign(uint8_t* hash, uint8_t* sig, int* recid) 
{
    // TODO:
    // Use micro eth ECDSA code
    BYTE fullSig[65];
    crypto->Sign(hash, fullSig);
    *recid = fullSig[64];

    // secp256k1_nonce_function noncefn = secp256k1_nonce_function_rfc6979;
    // void* data_ = NULL;

    // secp256k1_ecdsa_recoverable_signature signature;
    // memset(&signature, 0, sizeof(signature));
    // if (secp256k1_ecdsa_sign_recoverable(ctx, &signature, hash,  privateKey, noncefn, data_) == 0) {
    //     return;
    // }

    // secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, &sig[0], &recid[0], &signature);

#if 0
    printf("\nhash--------\n ");
    for (int i = 0; i<32; i++) {
        printf("%02x ", hash[i]);
    }
    printf("\npriv--------\n ");
    for (int i = 0; i<32; i++) {
        printf("%02x ", privateKey[i]);
    }
    printf("\nsignature--------\n ");
    for (int i = 0; i<64; i++) {
        printf("%02x ", sig[i]);
    }
#endif
}

vector<uint8_t> Contract::RlpEncodeForRawTransaction(
                             uint32_t nonceVal, uint32_t gasPriceVal, uint32_t  gasLimitVal,
                             string* toStr, string* valueStr, string* dataStr, uint8_t* sig, uint8_t recid) {

    vector<uint8_t> signature;
    for (int i=0; i<SIGNATURE_LENGTH; i++) {
        signature.push_back(sig[i]);
    }
    vector<uint8_t> nonce = Util::ConvertNumberToVector(nonceVal);
    vector<uint8_t> gasPrice = Util::ConvertNumberToVector(gasPriceVal);
    vector<uint8_t> gasLimit = Util::ConvertNumberToVector(gasLimitVal);
    vector<uint8_t> to = Util::ConvertStringToVector(toStr);
    vector<uint8_t> value = Util::ConvertStringToVector(valueStr);
    vector<uint8_t> data = Util::ConvertStringToVector(dataStr);

    vector<uint8_t> outputNonce = Util::RlpEncodeItemWithVector(nonce);
    vector<uint8_t> outputGasPrice = Util::RlpEncodeItemWithVector(gasPrice);
    vector<uint8_t> outputGasLimit = Util::RlpEncodeItemWithVector(gasLimit);
    vector<uint8_t> outputTo = Util::RlpEncodeItemWithVector(to);
    vector<uint8_t> outputValue = Util::RlpEncodeItemWithVector(value);
    vector<uint8_t> outputData = Util::RlpEncodeItemWithVector(data);

    vector<uint8_t> R;
    R.insert(R.end(), signature.begin(), signature.begin()+(SIGNATURE_LENGTH/2));
    vector<uint8_t> S;
    S.insert(S.end(), signature.begin()+(SIGNATURE_LENGTH/2), signature.end());
    vector<uint8_t> V;
    V.push_back((uint8_t)(recid+27)); // 27 is a magic number for Ethereum spec
    vector<uint8_t> outputR = Util::RlpEncodeItemWithVector(R);
    vector<uint8_t> outputS = Util::RlpEncodeItemWithVector(S);
    vector<uint8_t> outputV = Util::RlpEncodeItemWithVector(V);

#if 0
    printf("\noutputNonce--------\n ");
    for (int i = 0; i<outputNonce.size(); i++) { printf("%02x ", outputNonce[i]); }
    printf("\noutputGasPrice--------\n ");
    for (int i = 0; i<outputGasPrice.size(); i++) {printf("%02x ", outputGasPrice[i]); }
    printf("\noutputGasLimit--------\n ");
    for (int i = 0; i<outputGasLimit.size(); i++) {printf("%02x ", outputGasLimit[i]); }
    printf("\noutputTo--------\n ");
    for (int i = 0; i<outputTo.size(); i++) {printf("%02x ", outputTo[i]); }
    printf("\noutputValue--------\n ");
    for (int i = 0; i<outputValue.size(); i++) { printf("%02x ", outputValue[i]); }
    printf("\noutputData--------\n ");
    for (int i = 0; i<outputData.size(); i++) { printf("%02x ", outputData[i]); }
    printf("\nR--------\n ");
    for (int i = 0; i<outputR.size(); i++) { printf("%02x ", outputR[i]); }
    printf("\nS--------\n ");
    for (int i = 0; i<outputS.size(); i++) { printf("%02x ", outputS[i]); }
    printf("\nV--------\n ");
    for (int i = 0; i<outputV.size(); i++) { printf("%02x ", outputV[i]); }
    printf("\n");
#endif

    vector<uint8_t> encoded = Util::RlpEncodeWholeHeaderWithVector(
                                        outputNonce.size()+
                                        outputGasPrice.size()+
                                        outputGasLimit.size()+
                                        outputTo.size()+
                                        outputValue.size()+
                                        outputData.size()+
                                        outputR.size()+
                                        outputS.size()+
                                        outputV.size());

    encoded.insert(encoded.end(), outputNonce.begin(), outputNonce.end());
    encoded.insert(encoded.end(), outputGasPrice.begin(), outputGasPrice.end());
    encoded.insert(encoded.end(), outputGasLimit.begin(), outputGasLimit.end());
    encoded.insert(encoded.end(), outputTo.begin(), outputTo.end());
    encoded.insert(encoded.end(), outputValue.begin(), outputValue.end());
    encoded.insert(encoded.end(), outputData.begin(), outputData.end());
    encoded.insert(encoded.end(), outputV.begin(), outputV.end());
    encoded.insert(encoded.end(), outputR.begin(), outputR.end());
    encoded.insert(encoded.end(), outputS.begin(), outputS.end());

    return encoded;
}

void ConvertCharStrToVector(string *value, vector<string> *result);

vector<string> *Contract::InterpretVectorResult(string *result)
{
    const char *resultScan = "result\":\"";
    std::size_t found = result->find(resultScan);
    vector<string> *retVal = new vector<string>();
    
    if (found != std::string::npos)
    {
        found += strlen(resultScan);
        string newChar = result->substr(found);
        //first test if it's array type
        vector<string> breakDown;
        ConvertCharStrToVector(&newChar, &breakDown);

        if (breakDown.size() > 2)
        {
            //check first value
            auto itr = breakDown.begin();
            long dyn = strtol(itr++->c_str(), NULL, 16);
            if (dyn == 32) //array marker
            {
                long length = strtol(itr++->c_str(), NULL, 16);
                
                //checksum
                if (breakDown.size() != (length + 2))
                {
                    Serial.println("Bad array result data.");
                    for (itr = breakDown.begin(); itr != breakDown.end(); itr++) Serial.println(*itr->c_str());
                }
                for (;itr != breakDown.end(); itr++)
                {
                    retVal->push_back(*itr);
                }   
            }
        }
    }

    return retVal;
}

void ConvertCharStrToVector(string *value, vector<string> *result) 
{
    int index = 0;
    if (value->find("0x") != std::string::npos) index = 2;

    while (index <= (value->length() - 64))
    {
        //Serial.println(value->substr(index, 64).c_str());
        result->push_back(value->substr(index, 64));
        index += 64;
    }
}