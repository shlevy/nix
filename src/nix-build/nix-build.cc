#include "globals.hh"
#include "shared.hh"
#include "eval.hh"
#include "eval-inline.hh"
#include "get-drvs.hh"
#include "attr-path.hh"
#include "value-to-xml.hh"
#include "util.hh"
#include "store-api.hh"
#include "common-opts.hh"
#include "misc.hh"

#include <map>
#include <iostream>


using namespace nix;


void printHelp()
{
    showManPage("nix-build");
}


static Expr * parseStdin(EvalState & state)
{
    startNest(nest, lvlTalkative, format("parsing standard input"));
    return state.parseExprFromString(drainFD(0), absPath("."));
}


void processExpr(EvalState & state, const Strings & attrPaths,
    Bindings & autoArgs, Expr * e, PathSet & wantedPaths)
{
    foreach (Strings::const_iterator, i, attrPaths) {
        Value v;
        findAlongAttrPath(state, *i, autoArgs, e, v);
        state.forceValue(v);

        DrvInfos drvs;
        getDerivations(state, v, "", autoArgs, drvs, false);
        foreach (DrvInfos::iterator, i, drvs)
            wantedPaths.insert(i->queryOutPath(state));
    }
}


void run(Strings args)
{
    EvalState state;
    Strings files;
    bool readStdin = false;
    bool dryRun = false;
    bool ignoreUnknown = false;
    Strings attrPaths;
    Bindings autoArgs;
    string outLink = absPath("./result");
    Path gcRoot;
    std::map<Derivation, int> rootNrs;
    int rootNr = 0;
    bool indirectRoot = false;

    bool runEnv = false;
    string envCommand = (format("p=$PATH; source $stdenv/setup; PATH=$PATH:$p; exec %1%")
      % getEnv("SHELL", "/bin/sh")).str();
    StringSet envExclude;

    for (Strings::iterator i = args.begin(); i != args.end(); ) {
        string arg = *i++;

        if (arg == "-")
            readStdin = true;
        else if (arg == "--attr" || arg == "-A") {
            if (i == args.end())
                throw UsageError("`--attr' requires an argument");
            attrPaths.push_back(*i++);
        }
        else if (parseOptionArg(arg, i, args.end(), state, autoArgs))
            ;
        else if (parseSearchPathArg(arg, i, args.end(), state))
            ;
        else if (arg == "--add-root") {
            if (i == args.end())
                throw UsageError("`--add-root' requires an argument");
            gcRoot = absPath(*i++);
        }
        else if (arg == "--indirect")
            indirectRoot = true;
        else if (arg == "--repair")
            state.repair = true;
        else if (arg == "--no-out-link" || arg == "--no-link")
            outLink = "";
        else if (arg == "--out-link" || arg == "-o") {
            if (i == args.end())
                throw UsageError("`--out-link' requires an argument");
            outLink = absPath(*i++);
        }
        else if (arg == "--dry-run")
            dryRun = true;
        else if (arg == "--run-env")
            runEnv = true;
        else if (arg == "--command") {
            if (i == args.end())
                throw UsageError("`--command' requires an argument");
            envCommand = *i++;
        }
        else if (arg == "--exclude") {
            if (i == args.end())
                throw UsageError("`--exclude' requires an argument");
            envExclude.insert(*i++);
        }
        else if (arg == "--ignore-unknown")
            ignoreUnknown = true;
        else if (arg[0] == '-')
            throw UsageError(format("unknown flag `%1%'") % arg);
        else
            files.push_back(arg);
    }

    if (gcRoot == "" and outLink != "") {
        gcRoot = outLink;
        indirectRoot = true;
    }

    if (attrPaths.empty()) attrPaths.push_back("");

    store = openStore();
    PathSet wantedPaths;

    if (readStdin) {
        Expr * e = parseStdin(state);
        processExpr(state, attrPaths, autoArgs, e, wantedPaths);
    } else if (files.empty())
        files.push_back("./default.nix");

    foreach (Strings::iterator, i, files) {
        Expr * e = state.parseExprFromFile(lookupFileArg(state, *i));
        processExpr(state, attrPaths, autoArgs, e, wantedPaths);
    }

    if (runEnv) {
        if (wantedPaths.size() !=1 )
            throw Error("`--run-env' requires a single derivation");
        BuildMap::iterator i = knownDerivations.buildMap.find(
                *wantedPaths.begin());
        if (i == knownDerivations.buildMap.end())
            throw Error("`--run-env' called on value that is not a derivation");
        Derivation drv = *i->second;
        PathSet neededInputs;
        foreach (PathSet::iterator, j, drv.inputs)
            if (envExclude.find(*j) == envExclude.end())
                neededInputs.insert(*j);
        store->buildPaths(neededInputs, state.repair);

        Strings envStrs;
        foreach (StringPairs::iterator, j, drv.env)
            envStrs.push_back(j->first + "=" + j->second);

        foreach (DerivationOutputs::iterator, j, drv.outputs)
            envStrs.push_back(j->first + "=" + j->second.outPath);

        envStrs.push_back("NIX_BUILD_TOP=" + getEnv("TMPDIR", "/tmp"));
        const char * * envArr = strings2CharPtrs(envStrs);
        Path shellPath = getEnv("SHELL", "/bin/sh");
        const char *cmd = shellPath.c_str();
        execle(cmd, cmd, "-c", envCommand.c_str(), NULL, envArr);
        throw SysError("run-env execle");
    } else {
        unsigned long long downloadSize, narSize;
        PathSet willBuild, willSubstitute, unknown;
        queryMissing(*store, wantedPaths,
                willBuild, willSubstitute, unknown, downloadSize, narSize);

        if (ignoreUnknown) {
            PathSet wantedPaths2;
            foreach (PathSet::iterator, i, wantedPaths)
                if (unknown.find(*i) == unknown.end())
                    wantedPaths2.insert(*i);
            wantedPaths = wantedPaths2;
            unknown = PathSet();
        }

        printMissing(willBuild, willSubstitute, unknown, downloadSize, narSize);

        if (dryRun) return;

        store->buildPaths(wantedPaths, state.repair);

        if (!ignoreUnknown) {
            foreach (PathSet::iterator, i, wantedPaths) {
                BuildMap::iterator j = knownDerivations.buildMap.find(*i);
                if (j != knownDerivations.buildMap.end()) {
                    if (gcRoot == "")
                        printGCWarning();
                    else {
                        Derivation drv = *j->second;
                        if (rootNrs.find(drv) == rootNrs.end())
                            rootNrs[drv] = ++rootNr;
                        Path rootName = gcRoot;
                        if (rootNrs[drv] > 1)
                            rootName += "-" + int2String(rootNrs[drv]);
                        foreach (DerivationOutputs::iterator, k, drv.outputs) {
                            if (k->second.outPath == *i) {
                                if (k->first != "out")
                                    rootName += "-" + k->first;
                                break;
                            }
                        }

                        addPermRoot(*store, *i, rootName, indirectRoot);
                    }
                }
                std::cout << format("%1%\n") % *i;
            }
        }
    }
}


string programId = "nix-build";
