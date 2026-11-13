// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/exporters.hpp
// Role:          ANSI Table, Canonical JSON, and OASIS SARIF v2.1.0 Exporters
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_EXPORTERS_HPP
#define LH_EXPORTERS_HPP

#include "sdr_types.hpp"
#include <string>

namespace logihertz {

class Exporters {
public:
    /// Format report into ANSI terminal table with score banner
    static std::string format_table(const AuditReport& report);

    /// Format report into structured canonical JSON
    static std::string format_json(const AuditReport& report);

    /// Format report into OASIS SARIF v2.1.0 document
    static std::string format_sarif(const AuditReport& report);
};

} // namespace logihertz

#endif // LH_EXPORTERS_HPP
