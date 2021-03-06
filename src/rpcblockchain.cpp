// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "consensus/validation.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "sync.h"
#include "util.h"

#include <stdint.h>

#include <univalue.h>

#include <regex>

using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);

double GetDifficultyINTERNAL(const CBlockIndex* blockindex, bool networkDifficulty)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    uint32_t bits;
    if (networkDifficulty) {
        bits = GetNextWorkRequired(blockindex, nullptr, Params().GetConsensus());
    } else {
        bits = blockindex->nBits;
    }

    uint32_t powLimit =
        UintToArith256(Params().GetConsensus().powLimit).GetCompact();
    int nShift = (bits >> 24) & 0xff;
    int nShiftAmount = (powLimit >> 24) & 0xff;

    double dDiff =
        (double)(powLimit & 0x00ffffff) /
        (double)(bits & 0x00ffffff);

    while (nShift < nShiftAmount)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > nShiftAmount)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetDifficulty(const CBlockIndex* blockindex)
{
    return GetDifficultyINTERNAL(blockindex, false);
}

double GetNetworkDifficulty(const CBlockIndex* blockindex)
{
    return GetDifficultyINTERNAL(blockindex, true);
}

UniValue blockheaderToJSON(const CBlockIndex* blockindex)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", blockindex->nVersion));
    result.push_back(Pair("merkleroot", blockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", (int64_t)blockindex->nTime));
    result.push_back(Pair("nonce", blockindex->nNonce.GetHex()));
    result.push_back(Pair("solution", HexStr(blockindex->nSolution)));
    result.push_back(Pair("bits", strprintf("%08x", blockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    UniValue txs(UniValue::VARR);
    BOOST_FOREACH(const CTransaction&tx, block.vtx)
    {
        if(txDetails)
        {
            UniValue objTx(UniValue::VOBJ);
            TxToJSON(tx, uint256(), objTx);
            txs.push_back(objTx);
        }
        else
            txs.push_back(tx.GetHash().GetHex());
    }
    result.push_back(Pair("tx", txs));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("nonce", block.nNonce.GetHex()));
    result.push_back(Pair("solution", HexStr(block.nSolution)));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));
    result.push_back(Pair("anchor", blockindex->hashAnchorEnd.GetHex()));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}

UniValue getblockcount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "\nReturns the number of blocks in the longest block chain.\n"
            "\nResult:\n"
            "n    (numeric) The current block count\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        );

    LOCK(cs_main);
    return chainActive.Height();
}

UniValue getbestblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest block chain.\n"
            "\nResult\n"
            "\"hex\"      (string) the block hash hex encoded\n"
            "\nExamples\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        );

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

UniValue getdifficulty(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nResult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        );

    LOCK(cs_main);
    return GetNetworkDifficulty();
}

UniValue mempoolToJSON(bool fVerbose = false)
{
    if (fVerbose)
    {
        LOCK(mempool.cs);
        UniValue o(UniValue::VOBJ);
        BOOST_FOREACH(const PAIRTYPE(uint256, CTxMemPoolEntry)& entry, mempool.mapTx)
        {
            const uint256& hash = entry.first;
            const CTxMemPoolEntry& e = entry.second;
            UniValue info(UniValue::VOBJ);
            info.push_back(Pair("size", (int)e.GetTxSize()));
            info.push_back(Pair("fee", ValueFromAmount(e.GetFee())));
            info.push_back(Pair("time", e.GetTime()));
            info.push_back(Pair("height", (int)e.GetHeight()));
            info.push_back(Pair("startingpriority", e.GetPriority(e.GetHeight())));
            info.push_back(Pair("currentpriority", e.GetPriority(chainActive.Height())));
            const CTransaction& tx = e.GetTx();
            set<string> setDepends;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                if (mempool.exists(txin.prevout.hash))
                    setDepends.insert(txin.prevout.hash.ToString());
            }

            UniValue depends(UniValue::VARR);
            BOOST_FOREACH(const string& dep, setDepends)
            {
                depends.push_back(dep);
            }

            info.push_back(Pair("depends", depends));
            o.push_back(Pair(hash.ToString(), info));
        }
        return o;
    }
    else
    {
        vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        UniValue a(UniValue::VARR);
        BOOST_FOREACH(const uint256& hash, vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

UniValue getrawmempool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawmempool ( verbose )\n"
            "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\nArguments:\n"
            "1. verbose           (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"
            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            "    \"size\" : n,             (numeric) transaction size in bytes\n"
            "    \"fee\" : n,              (numeric) transaction fee in bitcoins\n"
            "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
            "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
            "    \"startingpriority\" : n, (numeric) priority when transaction entered pool\n"
            "    \"currentpriority\" : n,  (numeric) transaction priority now\n"
            "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",    (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "}\n"
            "\nExamples\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        );

    LOCK(cs_main);

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = params[0].get_bool();

    return mempoolToJSON(fVerbose);
}

UniValue getblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash index\n"
            "\nReturns hash of block in best-block-chain at index provided.\n"
            "\nArguments:\n"
            "1. index         (numeric, required) The block index\n"
            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        );

    LOCK(cs_main);

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    CBlockIndex* pblockindex = chainActive[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

/*uint256 _safecoin_getblockhash(int32_t nHeight)
{
    uint256 hash;
    LOCK(cs_main);
    if ( nHeight >= 0 && nHeight <= chainActive.Height() )
    {
        CBlockIndex* pblockindex = chainActive[nHeight];
        hash = pblockindex->GetBlockHash();
        int32_t i;
        for (i=0; i<32; i++)
            printf("%02x",((uint8_t *)&hash)[i]);
        printf(" blockhash.%d\n",nHeight);
    } else memset(&hash,0,sizeof(hash));
    return(hash);
}*/

UniValue getblockheader(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblockheader \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
            "If verbose is true, returns an Object with information about blockheader <hash>.\n"
            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (!fVerbose)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << pblockindex->GetBlockHeader();
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockheaderToJSON(pblockindex);
}

UniValue getblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock \"hash|height\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for block 'hash|height'.\n"
            "If verbose is true, returns an Object with information about block <hash|height>.\n"
            "\nArguments:\n"
            "1. \"hash|height\"     (string, required) The block hash or height\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",       (string) the block hash (same as provided hash)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"height\" : n,          (numeric) The block height or index (same as provided height)\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\",   (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleCli("getblock", "12800")
            + HelpExampleRpc("getblock", "12800")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();

    // If height is supplied, find the hash
    if (strHash.size() < (2 * sizeof(uint256))) {
        // std::stoi allows characters, whereas we want to be strict
        regex r("[[:digit:]]+");
        if (!regex_match(strHash, r)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
        }

        int nHeight = -1;
        try {
            nHeight = std::stoi(strHash);
        }
        catch (const std::exception &e) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
        }

        if (nHeight < 0 || nHeight > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
        }
        strHash = chainActive[nHeight]->GetBlockHash().GetHex();
    }

    uint256 hash(uint256S(strHash));

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not available (pruned data)");

    if(!ReadBlockFromDisk(block, pblockindex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    if (!fVerbose)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockToJSON(block, pblockindex);
}

UniValue gettxoutsetinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "\nReturns statistics about the unspent transaction output set.\n"
            "Note this call may take some time.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\":n,     (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) The number of transactions\n"
            "  \"txouts\": n,            (numeric) The number of output transactions\n"
            "  \"bytes_serialized\": n,  (numeric) The serialized size\n"
            "  \"hash_serialized\": \"hash\",   (string) The serialized hash\n"
            "  \"total_amount\": x.xxx          (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettxoutsetinfo", "")
            + HelpExampleRpc("gettxoutsetinfo", "")
        );

    UniValue ret(UniValue::VOBJ);

    CCoinsStats stats;
    FlushStateToDisk();
    if (pcoinsTip->GetStats(stats)) {
        ret.push_back(Pair("height", (int64_t)stats.nHeight));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", (int64_t)stats.nTransactions));
        ret.push_back(Pair("txouts", (int64_t)stats.nTransactionOutputs));
        ret.push_back(Pair("bytes_serialized", (int64_t)stats.nSerializedSize));
        ret.push_back(Pair("hash_serialized", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    }
    return ret;
}

#include "safecoin_defs.h"

#define IGUANA_MAXSCRIPTSIZE 10001
#define SAFECOIN_KVDURATION 1440
#define SAFECOIN_KVBINARY 2
extern char ASSETCHAINS_SYMBOL[SAFECOIN_ASSETCHAIN_MAXLEN];
uint64_t safecoin_interest(int32_t txheight,uint64_t nValue,uint32_t nLockTime,uint32_t tiptime);
uint32_t safecoin_txtime(uint256 hash);
uint64_t safecoin_paxprice(uint64_t *seedp,int32_t height,char *base,char *rel,uint64_t basevolume);
int32_t safecoin_paxprices(int32_t *heights,uint64_t *prices,int32_t max,char *base,char *rel);
int32_t safecoin_notaries(uint8_t pubkeys[64][33],int32_t height);
char *bitcoin_address(char *coinaddr,uint8_t addrtype,uint8_t *pubkey_or_rmd160,int32_t len);
//uint32_t safecoin_interest_args(int32_t *txheightp,uint32_t *tiptimep,uint64_t *valuep,uint256 hash,int32_t n);
int32_t safecoin_minerids(uint8_t *minerids,int32_t height,int32_t width);
int32_t safecoin_kvsearch(uint256 *refpubkeyp,int32_t current_height,uint32_t *flagsp,int32_t *heightp,uint8_t value[IGUANA_MAXSCRIPTSIZE],uint8_t *key,int32_t keylen);
/*uint64_t conv_NXTpassword(unsigned char *mysecret,unsigned char *mypublic,uint8_t *pass,int32_t passlen);


UniValue passphrasewif(const UniValue& params, bool fHelp)
{
    UniValue ret(UniValue::VOBJ); char *passphrase,wifstr[64],coinaddr[64]; uint8_t tmptype,pubkey33[33]; void *ctx; uint256 privkey,pubkey;
    passphrase = params[0].get_str().c_str();
    conv_NXTpassword((void *)&privkey,(void *)&pubkey,(uint8_t *)passphrase,(int32_t)strlen(passphrase));
    ctx = bitcoin_ctx();
    bitcoin_priv2pub(ctx,pubkey33,coinaddr,privkey,0,60);
    bitcoin_priv2wif(0,wifstr,privkey,188);
    free(ctx);
    ret.push_back(Pair("address",coinaddr));
    ret.push_back(Pair("wif",wifstr));
    return ret;
}*/

UniValue kvsearch(const UniValue& params, bool fHelp)
{
    UniValue ret(UniValue::VOBJ); uint32_t flags; uint8_t value[IGUANA_MAXSCRIPTSIZE],key[IGUANA_MAXSCRIPTSIZE]; int32_t duration,j,height,valuesize,keylen; uint256 refpubkey; static uint256 zeroes;
    if (fHelp || params.size() != 1 )
        throw runtime_error("kvsearch key");
    LOCK(cs_main);
    if ( (keylen= (int32_t)strlen(params[0].get_str().c_str())) > 0 )
    {
        ret.push_back(Pair("coin",(char *)(ASSETCHAINS_SYMBOL[0] == 0 ? "SAFE" : ASSETCHAINS_SYMBOL)));
        ret.push_back(Pair("currentheight", (int64_t)chainActive.Tip()->nHeight));
        ret.push_back(Pair("key",params[0].get_str()));
        ret.push_back(Pair("keylen",keylen));
        if ( keylen < sizeof(key) )
        {
            memcpy(key,params[0].get_str().c_str(),keylen);
            if ( (valuesize= safecoin_kvsearch(&refpubkey,chainActive.Tip()->nHeight,&flags,&height,value,key,keylen)) >= 0 )
            {
                std::string val; char *valuestr;
                val.resize(valuesize);
                valuestr = (char *)val.data();
                memcpy(valuestr,value,valuesize);
                if ( memcmp(&zeroes,&refpubkey,sizeof(refpubkey)) != 0 )
                    ret.push_back(Pair("owner",refpubkey.GetHex()));
                ret.push_back(Pair("height",height));
                duration = ((flags >> 2) + 1) * SAFECOIN_KVDURATION;
                ret.push_back(Pair("expiration", (int64_t)(height+duration)));
                ret.push_back(Pair("flags",(int64_t)flags));
                ret.push_back(Pair("value",val));
                ret.push_back(Pair("valuesize",valuesize));
            } else ret.push_back(Pair("error",(char *)"cant find key"));
        } else ret.push_back(Pair("error",(char *)"key too big"));
    } else ret.push_back(Pair("error",(char *)"null key"));
    return ret;
}

UniValue minerids(const UniValue& params, bool fHelp)
{
    UniValue ret(UniValue::VOBJ); UniValue a(UniValue::VARR); uint8_t minerids[2000],pubkeys[65][33]; int32_t i,j,n,numnotaries,tally[129];
    if ( fHelp || params.size() != 1 )
        throw runtime_error("minerids needs height\n");
    LOCK(cs_main);
    int32_t height = atoi(params[0].get_str().c_str());
    if ( height <= 0 )
        height = chainActive.Tip()->nHeight;
    if ( (n= safecoin_minerids(minerids,height,(int32_t)(sizeof(minerids)/sizeof(*minerids)))) > 0 )
    {
        memset(tally,0,sizeof(tally));
        numnotaries = safecoin_notaries(pubkeys,height);
        if ( numnotaries > 0 )
        {
            for (i=0; i<n; i++)
            {
                if ( minerids[i] >= numnotaries )
                    tally[128]++;
                else tally[minerids[i]]++;
            }
            for (i=0; i<64; i++)
            {
                UniValue item(UniValue::VOBJ); std::string hex,SAFEaddress; char *hexstr,SAFEaddr[64],*ptr; int32_t m;
                hex.resize(66);
                hexstr = (char *)hex.data();
                for (j=0; j<33; j++)
                    sprintf(&hexstr[j*2],"%02x",pubkeys[i][j]);
                item.push_back(Pair("notaryid", i));

                bitcoin_address(SAFEaddr,60,pubkeys[i],33);
                m = (int32_t)strlen(SAFEaddr);
                SAFEaddress.resize(m);
                ptr = (char *)SAFEaddress.data();
                memcpy(ptr,SAFEaddr,m);
                item.push_back(Pair("SAFEaddress", SAFEaddress));

                item.push_back(Pair("pubkey", hex));
                item.push_back(Pair("blocks", tally[i]));
                a.push_back(item);
            }
            UniValue item(UniValue::VOBJ);
            item.push_back(Pair("pubkey", (char *)"external miners"));
            item.push_back(Pair("blocks", tally[128]));
            a.push_back(item);
        }
        ret.push_back(Pair("mined", a));
    } else ret.push_back(Pair("error", (char *)"couldnt extract minerids"));
    return ret;
}

UniValue notaries(const UniValue& params, bool fHelp)
{
    UniValue a(UniValue::VARR); UniValue ret(UniValue::VOBJ); int32_t i,j,n,m; char *hexstr;  uint8_t pubkeys[64][33]; char btcaddr[64],SAFEaddr[64],*ptr;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("notaries height\n");
    LOCK(cs_main);
    int32_t height = atoi(params[0].get_str().c_str());
    if ( height < 0 )
        height = chainActive.Tip()->nHeight;
    //fprintf(stderr,"notaries as of height.%d\n",height);
    //if ( height > chainActive.Height()+20000 )
    //    throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    //else
    {
        if ( (n= safecoin_notaries(pubkeys,height)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                UniValue item(UniValue::VOBJ);
                std::string btcaddress,SAFEaddress,hex;
                hex.resize(66);
                hexstr = (char *)hex.data();
                for (j=0; j<33; j++)
                    sprintf(&hexstr[j*2],"%02x",pubkeys[i][j]);
                item.push_back(Pair("pubkey", hex));

                bitcoin_address(btcaddr,0,pubkeys[i],33);
                m = (int32_t)strlen(btcaddr);
                btcaddress.resize(m);
                ptr = (char *)btcaddress.data();
                memcpy(ptr,btcaddr,m);
                item.push_back(Pair("BTCaddress", btcaddress));

                bitcoin_address(SAFEaddr,60,pubkeys[i],33);
                m = (int32_t)strlen(SAFEaddr);
                SAFEaddress.resize(m);
                ptr = (char *)SAFEaddress.data();
                memcpy(ptr,SAFEaddr,m);
                item.push_back(Pair("SAFEaddress", SAFEaddress));
                a.push_back(item);
            }
        }
        ret.push_back(Pair("notaries", a));
        ret.push_back(Pair("numnotaries", n));
    }
    return ret;
}

int32_t safecoin_pending_withdraws(char *opretstr);
int32_t pax_fiatstatus(uint64_t *available,uint64_t *deposited,uint64_t *issued,uint64_t *withdrawn,uint64_t *approved,uint64_t *redeemed,char *base);
extern char CURRENCIES[][8];

UniValue paxpending(const UniValue& params, bool fHelp)
{
    UniValue ret(UniValue::VOBJ); UniValue a(UniValue::VARR); char opretbuf[10000*2]; int32_t opretlen,baseid; uint64_t available,deposited,issued,withdrawn,approved,redeemed;
    if ( fHelp || params.size() != 0 )
        throw runtime_error("paxpending needs no args\n");
    LOCK(cs_main);
    if ( (opretlen= safecoin_pending_withdraws(opretbuf)) > 0 )
        ret.push_back(Pair("withdraws", opretbuf));
    else ret.push_back(Pair("withdraws", (char *)""));
    for (baseid=0; baseid<32; baseid++)
    {
        UniValue item(UniValue::VOBJ); UniValue obj(UniValue::VOBJ);
        if ( pax_fiatstatus(&available,&deposited,&issued,&withdrawn,&approved,&redeemed,CURRENCIES[baseid]) == 0 )
        {
            if ( deposited != 0 || issued != 0 || withdrawn != 0 || approved != 0 || redeemed != 0 )
            {
                item.push_back(Pair("available", ValueFromAmount(available)));
                item.push_back(Pair("deposited", ValueFromAmount(deposited)));
                item.push_back(Pair("issued", ValueFromAmount(issued)));
                item.push_back(Pair("withdrawn", ValueFromAmount(withdrawn)));
                item.push_back(Pair("approved", ValueFromAmount(approved)));
                item.push_back(Pair("redeemed", ValueFromAmount(redeemed)));
                obj.push_back(Pair(CURRENCIES[baseid],item));
                a.push_back(obj);
            }
        }
    }
    ret.push_back(Pair("fiatstatus", a));
    return ret;
}

UniValue paxprice(const UniValue& params, bool fHelp)
{
    if ( fHelp || params.size() > 4 || params.size() < 2 )
        throw runtime_error("paxprice \"base\" \"rel\" height\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ); uint64_t basevolume=0,relvolume,seed;
    std::string base = params[0].get_str();
    std::string rel = params[1].get_str();
    int32_t height;
    if ( params.size() == 2 )
        height = chainActive.Tip()->nHeight;
    else height = atoi(params[2].get_str().c_str());
    //if ( params.size() == 3 || (basevolume= COIN * atof(params[3].get_str().c_str())) == 0 )
        basevolume = 100000;
    relvolume = safecoin_paxprice(&seed,height,(char *)base.c_str(),(char *)rel.c_str(),basevolume);
    ret.push_back(Pair("base", base));
    ret.push_back(Pair("rel", rel));
    ret.push_back(Pair("height", height));
    char seedstr[32];
    sprintf(seedstr,"%llu",(long long)seed);
    ret.push_back(Pair("seed", seedstr));
    if ( height < 0 || height > chainActive.Height() )
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    else
    {
        CBlockIndex *pblockindex = chainActive[height];
        if ( pblockindex != 0 )
            ret.push_back(Pair("timestamp", (int64_t)pblockindex->nTime));
        if ( basevolume != 0 && relvolume != 0 )
        {
            ret.push_back(Pair("price",((double)relvolume / (double)basevolume)));
            ret.push_back(Pair("invprice",((double)basevolume / (double)relvolume)));
            ret.push_back(Pair("basevolume",ValueFromAmount(basevolume)));
            ret.push_back(Pair("relvolume",ValueFromAmount(relvolume)));
        } else ret.push_back(Pair("error", "overflow or error in one or more of parameters"));
    }
    return ret;
}

UniValue paxprices(const UniValue& params, bool fHelp)
{
    if ( fHelp || params.size() != 3 )
        throw runtime_error("paxprices \"base\" \"rel\" maxsamples\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ); uint64_t relvolume,prices[4096]; uint32_t i,n; int32_t heights[sizeof(prices)/sizeof(*prices)];
    std::string base = params[0].get_str();
    std::string rel = params[1].get_str();
    int32_t maxsamples = atoi(params[2].get_str().c_str());
    if ( maxsamples < 1 )
        maxsamples = 1;
    else if ( maxsamples > sizeof(heights)/sizeof(*heights) )
        maxsamples = sizeof(heights)/sizeof(*heights);
    ret.push_back(Pair("base", base));
    ret.push_back(Pair("rel", rel));
    n = safecoin_paxprices(heights,prices,maxsamples,(char *)base.c_str(),(char *)rel.c_str());
    UniValue a(UniValue::VARR);
    for (i=0; i<n; i++)
    {
        UniValue item(UniValue::VOBJ);
        if ( heights[i] < 0 || heights[i] > chainActive.Height() )
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
        else
        {
            CBlockIndex *pblockindex = chainActive[heights[i]];

            item.push_back(Pair("t", (int64_t)pblockindex->nTime));
            item.push_back(Pair("p", (double)prices[i] / COIN));
            a.push_back(item);
        }
    }
    ret.push_back(Pair("array", a));
    return ret;
}

uint64_t safecoin_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue);

UniValue gettxout(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout \"txid\" n ( includemempool )\n"
            "\nReturns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id\n"
            "2. n              (numeric, required) vout value\n"
            "3. includemempool  (boolean, optional) Whether to included the mem pool\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in btc\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of bitcoin addresses\n"
            "        \"bitcoinaddress\"     (string) bitcoin address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,            (numeric) The version\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        );

    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    int n = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return NullUniValue;
        mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return NullUniValue;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
        return NullUniValue;

    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    CBlockIndex *pindex = it->second;
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.push_back(Pair("confirmations", 0));
    else ret.push_back(Pair("confirmations", pindex->nHeight - coins.nHeight + 1));
    ret.push_back(Pair("value", ValueFromAmount(coins.vout[n].nValue)));
    uint64_t interest; int32_t txheight; uint32_t locktime;
    if ( (interest= safecoin_accrued_interest(&txheight,&locktime,hash,n,coins.nHeight,coins.vout[n].nValue)) != 0 )
        ret.push_back(Pair("interest", ValueFromAmount(interest)));
    UniValue o(UniValue::VOBJ);
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("version", coins.nVersion));
    ret.push_back(Pair("coinbase", coins.fCoinBase));

    return ret;
}

UniValue verifychain(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "verifychain ( checklevel numblocks )\n"
            "\nVerifies blockchain database.\n"
            "\nArguments:\n"
            "1. checklevel   (numeric, optional, 0-4, default=3) How thorough the block verification is.\n"
            "2. numblocks    (numeric, optional, default=288, 0=all) The number of blocks to check.\n"
            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        );

    LOCK(cs_main);

    int nCheckLevel = GetArg("-checklevel", 3);
    int nCheckDepth = GetArg("-checkblocks", 288);
    if (params.size() > 0)
        nCheckLevel = params[0].get_int();
    if (params.size() > 1)
        nCheckDepth = params[1].get_int();

    return CVerifyDB().VerifyDB(pcoinsTip, nCheckLevel, nCheckDepth);
}

/** Implementation of IsSuperMajority with better feedback */
static UniValue SoftForkMajorityDesc(int minVersion, CBlockIndex* pindex, int nRequired, const Consensus::Params& consensusParams)
{
    int nFound = 0;
    CBlockIndex* pstart = pindex;
    for (int i = 0; i < consensusParams.nMajorityWindow && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }

    UniValue rv(UniValue::VOBJ);
    rv.push_back(Pair("status", nFound >= nRequired));
    rv.push_back(Pair("found", nFound));
    rv.push_back(Pair("required", nRequired));
    rv.push_back(Pair("window", consensusParams.nMajorityWindow));
    return rv;
}

static UniValue SoftForkDesc(const std::string &name, int version, CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv(UniValue::VOBJ);
    rv.push_back(Pair("id", name));
    rv.push_back(Pair("version", version));
    rv.push_back(Pair("enforce", SoftForkMajorityDesc(version, pindex, consensusParams.nMajorityEnforceBlockUpgrade, consensusParams)));
    rv.push_back(Pair("reject", SoftForkMajorityDesc(version, pindex, consensusParams.nMajorityRejectBlockOutdated, consensusParams)));
    return rv;
}

UniValue getblockchaininfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding block chain processing.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",        (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,         (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,        (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\", (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,     (numeric) the current difficulty\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"chainwork\": \"xxxx\"     (string) total amount of work in active chain, in hexadecimal\n"
            "  \"commitments\": xxxxxx,    (numeric) the current number of note commitments in the commitment tree\n"
            "  \"softforks\": [            (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",        (string) name of softfork\n"
            "        \"version\": xx,         (numeric) block version\n"
            "        \"enforce\": {           (object) progress toward enforcing the softfork rules for new-version blocks\n"
            "           \"status\": xx,       (boolean) true if threshold reached\n"
            "           \"found\": xx,        (numeric) number of blocks with the new version found\n"
            "           \"required\": xx,     (numeric) number of blocks required to trigger\n"
            "           \"window\": xx,       (numeric) maximum size of examined window of recent blocks\n"
            "        },\n"
            "        \"reject\": { ... }      (object) progress toward rejecting pre-softfork blocks (same fields as \"enforce\")\n"
            "     }, ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        );

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("chain",                 Params().NetworkIDString()));
    obj.push_back(Pair("blocks",                (int)chainActive.Height()));
    obj.push_back(Pair("headers",               pindexBestHeader ? pindexBestHeader->nHeight : -1));
    obj.push_back(Pair("bestblockhash",         chainActive.Tip()->GetBlockHash().GetHex()));
    obj.push_back(Pair("difficulty",            (double)GetNetworkDifficulty()));
    obj.push_back(Pair("verificationprogress",  Checkpoints::GuessVerificationProgress(Params().Checkpoints(), chainActive.Tip())));
    obj.push_back(Pair("chainwork",             chainActive.Tip()->nChainWork.GetHex()));
    obj.push_back(Pair("pruned",                fPruneMode));

    ZCIncrementalMerkleTree tree;
    pcoinsTip->GetAnchorAt(pcoinsTip->GetBestAnchor(), tree);
    #ifdef __APPLE__
    obj.push_back(Pair("commitments",           (uint64_t)tree.size()));
    #else
    obj.push_back(Pair("commitments",           tree.size()));
    #endif

    const Consensus::Params& consensusParams = Params().GetConsensus();
    CBlockIndex* tip = chainActive.Tip();
    UniValue softforks(UniValue::VARR);
    softforks.push_back(SoftForkDesc("bip34", 2, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip66", 3, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip65", 4, tip, consensusParams));
    obj.push_back(Pair("softforks",             softforks));

    if (fPruneMode)
    {
        CBlockIndex *block = chainActive.Tip();
        while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA))
            block = block->pprev;

        obj.push_back(Pair("pruneheight",        block->nHeight));
    }
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

UniValue getchaintips(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        );

    LOCK(cs_main);

    /* Build up a list of chain tips.  We start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
        setTips.insert(item.second);
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        const CBlockIndex* pprev=0;
        if ( item.second != 0 )
            pprev = item.second->pprev;
        if (pprev)
            setTips.erase(pprev);
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    UniValue res(UniValue::VARR); const CBlockIndex *forked;
    BOOST_FOREACH(const CBlockIndex* block, setTips)
    BOOST_FOREACH(const CBlockIndex* block, setTips)
        {
            UniValue obj(UniValue::VOBJ);
            obj.push_back(Pair("height", block->nHeight));
            obj.push_back(Pair("hash", block->phashBlock->GetHex()));
            forked = chainActive.FindFork(block);
            if ( forked != 0 )
            {
                const int branchLen = block->nHeight - forked->nHeight;
                obj.push_back(Pair("branchlen", branchLen));

                string status;
                if (chainActive.Contains(block)) {
                    // This block is part of the currently active chain.
                    status = "active";
                } else if (block->nStatus & BLOCK_FAILED_MASK) {
                    // This block or one of its ancestors is invalid.
                    status = "invalid";
                } else if (block->nChainTx == 0) {
                    // This block cannot be connected because full block data for it or one of its parents is missing.
                    status = "headers-only";
                } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
                    // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
                    status = "valid-fork";
                } else if (block->IsValid(BLOCK_VALID_TREE)) {
                    // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
                    status = "valid-headers";
                } else {
                    // No clue.
                    status = "unknown";
                }
                obj.push_back(Pair("status", status));
            }
            res.push_back(obj);
        }

    return res;
}

UniValue mempoolInfoToJSON()
{
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("size", (int64_t) mempool.size()));
    ret.push_back(Pair("bytes", (int64_t) mempool.GetTotalTxSize()));
    ret.push_back(Pair("usage", (int64_t) mempool.DynamicMemoryUsage()));

    return ret;
}

UniValue getmempoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx                (numeric) Current tx count\n"
            "  \"bytes\": xxxxx               (numeric) Sum of all tx sizes\n"
            "  \"usage\": xxxxx               (numeric) Total memory usage for the mempool\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        );

    return mempoolInfoToJSON();
}

UniValue invalidateblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock \"hash\"\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to mark as invalid\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        );

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        InvalidateBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue reconsiderblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock \"hash\"\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "This can be used to undo the effects of invalidateblock.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to reconsider\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
        );

    std::string strHash = params[0].get_str();
    uint256 hash(uint256S(strHash));
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        ReconsiderBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}
