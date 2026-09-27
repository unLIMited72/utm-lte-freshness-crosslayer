/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/*
 * Project modification notice: changed by the UTM study project.
 * Changes relative to baseline 4059af204636298c061df368e241941fa3ff97bf:
 * 2026-09-04T16:54:57+09:00 | 9187ea8c3323fff5df82c376b4292e604d8533a1 | Add behavior-neutral packet and RLC SDU lineage tracing
 * Dates above are verified Git revision timestamps, not inferred wall-clock edit times.
 * Public notice annotation added 2026-09-27; scientific code unchanged.
 * Upstream copyright, license and author notices remain intact.
 */


#ifndef LTE_RLC_SDU_LINEAGE_TAG_H
#define LTE_RLC_SDU_LINEAGE_TAG_H

#include "ns3/tag.h"

#include <cstdint>

namespace ns3
{

/**
 * Simulation-only metadata used to identify an application SDU at RLC ingress.
 * The tag is not serialized into the simulated packet byte buffer.
 */
class LteRlcSduLineageTag : public Tag
{
  public:
    enum TrafficClass : uint8_t
    {
        UNINSTRUMENTED = 0,
        STATUS = 1,
        BACKGROUND = 2,
        BURST = 3
    };

    LteRlcSduLineageTag();
    LteRlcSduLineageTag(uint64_t lineageId,
                        uint8_t trafficClass,
                        int64_t generationTimeNs,
                        int64_t applicationAttemptTimeNs);

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer i) const override;
    void Deserialize(TagBuffer i) override;
    void Print(std::ostream& os) const override;

    uint64_t GetLineageId() const;
    uint8_t GetTrafficClass() const;
    int64_t GetGenerationTimeNs() const;
    int64_t GetApplicationAttemptTimeNs() const;

  private:
    uint64_t m_lineageId{0};
    uint8_t m_trafficClass{UNINSTRUMENTED};
    int64_t m_generationTimeNs{0};
    int64_t m_applicationAttemptTimeNs{0};
};

} // namespace ns3

#endif // LTE_RLC_SDU_LINEAGE_TAG_H
