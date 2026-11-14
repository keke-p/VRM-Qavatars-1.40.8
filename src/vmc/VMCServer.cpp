#include "VMC/VMCServer.hpp"

#include <algorithm>

#include "config/ConfigManager.hpp"
#include "VMC/util.hpp"

#include "main.hpp"

namespace VRMQavatars::VMC
{
    kissnet::udp_socket VMCServer::socket;
    std::vector<Tracker> VMCServer::availableTrackers;

    void VMCServer::handlePacket(const OSCPP::Server::Packet& packet)
    {
        if (packet.isBundle()) {
            const OSCPP::Server::Bundle bundle(packet);

            OSCPP::Server::PacketStream packets(bundle.packets());

            while (!packets.atEnd()) {
                handlePacket(packets.next());
            }
        } else {
            const OSCPP::Server::Message msg(packet);

            OSCPP::Server::ArgStream args(msg.args());

            /*VRMLogger.info("packet address {}", msg.address());

            if(msg == "/VMC/Ext/T")
            {
                VRMLogger.info("time! {}", args.float32());
            }

            if(msg == "/VMC/Ext/Ok")
            {
                int loaded = -1;
                int calibState = -1;
                int calibMode = -1;
                if(!args.atEnd())
                {
                    loaded = args.int32();
                    if(!args.atEnd())
                    {
                        calibState = args.int32();
                        if(!args.atEnd())
                        {
                            calibMode = args.int32();
                        }
                    }
                }
                VRMLogger.info("Ok! {}, {}, {}", loaded, calibState, calibMode);
            }

            if(msg == "/VMC/Ext/Root/Pos")
            {
                VRMLogger.info("Root Pos! {} {{} {} {}} {{} {} {} {}}", args.string(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32());
            }

            if(msg == "/VMC/Ext/Bone/Pos")
            {
                VRMLogger.info("Bone Pos! {} {{} {} {}} {{} {} {} {}}", args.string(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32());
            }

            if(msg == "/VMC/Ext/Con/Pos")
            {
                VRMLogger.info("Controller Pos! {} {{} {} {}} {{} {} {} {}}", args.string(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32());
            }

            if(msg == "/VMC/Ext/Hmd/Pos")
            {
                VRMLogger.info("Hmd Pos! {} {{} {} {}} {{} {} {} {}}", args.string(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32(), args.float32());
            }*/

            if(msg == "/VMC/Ext/Tra/Pos")
            {
                const std::string name = args.string();
                const UnityEngine::Vector3 pos = {args.float32(), args.float32(), args.float32()};
                UnityEngine::Quaternion rot = {args.float32(), args.float32(), args.float32(), args.float32()};

                // keep one entry per tracker, senders split them over several bundles or send them unbundled
                const auto existing = std::find_if(availableTrackers.begin(), availableTrackers.end(), [&name](const Tracker& tracker) { return tracker.name == name; });
                if(existing != availableTrackers.end())
                {
                    existing->pos = pos;
                    existing->rot = rot;
                }
                else
                {
                    availableTrackers.push_back({name, pos, rot});
                }
            }
        }
    }

    void VMCServer::InitServer()
    {
        availableTrackers.clear();
        // kissnet throws when e.g. the port is invalid or in use, and the port is typed in by the user
        try
        {
            if(socket)
            {
                socket.shutdown();
            }
            const auto config = Config::ConfigManager::GetVMCSettings();
            if(config.enableReceiver)
            {
                socket = kissnet::udp_socket(kissnet::endpoint("0.0.0.0", IntUtil::to_int(config.recvPort.c_str())));
                socket.bind();
                socket.listen();
            }
        }
        catch(const std::exception& e)
        {
            VRMLogger.error("Couldn't start the VMC receiver: {}", e.what());
            socket = kissnet::udp_socket();
        }
    }

    void VMCServer::Receive()
    {
        // enough for a sender running several times faster than the game
        constexpr int maxPacketsPerFrame = 32;

        const auto config = Config::ConfigManager::GetVMCSettings();
        if(!config.enableReceiver)
            return;

        // the packets come from the network, a malformed one makes oscpp throw
        try
        {
            if(socket.get_status() != kissnet::socket_status::valid)
                return;

            // read everything that arrived since the last frame, or the poses fall further and further behind
            for(int i = 0; i < maxPacketsPerFrame && socket.bytes_available() > 0; i++)
            {
                kissnet::buffer<8192> buff;
                auto [received_bytes, status] = socket.recv(buff);
                if(received_bytes == 0)
                    break;
                handlePacket(OSCPP::Server::Packet(buff.data(), received_bytes));
            }
        }
        catch(const std::exception& e)
        {
            static bool logged = false;
            if(!logged)
            {
                logged = true;
                VRMLogger.error("Couldn't read a VMC packet: {}", e.what());
            }
        }
    }
}
