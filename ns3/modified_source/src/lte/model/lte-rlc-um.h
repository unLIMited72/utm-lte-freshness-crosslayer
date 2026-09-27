/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

/*
 * Project modification notice: changed by the UTM study project.
 * Changes relative to baseline 4059af204636298c061df368e241941fa3ff97bf:
 * 2026-09-02T18:56:33+09:00 | 2fad3c6b0a209edc52d019b7d75f5fcbe00a3947 | Add Stage 3 RLC queue and HOL tracing
 * 2026-09-04T16:54:57+09:00 | 9187ea8c3323fff5df82c376b4292e604d8533a1 | Add behavior-neutral packet and RLC SDU lineage tracing
 * 2026-09-04T17:01:51+09:00 | c6e2455e8977d0ea831f88a2a75d9569bbc3485e | Refactor RLC UM FIFO SDU selection boundary
 * 2026-09-04T17:12:30+09:00 | 812d1cd8097192bceb2416de9802b6aa5405ee0b | Implement non-dropping STATUS-priority RLC selection
 * 2026-09-04T17:35:56+09:00 | d118456d09b579cb30a98cf1a577d0046a24e640 | Distinguish handover RLC disposal from simulation censor
 * Dates above are verified Git revision timestamps, not inferred wall-clock edit times.
 * Public notice annotation added 2026-09-27; scientific code unchanged.
 * Upstream copyright, license and author notices remain intact.
 */


#ifndef LTE_RLC_UM_H
#define LTE_RLC_UM_H

#include "lte-rlc-sequence-number.h"
#include "lte-rlc.h"

#include "ns3/event-id.h"

#include <deque>
#include <map>

namespace ns3
{

/**
 * LTE RLC Unacknowledged Mode (UM), see 3GPP TS 36.322
 */
class LteRlcUm : public LteRlc
{
  public:
    enum QueueEventReason : uint8_t
    {
        QUEUE_ENQUEUE = 0,
        QUEUE_TX_OPPORTUNITY = 1,
        QUEUE_DROP_OVERFLOW = 2
    };

    enum SduLineageEvent : uint8_t
    {
        LINEAGE_ENQUEUE = 0,
        LINEAGE_SERVICE_FRAGMENT = 1,
        LINEAGE_FULLY_SERVED = 2,
        LINEAGE_DROP_OVERFLOW = 3,
        LINEAGE_CENSORED_SIM_END = 4,
        LINEAGE_CENSORED_RLC_DISPOSE = 5
    };

    enum SduLineageDisposition : uint8_t
    {
        DISPOSITION_NONE = 0,
        DISPOSITION_FULLY_SERVED = 1,
        DISPOSITION_DROP_OVERFLOW = 2,
        DISPOSITION_CENSORED_SIM_END = 3,
        DISPOSITION_CENSORED_RLC_DISPOSE = 4
    };

    enum TxSduSelectionPolicy : uint8_t
    {
        FIFO = 0,
        STATUS_PRIORITY_NON_DROPPING = 1
    };

    enum SduSelectionAction : uint8_t
    {
        SELECT_FIFO = 0,
        SELECT_STATUS_PRIORITY = 1,
        CONTINUE_PARTIAL_SDU = 2
    };

    LteRlcUm();
    ~LteRlcUm() override;
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    void DoDispose() override;

    /**
     * RLC SAP
     *
     * @param p packet
     */
    void DoTransmitPdcpPdu(Ptr<Packet> p) override;

    /**
     * MAC SAP
     *
     * @param txOpParams the LteMacSapUser::TxOpportunityParameters
     */
    void DoNotifyTxOpportunity(LteMacSapUser::TxOpportunityParameters txOpParams) override;
    void DoNotifyHarqDeliveryFailure() override;
    void DoReceivePdu(LteMacSapUser::ReceivePduParameters rxPduParams) override;

  private:
    void EmitQueueState(QueueEventReason reason, uint32_t affectedBytes);
    struct LineageMetadata;
    void EmitSduLineage(const LineageMetadata& metadata,
                        SduLineageEvent event,
                        uint64_t rlcPduId,
                        uint32_t segmentBytes,
                        SduLineageDisposition disposition);
    void RecordServiceFragment(LineageMetadata& metadata,
                               uint32_t segmentBytes,
                               uint64_t rlcPduId);
    /// Expire reordering timer
    void ExpireReorderingTimer();
    /// Expire RBS timer
    void ExpireRbsTimer();

    /**
     * Is inside reordering window function
     *
     * @param seqNumber the sequence number
     * @returns true if inside the window
     */
    bool IsInsideReorderingWindow(SequenceNumber10 seqNumber);

    /// Reassemble outside window
    void ReassembleOutsideWindow();
    /**
     * Reassemble SN interval function
     *
     * @param lowSeqNumber the low sequence number
     * @param highSeqNumber the high sequence number
     */
    void ReassembleSnInterval(SequenceNumber10 lowSeqNumber, SequenceNumber10 highSeqNumber);

    /**
     * Reassemble and deliver function
     *
     * @param packet the packet
     */
    void ReassembleAndDeliver(Ptr<Packet> packet);

    /// Report buffer status
    void DoReportBufferStatus();

  private:
    uint32_t m_maxTxBufferSize; ///< maximum transmit buffer status
    uint32_t m_txBufferSize;    ///< transmit buffer size

    TracedCallback<uint16_t, uint8_t, uint8_t, uint32_t, uint32_t, uint64_t, bool, uint32_t>
        m_queueStateTrace; ///< post-transition transmission queue state

    TracedCallback<uint16_t,
                   uint8_t,
                   uint8_t,
                   uint64_t,
                   uint8_t,
                   int64_t,
                   int64_t,
                   int64_t,
                   uint32_t,
                   uint32_t,
                   uint32_t,
                   uint32_t,
                   uint64_t,
                   uint8_t>
        m_sduLineageTrace; ///< constituent SDU lineage and service events

    struct LineageMetadata
    {
        bool instrumented{false};
        uint64_t lineageId{0};
        uint8_t trafficClass{0};
        int64_t generationTimeNs{0};
        int64_t applicationAttemptTimeNs{0};
        int64_t enqueueTimeNs{0};
        uint32_t originalSduBytes{0};
        uint32_t remainingSduBytes{0};
        uint32_t bytesServedTotal{0};
        bool hasStartedService{false};
    };

    /**
     * @brief Store an incoming (from layer above us) PDU, waiting to transmit it
     */
    struct TxPdu
    {
        /**
         * @brief TxPdu default constructor
         * @param pdu the PDU
         * @param time the arrival time
         */
        TxPdu(const Ptr<Packet>& pdu, const Time& time)
            : m_pdu(pdu),
              m_waitingSince(time),
              m_isPartial(false)
        {
        }

        TxPdu(const Ptr<Packet>& pdu,
              const Time& time,
              const LineageMetadata& lineage,
              bool isPartial = false)
            : m_pdu(pdu),
              m_waitingSince(time),
              m_lineage(lineage),
              m_isPartial(isPartial)
        {
        }

        TxPdu() = delete;

        Ptr<Packet> m_pdu;   ///< PDU
        Time m_waitingSince; ///< Layer arrival time
        LineageMetadata m_lineage; ///< application SDU identity and service accounting
        bool m_isPartial;          ///< true after a prior fragment has been served
    };

    std::deque<TxPdu>::iterator SelectNextSdu();
    void EmitSduSelection(const TxPdu& selected, SduSelectionAction action);

    std::deque<TxPdu> m_txBuffer;               ///< Transmission buffer
    std::map<uint16_t, Ptr<Packet>> m_rxBuffer; ///< Reception buffer
    std::vector<Ptr<Packet>> m_reasBuffer;      ///< Reassembling buffer

    std::list<Ptr<Packet>> m_sdusBuffer; ///< List of SDUs in a packet

    /**
     * State variables. See section 7.1 in TS 36.322
     */
    SequenceNumber10 m_sequenceNumber; ///< VT(US)
    uint64_t m_nextLineageRlcPduId{0}; ///< trace-only identifier for transmitted RLC PDUs
    TxSduSelectionPolicy m_txSduSelectionPolicy{FIFO}; ///< deterministic service-order policy

    TracedCallback<uint16_t,
                   uint8_t,
                   uint8_t,
                   uint8_t,
                   uint64_t,
                   uint8_t,
                   int64_t,
                   int64_t,
                   uint32_t,
                   uint32_t,
                   uint32_t,
                   uint32_t,
                   uint32_t,
                   uint32_t>
        m_sduSelectionTrace; ///< selected SDU and pre-selection queue composition

    SequenceNumber10 m_vrUr; ///< VR(UR)
    SequenceNumber10 m_vrUx; ///< VR(UX)
    SequenceNumber10 m_vrUh; ///< VR(UH)

    /**
     * Constants. See section 7.2 in TS 36.322
     */
    uint16_t m_windowSize; ///< windows size

    /**
     * Timers. See section 7.3 in TS 36.322
     */
    Time m_reorderingTimerValue;        ///< reordering timer value
    EventId m_reorderingTimer;          ///< reordering timer
    EventId m_rbsTimer;                 ///< RBS timer
    bool m_enablePdcpDiscarding{false}; //!< whether to use the PDCP discarding (perform discarding
                                        //!< at the moment of passing the PDCP SDU to RLC)
    uint32_t m_discardTimerMs{0};       //!< the discard timer value in milliseconds

    /**
     * Reassembling state
     */
    enum ReassemblingState_t
    {
        NONE = 0,
        WAITING_S0_FULL = 1,
        WAITING_SI_SF = 2
    };

    ReassemblingState_t m_reassemblingState; ///< reassembling state
    Ptr<Packet> m_keepS0;                    ///< keep S0

    /**
     * Expected Sequence Number
     */
    SequenceNumber10 m_expectedSeqNumber;
};

} // namespace ns3

#endif // LTE_RLC_UM_H
