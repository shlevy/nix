#include "derivations.hh"
#include "store-api.hh"
#include "globals.hh"
#include "util.hh"
#include "misc.hh"


namespace nix {


void OldDerivationOutput::parseHashInfo(bool & recursive, HashType & hashType, Hash & hash) const
{
    recursive = false;
    string algo = hashAlgo;

    if (string(algo, 0, 2) == "r:") {
        recursive = true;
        algo = string(algo, 2);
    }

    hashType = parseHashType(algo);
    if (hashType == htUnknown)
        throw Error(format("unknown hash algorithm `%1%'") % algo);

    hash = parseHash(hashType, this->hash);
}


Path writeDerivation(StoreAPI & store,
    const OldDerivation & drv, const string & name, bool repair)
{
    PathSet references;
    references.insert(drv.inputSrcs.begin(), drv.inputSrcs.end());
    foreach (DerivationInputs::const_iterator, i, drv.inputDrvs)
        references.insert(i->first);
    /* Note that the outputs of a derivation are *not* references
       (that can be missing (of course) and should not necessarily be
       held during a garbage collection). */
    string suffix = name + drvExtension;
    string contents = unparseDerivation(drv);
    return settings.readOnlyMode
        ? computeStorePathForText(suffix, contents, references)
        : store.addTextToStore(suffix, contents, references, repair);
}


static Path parsePath(std::istream & str)
{
    string s = parseString(str);
    if (s.size() == 0 || s[0] != '/')
        throw Error(format("bad path `%1%' in derivation") % s);
    return s;
}


static StringSet parseStrings(std::istream & str, bool arePaths)
{
    StringSet res;
    while (!endOfList(str))
        res.insert(arePaths ? parsePath(str) : parseString(str));
    return res;
}


OldDerivation parseDerivation(const string & s)
{
    OldDerivation drv;
    std::istringstream str(s);
    expect(str, "Derive([");

    /* Parse the list of outputs. */
    while (!endOfList(str)) {
        OldDerivationOutput out;
        expect(str, "("); string id = parseString(str);
        expect(str, ","); out.path = parsePath(str);
        expect(str, ","); out.hashAlgo = parseString(str);
        expect(str, ","); out.hash = parseString(str);
        expect(str, ")");
        drv.outputs[id] = out;
    }

    /* Parse the list of input derivations. */
    expect(str, ",[");
    while (!endOfList(str)) {
        expect(str, "(");
        Path drvPath = parsePath(str);
        expect(str, ",[");
        drv.inputDrvs[drvPath] = parseStrings(str, false);
        expect(str, ")");
    }

    expect(str, ",["); drv.inputSrcs = parseStrings(str, true);
    expect(str, ","); drv.platform = parseString(str);
    expect(str, ","); drv.builder = parseString(str);

    /* Parse the builder arguments. */
    expect(str, ",[");
    while (!endOfList(str))
        drv.args.push_back(parseString(str));

    /* Parse the environment variables. */
    expect(str, ",[");
    while (!endOfList(str)) {
        expect(str, "("); string name = parseString(str);
        expect(str, ","); string value = parseString(str);
        expect(str, ")");
        drv.env[name] = value;
    }

    expect(str, ")");
    return drv;
}


static void printString(string & res, const string & s)
{
    res += '"';
    for (const char * i = s.c_str(); *i; i++)
        if (*i == '\"' || *i == '\\') { res += "\\"; res += *i; }
        else if (*i == '\n') res += "\\n";
        else if (*i == '\r') res += "\\r";
        else if (*i == '\t') res += "\\t";
        else res += *i;
    res += '"';
}


template<class ForwardIterator>
static void printStrings(string & res, ForwardIterator i, ForwardIterator j)
{
    res += '[';
    bool first = true;
    for ( ; i != j; ++i) {
        if (first) first = false; else res += ',';
        printString(res, *i);
    }
    res += ']';
}


string unparseDerivation(const OldDerivation & drv)
{
    string s;
    s.reserve(65536);
    s += "Derive([";

    bool first = true;
    foreach (OldDerivationOutputs::const_iterator, i, drv.outputs) {
        if (first) first = false; else s += ',';
        s += '('; printString(s, i->first);
        s += ','; printString(s, i->second.path);
        s += ','; printString(s, i->second.hashAlgo);
        s += ','; printString(s, i->second.hash);
        s += ')';
    }

    s += "],[";
    first = true;
    foreach (DerivationInputs::const_iterator, i, drv.inputDrvs) {
        if (first) first = false; else s += ',';
        s += '('; printString(s, i->first);
        s += ','; printStrings(s, i->second.begin(), i->second.end());
        s += ')';
    }

    s += "],";
    printStrings(s, drv.inputSrcs.begin(), drv.inputSrcs.end());

    s += ','; printString(s, drv.platform);
    s += ','; printString(s, drv.builder);
    s += ','; printStrings(s, drv.args.begin(), drv.args.end());

    s += ",[";
    first = true;
    foreach (StringPairs::const_iterator, i, drv.env) {
        if (first) first = false; else s += ',';
        s += '('; printString(s, i->first);
        s += ','; printString(s, i->second);
        s += ')';
    }

    s += "])";

    return s;
}


bool isDerivation(const string & fileName)
{
    return hasSuffix(fileName, drvExtension);
}


bool isFixedOutputDrv(const OldDerivation & drv)
{
    return drv.outputs.size() == 1 &&
        drv.outputs.begin()->first == "out" &&
        drv.outputs.begin()->second.hash != "";
}


DrvHashes drvHashes;


/* Returns the hash of a derivation modulo fixed-output
   subderivations.  A fixed-output derivation is a derivation with one
   output (`out') for which an expected hash and hash algorithm are
   specified (using the `outputHash' and `outputHashAlgo'
   attributes).  We don't want changes to such derivations to
   propagate upwards through the dependency graph, changing output
   paths everywhere.

   For instance, if we change the url in a call to the `fetchurl'
   function, we do not want to rebuild everything depending on it
   (after all, (the hash of) the file being downloaded is unchanged).
   So the *output paths* should not change.  On the other hand, the
   *derivation paths* should change to reflect the new dependency
   graph.

   That's what this function does: it returns a hash which is just the
   hash of the derivation ATerm, except that any input derivation
   paths have been replaced by the result of a recursive call to this
   function, and that for fixed-output derivations we return a hash of
   its output path. */
Hash hashDerivationModulo(StoreAPI & store, OldDerivation drv)
{
    /* Return a fixed hash for fixed-output derivations. */
    if (isFixedOutputDrv(drv)) {
        OldDerivationOutputs::const_iterator i = drv.outputs.begin();
        return hashString(htSHA256, "fixed:out:"
            + i->second.hashAlgo + ":"
            + i->second.hash + ":"
            + i->second.path);
    }

    /* For other derivations, replace the inputs paths with recursive
       calls to this function.*/
    DerivationInputs inputs2;
    foreach (DerivationInputs::const_iterator, i, drv.inputDrvs) {
        Hash h = drvHashes[i->first];
        if (h.type == htUnknown) {
            assert(store.isValidPath(i->first));
            OldDerivation drv2 = parseDerivation(readFile(i->first));
            h = hashDerivationModulo(store, drv2);
            drvHashes[i->first] = h;
        }
        inputs2[printHash(h)] = i->second;
    }
    drv.inputDrvs = inputs2;

    return hashString(htSHA256, unparseDerivation(drv));
}


Hash Derivation::hash() const
{
    if (cachedHash.type == htUnknown) {
        string s;
        s.reserve(65536);
        s += "Derive([";

        printString(s, name); s += "],";

        bool first = true;
        foreach (DerivationOutputs::const_iterator, i, outputs) {
            if (first) first = false; else s += ',';
            s += '('; printString(s, i->first);
            if (i->second.fixedOutput) {
                s += ','; printString(s, i->second.recursive ? "recursive" : "flat");
                s += ','; printString(s, printHashType(i->second.hash.type));
                s += ','; printString(s, printHash(i->second.hash));
            }
            s += ')';
        }

        s += "],";
        printStrings(s, inputs.begin(), inputs.end());

        s += ','; printString(s, platform);
        s += ','; printString(s, builder);
        s += ','; printStrings(s, args.begin(), args.end());

        s += ",[";
        first = true;
        foreach (StringPairs::const_iterator, i, env) {
            if (first) first = false; else s += ',';
            s += '('; printString(s, i->first);
            s += ','; printString(s, i->second);
            s += ')';
        }

        s += "])";

        (Hash &) cachedHash = hashString(htSHA256, s);
    }
    return cachedHash;
}


bool Derivation::operator < (const Derivation & d2) const
{
    return hash() < d2.hash();
}


DrvPathWithOutputs parseDrvPathWithOutputs(const string & s)
{
    size_t n = s.find("!");
    return n == s.npos
        ? DrvPathWithOutputs(s, std::set<string>())
        : DrvPathWithOutputs(string(s, 0, n), tokenizeString<std::set<string> >(string(s, n + 1), ","));
}


Path makeDrvPathWithOutputs(const Path & drvPath, const std::set<string> & outputs)
{
    return outputs.empty()
        ? drvPath
        : drvPath + "!" + concatStringsSep(",", outputs);
}


bool wantOutput(const string & output, const std::set<string> & wanted)
{
    return wanted.empty() || wanted.find(output) != wanted.end();
}


void KnownDerivations::addDerivation(Derivation & drv)
{
    Derivation const *drvPointer = &(*derivations.insert(drv).first);
    foreach (DerivationOutputs::iterator, i, drv.outputs) {
        if (i->second.fixedOutput)
            i->second.outPath = makeFixedOutputPath(i->second.recursive, i->second.hash.type,
                    i->second.hash, drv.name + (i->first == "out" ? "" : "-" + i->first));
        else
            i->second.outPath = makeOutputPath(i->first, drv.hash(), drv.name);
        buildMap[i->second.outPath] = drvPointer;
    }
}


void KnownDerivations::removeDerivation(const Derivation & drv)
{
    DerivationSet::iterator i = derivations.find(drv);

    if (i == derivations.end())
        return;

    foreach (DerivationOutputs::const_iterator, j, i->outputs)
        buildMap.erase(j->second.outPath);

    derivations.erase(i);
}


const Derivation & KnownDerivations::addOldDerivation(const Path & drvPath, const PathSet & outputPaths)
{
    assert(isDerivation(drvPath));
    Derivation fakeDrv;
    /* Since derivation names can't end in .drv, this will never overlap with a
       new-style Derivation (also it has no outputs) */
    fakeDrv.name = drvPath;
    Derivation const *drvPointer = &(*derivations.insert(fakeDrv).first);
    foreach (PathSet::iterator, i, outputPaths)
        buildMap[*i] = drvPointer;
    return *drvPointer;
}


KnownDerivations knownDerivations;

}
