// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/sdr_auditor.hpp
// Role:          10-Rule Deterministic SDR Receiver Posture Auditor
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_SDR_AUDITOR_HPP
#define LH_SDR_AUDITOR_HPP

#include "sdr_types.hpp"
#include "json_parser.hpp"
#include <vector>

namespace logihertz {

class SdrAuditor {
public:
    /// Get the official 10 normative rule registry
    static const std::vector<RuleDefinition>& get_rules();

    /// Evaluate receiver policy configuration against all 10 rules
    static AuditReport audit_policy(const JsonValue& policy, const std::string& manifest_path = "");
};

} // namespace logihertz

#endif // LH_SDR_AUDITOR_HPP
