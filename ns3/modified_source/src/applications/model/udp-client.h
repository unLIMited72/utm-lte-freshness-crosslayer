/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 *
 */

/*
 * Project modification notice: changed by the UTM study project.
 * Changes relative to baseline 4059af204636298c061df368e241941fa3ff97bf:
 * 2026-09-02T18:42:55+09:00 | f168a27f5bcdee63eaf6534fd26a371cd912c907 | Instrument Stage 1 application packet events
 * Dates above are verified Git revision timestamps, not inferred wall-clock edit times.
 * Public notice annotation added 2026-09-27; scientific code unchanged.
 * Upstream copyright, license and author notices remain intact.
 */


#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "source-application.h"

#include "ns3/deprecated.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <optional>

namespace ns3
{

class Socket;
class Packet;

/**
 * @ingroup udpclientserver
 *
 * @brief A Udp client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class UdpClient : public SourceApplication
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    UdpClient();
    ~UdpClient() override;

    static constexpr uint16_t DEFAULT_PORT{100}; //!< default port

    /**
     * @brief set the remote address and port
     * @param ip remote IP address
     * @param port remote port
     */
    NS_DEPRECATED_3_44("Use SetRemote without port parameter instead")
    void SetRemote(const Address& ip, uint16_t port);
    void SetRemote(const Address& addr) override;

    /**
     * @return the total bytes sent by this app
     */
    uint64_t GetTotalTx() const;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * @brief Send a packet
     */
    void Send();

    /**
     * @brief Set the remote port (temporary function until deprecated attributes are removed)
     * @param port remote port
     */
    void SetPort(uint16_t port);

    /**
     * @brief Get the remote port (temporary function until deprecated attributes are removed)
     * @return the remote port
     */
    uint16_t GetPort() const;

    /**
     * @brief Get the remote address (temporary function until deprecated attributes are removed)
     * @return the remote address
     */
    Address GetRemote() const;

    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

    /// Callbacks for tracing the packet Tx events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;

    /// Successful packet Tx events after the SeqTsHeader has been added
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithSeqTs;

    uint32_t m_count; //!< Maximum number of packets the application will send
    Time m_interval;  //!< Packet inter-send time
    uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

    uint32_t m_sent;                    //!< Counter for sent packets
    uint64_t m_totalTx;                 //!< Total bytes sent
    Ptr<Socket> m_socket;               //!< Socket
    std::optional<uint16_t> m_peerPort; //!< Remote peer port (deprecated) // NS_DEPRECATED_3_44
    EventId m_sendEvent;                //!< Event to send the next packet

#ifdef NS3_LOG_ENABLE
    std::string m_peerString; //!< Remote peer address string
#endif                        // NS3_LOG_ENABLE
};

} // namespace ns3

#endif /* UDP_CLIENT_H */
