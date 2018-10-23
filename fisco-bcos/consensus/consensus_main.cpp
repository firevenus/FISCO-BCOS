/**
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 *
 * @brief: empty framework for main of consensus
 *
 * @file: consensus_main.cpp
 * @author: chaychen
 * @date 2018-10-09
 */

#include <fisco-bcos/Fake.h>
#include <fisco-bcos/ParamParse.h>
#include <libconsensus/pbft/PBFTConsensus.h>
#include <libdevcore/CommonJS.h>
#include <libdevcore/easylog.h>
#include <libethcore/Protocol.h>
#include <libtxpool/TxPool.h>
static void startConsensus(Params& params)
{
    ///< initialize component
    auto p2pMsgHandler = std::make_shared<P2PMsgHandler>();

    std::shared_ptr<AsioInterface> asioInterface = std::make_shared<AsioInterface>();
    std::shared_ptr<NetworkConfig> netConfig = params.creatNetworkConfig();
    std::shared_ptr<SocketFactory> socketFactory = std::make_shared<SocketFactory>();
    std::shared_ptr<SessionFactory> sessionFactory = std::make_shared<SessionFactory>();
    std::shared_ptr<Host> host =
        std::make_shared<Host>(params.clientVersion(), CertificateServer::GetInstance().keypair(),
            *netConfig.get(), asioInterface, socketFactory, sessionFactory);

    host->setStaticNodes(params.staticNodes());
    /// set the topic id
    uint8_t group_id = 2;
    uint16_t protocol_id = getGroupProtoclID(group_id, ProtocolID::PBFT);

    std::shared_ptr<Service> p2pService = std::make_shared<Service>(host, p2pMsgHandler);
    std::shared_ptr<BlockChainInterface> blockChain = std::make_shared<FakeBlockChain>();
    std::shared_ptr<dev::txpool::TxPool> txPool =
        std::make_shared<dev::txpool::TxPool>(p2pService, blockChain, dev::eth::ProtocolID::TxPool);
    std::shared_ptr<SyncInterface> blockSync = std::make_shared<FakeBlockSync>();
    std::shared_ptr<BlockVerifierInterface> blockVerifier = std::make_shared<FakeBlockVerifier>();
    ///< Read the KeyPair of node from configuration file.
    auto nodePrivate = contents(getDataDir().string() + "/node.private");
    KeyPair key_pair;
    string pri = asString(nodePrivate);
    if (pri.size() >= 64)
    {
        key_pair = KeyPair(Secret(fromHex(pri.substr(0, 64))));
        LOG(INFO) << "Consensus Load KeyPair " << toPublic(key_pair.secret());
    }
    else
    {
        LOG(ERROR) << "Consensus Load KeyPair Fail! Please Check node.private File.";
        exit(-1);
    }
    ///< TODO: Read the minerList from the configuration file.
    h512s minerList = h512s();
    for (auto miner : params.minerList())
    {
        std::cout << "#### set miner:" << toHex(miner) << std::endl;
        minerList.push_back(miner);
    }
    /// minerList.push_back(toPublic(key_pair.secret()));
    ///< int pbft consensus
    std::cout << "### before create pbftEngine" << std::endl;
    std::shared_ptr<dev::consensus::ConsensusInterface> pbftEngine =
        std::make_shared<dev::consensus::PBFTEngine>(p2pService, txPool, blockChain, blockSync,
            blockVerifier, protocol_id, "./", key_pair, minerList);
    std::cout << "#### before create pbftConsensus" << std::endl;
    std::shared_ptr<dev::consensus::PBFTConsensus> pbftConsensus =
        std::make_shared<dev::consensus::PBFTConsensus>(txPool, blockChain, blockSync, pbftEngine);
    /// start the host
    host->start();
    std::cout << "#### protocol_id:" << protocol_id << std::endl;
    std::shared_ptr<std::vector<std::string>> topics = host->topics();
    topics->push_back(toString(group_id));
    std::cout << "#### before setTopic" << std::endl;
    host->setTopics(topics);
    std::cout << "##### set topic" << std::endl;
    ///< start consensus
    pbftConsensus->start();

    ///< transaction related
    bytes rlpBytes = fromHex(
        "f8aa8401be1a7d80830f4240941dc8def0867ea7e3626e03acee3eb40ee17251c880b84494e78a100000000000"
        "000000000000003ca576d469d7aa0244071d27eb33c5629753593e000000000000000000000000000000000000"
        "00000000000000000000000013881ba0f44a5ce4a1d1d6c2e4385a7985cdf804cb10a7fb892e9c08ff6d62657c"
        "4da01ea01d4c2af5ce505f574a320563ea9ea55003903ca5d22140155b3c2c968df0509464");
    Transaction tx(ref(rlpBytes), CheckTransaction::Everything);
    Secret sec = key_pair.secret();
    while (true)
    {
        tx.setNonce(tx.nonce() + u256(1));
        dev::Signature sig = sign(sec, tx.sha3(WithoutSignature));
        tx.updateSignature(SignatureStruct(sig));
        std::pair<h256, Address> ret = txPool->submit(tx);
        /// LOG(INFO) << "Import tx hash:" << dev::toJS(ret.first)
        ///          << ", size:" << txPool->pendingSize();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, const char* argv[])
{
    Params params = initCommandLine(argc, argv);

    startConsensus(params);
}