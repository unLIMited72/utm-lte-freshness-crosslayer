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


#include "lte-rlc-sdu-lineage-tag.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(LteRlcSduLineageTag);

LteRlcSduLineageTag::LteRlcSduLineageTag() = default;

LteRlcSduLineageTag::LteRlcSduLineageTag(uint64_t lineageId,
                                         uint8_t trafficClass,
                                         int64_t generationTimeNs,
                                         int64_t applicationAttemptTimeNs)
    : m_lineageId(lineageId),
      m_trafficClass(trafficClass),
      m_generationTimeNs(generationTimeNs),
      m_applicationAttemptTimeNs(applicationAttemptTimeNs)
{
}

TypeId
LteRlcSduLineageTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::LteRlcSduLineageTag")
                            .SetParent<Tag>()
                            .SetGroupName("Lte")
                            .AddConstructor<LteRlcSduLineageTag>();
    return tid;
}

TypeId
LteRlcSduLineageTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
LteRlcSduLineageTag::GetSerializedSize() const
{
    return 25;
}

void
LteRlcSduLineageTag::Serialize(TagBuffer i) const
{
    i.WriteU64(m_lineageId);
    i.WriteU8(m_trafficClass);
    i.WriteU64(static_cast<uint64_t>(m_generationTimeNs));
    i.WriteU64(static_cast<uint64_t>(m_applicationAttemptTimeNs));
}

void
LteRlcSduLineageTag::Deserialize(TagBuffer i)
{
    m_lineageId = i.ReadU64();
    m_trafficClass = i.ReadU8();
    m_generationTimeNs = static_cast<int64_t>(i.ReadU64());
    m_applicationAttemptTimeNs = static_cast<int64_t>(i.ReadU64());
}

void
LteRlcSduLineageTag::Print(std::ostream& os) const
{
    os << "lineage=" << m_lineageId << ",class=" << static_cast<uint32_t>(m_trafficClass)
       << ",generationNs=" << m_generationTimeNs
       << ",attemptNs=" << m_applicationAttemptTimeNs;
}

uint64_t
LteRlcSduLineageTag::GetLineageId() const
{
    return m_lineageId;
}

uint8_t
LteRlcSduLineageTag::GetTrafficClass() const
{
    return m_trafficClass;
}

int64_t
LteRlcSduLineageTag::GetGenerationTimeNs() const
{
    return m_generationTimeNs;
}

int64_t
LteRlcSduLineageTag::GetApplicationAttemptTimeNs() const
{
    return m_applicationAttemptTimeNs;
}

} // namespace ns3
