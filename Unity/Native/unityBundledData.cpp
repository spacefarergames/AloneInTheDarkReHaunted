#include "embedded/embeddedData.h"

// Unity distributions carry original data in AssetBundles rather than in the
// executable. Normal file access resolves the versioned bundle cache.
bool getEmbeddedFile(const char*,const unsigned char** data,size_t* size) {
    if(data)*data=nullptr;if(size)*size=0;return false;
}
bool getEmbeddedAitd1File(const char* name,const unsigned char** data,size_t* size){return getEmbeddedFile(name,data,size);}
bool getEmbeddedJackFile(const char* name,const unsigned char** data,size_t* size){return getEmbeddedFile(name,data,size);}
bool getEmbeddedAitd2File(const char* name,const unsigned char** data,size_t* size){return getEmbeddedFile(name,data,size);}
bool getEmbeddedAitd3File(const char* name,const unsigned char** data,size_t* size){return getEmbeddedFile(name,data,size);}
bool preferEmbeddedGameData(){return false;}
