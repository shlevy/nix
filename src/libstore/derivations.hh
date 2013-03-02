#pragma once

#include "types.hh"
#include "hash.hh"

#include <map>


namespace nix {


/* Extension of derivations in the Nix store. */
const string drvExtension = ".drv";


/* Abstract syntax of derivations. */

struct OldDerivationOutput
{
    Path path;
    string hashAlgo; /* hash used for expected hash computation */
    string hash; /* expected hash, may be null */
    OldDerivationOutput()
    {
    }
    OldDerivationOutput(Path path, string hashAlgo, string hash)
    {
        this->path = path;
        this->hashAlgo = hashAlgo;
        this->hash = hash;
    }
    void parseHashInfo(bool & recursive, HashType & hashType, Hash & hash) const;
};

typedef std::map<string, OldDerivationOutput> OldDerivationOutputs;

/* For inputs that are sub-derivations, we specify exactly which
   output IDs we are interested in. */
typedef std::map<Path, StringSet> DerivationInputs;

typedef std::map<string, string> StringPairs;

struct OldDerivation
{
    OldDerivationOutputs outputs; /* keyed on symbolic IDs */
    DerivationInputs inputDrvs; /* inputs that are sub-derivations */
    PathSet inputSrcs; /* inputs that are sources */
    string platform;
    Path builder;
    Strings args;
    StringPairs env;
};


struct DerivationOutput
{
    Path outPath;
    bool fixedOutput;
    Hash hash;
    bool recursive;
    DerivationOutput()
    {
        this->fixedOutput = false;
    }
    DerivationOutput(Hash hash, bool recursive)
    {
        this->fixedOutput = true;
        this->hash = hash;
        this->recursive = recursive;
    }
};

typedef std::map<string, DerivationOutput> DerivationOutputs;

struct Derivation
{
    public:
    string name;
    DerivationOutputs outputs;
    PathSet inputs;
    string platform;
    Path builder;
    Strings args;
    StringPairs env;
    private:
    Hash cachedHash;
    public:
    Hash hash() const;
    bool operator < (const Derivation & d2) const;
};


typedef std::set<Derivation> DerivationSet;

typedef std::map<Path,Derivation const *> BuildMap;

struct KnownDerivations
{
    DerivationSet derivations;
    BuildMap buildMap;
    void addDerivation(Derivation & drv);
    void removeDerivation(const Derivation & drv);
    const Derivation & addOldDerivation(const Path & drvPath, const PathSet & outputPaths);
};

/* Given a path, how do I build it? */
extern KnownDerivations knownDerivations;


class StoreAPI;


/* Write a derivation to the Nix store, and return its path. */
Path writeDerivation(StoreAPI & store,
    const OldDerivation & drv, const string & name, bool repair = false);

/* Parse a derivation. */
OldDerivation parseDerivation(const string & s);

/* Print a derivation. */
string unparseDerivation(const OldDerivation & drv);

/* Check whether a file name ends with the extensions for
   derivations. */
bool isDerivation(const string & fileName);

/* Return true iff this is a fixed-output derivation. */
bool isFixedOutputDrv(const OldDerivation & drv);

Hash hashDerivationModulo(StoreAPI & store, OldDerivation drv);

/* Memoisation of hashDerivationModulo(). */
typedef std::map<Path, Hash> DrvHashes;

extern DrvHashes drvHashes;

/* Split a string specifying a derivation and a set of outputs
   (/nix/store/hash-foo!out1,out2,...) into the derivation path and
   the outputs. */
typedef std::pair<string, std::set<string> > DrvPathWithOutputs;
DrvPathWithOutputs parseDrvPathWithOutputs(const string & s);

Path makeDrvPathWithOutputs(const Path & drvPath, const std::set<string> & outputs);

bool wantOutput(const string & output, const std::set<string> & wanted);


}
